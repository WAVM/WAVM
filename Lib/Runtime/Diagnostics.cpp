#include "WAVM/Platform/Diagnostics.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Inline/StringBuilder.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Alloca.h"
#include "WAVM/Platform/CPU.h"
#include "WAVM/Platform/Unwind.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

using namespace WAVM;
using namespace WAVM::Platform;
using namespace WAVM::Runtime;

// Pre-created host disassembler for signal-safe use in trace-unwind path.
static std::shared_ptr<LLVMJIT::Disassembler> hostDisassembler;

void Runtime::appendToString(StringBuilderBase& stringBuilder, const InstructionSource& source)
{
	switch(source.type)
	{
	case InstructionSource::Type::unknown: stringBuilder.append("<unknown>"); break;
	case InstructionSource::Type::native:
		stringBuilder.append("host!", 5);
		appendToString(stringBuilder, source.native);
		break;
	case InstructionSource::Type::wasm:
		stringBuilder.appendf("%s+%" WAVM_PRIuPTR,
							  source.wasm.function->mutableData->debugName.c_str(),
							  source.wasm.instructionIndex);
		break;
	default: WAVM_UNREACHABLE();
	}
}

Uptr Runtime::getInstructionSourceByAddress(Uptr ip, InstructionSource* outSources, Uptr maxSources)
{
	if(maxSources == 0) { return 0; }

	// First try JIT'd WASM code
	LLVMJIT::InstructionSource jitSources[16];
	Uptr numJitSources = LLVMJIT::getInstructionSourceByAddress(ip, jitSources, 16);
	if(numJitSources > 0)
	{
		Uptr count = numJitSources < maxSources ? numJitSources : maxSources;
		for(Uptr i = 0; i < count; ++i)
		{
			outSources[i].type = InstructionSource::Type::wasm;
			outSources[i].wasm.function = jitSources[i].function;
			outSources[i].wasm.instructionIndex = jitSources[i].instructionIndex;
		}
		return count;
	}

	// Fall back to native code lookup
	if(Platform::getInstructionSourceByAddress(ip, outSources[0].native))
	{
		outSources[0].type = InstructionSource::Type::native;
		return 1;
	}

	return 0;
}

std::vector<Runtime::InstructionSource> Runtime::resolveCallStackFrames(const CallStack& callStack)
{
	std::vector<Runtime::InstructionSource> result;
	for(Uptr frameIndex = 0; frameIndex < callStack.frames.size(); ++frameIndex)
	{
		Runtime::InstructionSource sources[Runtime::maxInlineSourceFrames];
		Uptr numSources = getInstructionSourceByAddress(
			callStack.frames[frameIndex].ip, sources, Runtime::maxInlineSourceFrames);
		if(numSources == 0) { result.push_back(Runtime::InstructionSource()); }
		else
		{
			for(Uptr i = 0; i < numSources; ++i) { result.push_back(sources[i]); }
		}
	}
	return result;
}

std::vector<std::string> Runtime::describeCallStack(const CallStack& callStack)
{
	std::vector<std::string> frameDescriptions;
	HashSet<Uptr> describedIPs;
	Uptr frameIndex = 0;
	while(frameIndex < callStack.frames.size())
	{
		if(frameIndex + 1 < callStack.frames.size()
		   && describedIPs.contains(callStack.frames[frameIndex].ip)
		   && describedIPs.contains(callStack.frames[frameIndex + 1].ip))
		{
			Uptr numOmittedFrames = 2;
			while(frameIndex + numOmittedFrames < callStack.frames.size()
				  && describedIPs.contains(callStack.frames[frameIndex + numOmittedFrames].ip))
			{
				++numOmittedFrames;
			}

			frameDescriptions.emplace_back("<" + std::to_string(numOmittedFrames)
										   + " redundant frames omitted>");

			frameIndex += numOmittedFrames;
		}
		else
		{
			const Uptr frameIP = callStack.frames[frameIndex].ip;

			InstructionSource sources[Runtime::maxInlineSourceFrames];
			Uptr numSources
				= getInstructionSourceByAddress(frameIP, sources, Runtime::maxInlineSourceFrames);
			if(numSources == 0) { frameDescriptions.push_back("<unknown function>"); }
			else
			{
				for(Uptr i = 0; i < numSources; ++i)
				{
					StringBuilder stringBuilder;
					appendToString(stringBuilder, sources[i]);
					frameDescriptions.push_back(stringBuilder.moveToString());
				}
			}

			describedIPs.add(frameIP);

			++frameIndex;
		}
	}
	return frameDescriptions;
}

// Disassemble a function and interleave CFI ops at the appropriate offsets.
// Limits output to a window around the current IP.
static void disassembleWithCFI(const std::shared_ptr<LLVMJIT::Disassembler>& disasm,
							   const UnwindProcInfo& procInfo,
							   Uptr currentIP)
{
	constexpr Uptr maxBytesBeforeIP = 32;
	constexpr Uptr maxBytesAfterIP = 32;

	const U8* codeStart = reinterpret_cast<const U8*>(procInfo.startIP);
	Uptr codeSize = procInfo.endIP - procInfo.startIP;

	Uptr currentIPOffset = currentIP > procInfo.startIP ? currentIP - procInfo.startIP : 0;
	Uptr printStartOffset
		= currentIPOffset > maxBytesBeforeIP ? currentIPOffset - maxBytesBeforeIP : 0;
	Uptr endOffset = std::min(codeSize, currentIPOffset + maxBytesAfterIP);

	// Decode unwind ops into a stack-allocated buffer
	Uptr numUnwindOps = procInfo.decodeUnwindOps(nullptr, 0);
	UnwindOp* unwindOps = numUnwindOps > 0
							  ? static_cast<UnwindOp*>(alloca(numUnwindOps * sizeof(UnwindOp)))
							  : nullptr;
	if(unwindOps) { procInfo.decodeUnwindOps(unwindOps, numUnwindOps); }

	Uptr unwindOpIndex = 0;

	// Skip to the first instruction boundary at or after printStartOffset.
	Uptr codeOffset
		= LLVMJIT::findInstructionBoundary(disasm, codeStart, codeSize, printStartOffset);
	while(unwindOpIndex < numUnwindOps && unwindOps[unwindOpIndex].codeOffset < codeOffset)
	{
		++unwindOpIndex;
	}

	if(codeOffset > 0) { Log::printf(Log::traceUnwind, "               ...\n"); }

	while(codeOffset < endOffset)
	{
		// Print any CFI ops that apply at this code offset
		while(unwindOpIndex < numUnwindOps && unwindOps[unwindOpIndex].codeOffset <= codeOffset)
		{
			TruncatingFixedStringBuilder<256> opBuf;
			unwindOps[unwindOpIndex].format(opBuf);
			Log::printf(Log::traceUnwind, "           ; %s\n", opBuf.c_str());
			++unwindOpIndex;
		}

		// Get the absolute address for this instruction
		Uptr instrAddr = procInfo.startIP + codeOffset;

		// Disassemble the instruction
		LLVMJIT::DisassembledInstruction instr = LLVMJIT::disassembleInstruction(
			disasm, codeStart + codeOffset, codeSize - codeOffset);

		// Mark current IP with an arrow
		const char* marker
			= (instrAddr <= currentIP && currentIP < instrAddr + instr.numBytes) ? ">>>" : "   ";

		if(instr.numBytes == 0)
		{
			// Disassembly failed, print raw byte and advance
			Log::printf(Log::traceUnwind,
						"           %s 0x%" WAVM_PRIxPTR ": .byte 0x%02x\n",
						marker,
						instrAddr,
						codeStart[codeOffset]);
			codeOffset += 1;
		}
		else
		{
			Log::printf(Log::traceUnwind,
						"           %s 0x%" WAVM_PRIxPTR ": %s\n",
						marker,
						instrAddr,
						instr.mnemonic);
			codeOffset += instr.numBytes;
		}
	}

	if(endOffset < codeSize) { Log::printf(Log::traceUnwind, "               ...\n"); }
}

static void traceFrame(const std::shared_ptr<LLVMJIT::Disassembler>& disasm,
					   UnwindState& state,
					   Uptr frameIndex,
					   Uptr ip)
{
	auto optSP = state.getRegister(HostCPURegister::SP);
	WAVM_ERROR_UNLESS(optSP.has_value());
	Uptr sp = *optSP;

	// Get instruction source using signal-safe API (handles both WASM and native).
	Runtime::InstructionSource sources[Runtime::maxInlineSourceFrames];
	Uptr numSources
		= Runtime::getInstructionSourceByAddress(ip, sources, Runtime::maxInlineSourceFrames);

	// Format source string using a stack buffer (no heap allocation).
	TruncatingFixedStringBuilder<512> stringBuilder;
	if(numSources > 0)
	{
		for(Uptr i = 0; i < numSources; ++i)
		{
			if(i > 0) { stringBuilder.append(" <- ", 4); }
			appendToString(stringBuilder, sources[i]);
		}
	}
	else
	{
		stringBuilder.append("<unknown>");
	}

	Log::printf(Log::traceUnwind,
				"  Frame %" WAVM_PRIuPTR ": IP=0x%" WAVM_PRIxPTR " SP=0x%" WAVM_PRIxPTR " %s\n",
				frameIndex,
				ip,
				sp,
				stringBuilder.c_str());

	// Try to get procedure info
	UnwindProcInfo procInfo;
	if(state.getProcInfo(procInfo))
	{
		Log::printf(Log::traceUnwind,
					"           start=0x%" WAVM_PRIxPTR " end=0x%" WAVM_PRIxPTR
					" lsda=0x%" WAVM_PRIxPTR " handler=0x%" WAVM_PRIxPTR "\n",
					procInfo.startIP,
					procInfo.endIP,
					procInfo.lsda,
					procInfo.handler);

		// Disassemble with interleaved CFI ops
		disassembleWithCFI(disasm, procInfo, ip);
	}

	// Print all registers at this frame
	Log::printf(Log::traceUnwind, "           Registers:\n");
#define WAVM_TRACE_REGISTER_(enumName, displayName)                                                \
	{                                                                                              \
		auto value = state.getRegister(HostCPURegister::enumName);                                 \
		if(value.has_value())                                                                      \
		{                                                                                          \
			Log::printf(Log::traceUnwind,                                                          \
						"               %-3s=%016" WAVM_PRIxPTR "\n",                              \
						displayName,                                                               \
						*value);                                                                   \
		}                                                                                          \
	}
	WAVM_ENUM_HOST_CPU_REGISTERS(WAVM_TRACE_REGISTER_)
#undef WAVM_TRACE_REGISTER_
}

void Runtime::initTraceUnwindState()
{
	if(!hostDisassembler)
	{
		hostDisassembler = LLVMJIT::createDisassembler(LLVMJIT::getHostTargetSpec());
	}
}

CallStack Runtime::unwindCallStack(UnwindState& state)
{
	CallStack result;

	constexpr Uptr maxUnwindSteps = 50;

	if(Log::isCategoryEnabled(Log::traceUnwind) && hostDisassembler)
	{
		Log::printf(Log::traceUnwind, "=== Unwinding call stack ===\n");

		for(Uptr frameIndex = 0; frameIndex < maxUnwindSteps; ++frameIndex)
		{
			Uptr ip = *state.getRegister(HostCPURegister::IP);

			traceFrame(hostDisassembler, state, frameIndex, ip);

			if(!result.frames.isFull()) { result.frames.push_back(CallStack::Frame{ip}); }

			UnwindStepResult stepResult = state.step();
			if(stepResult == UnwindStepResult::end)
			{
				Log::printf(Log::traceUnwind, "  End of stack\n");
				break;
			}
			else if(stepResult == UnwindStepResult::error)
			{
				Log::printf(Log::traceUnwind, "  Unwind step failed\n");
				break;
			}
		}

		Log::printf(Log::traceUnwind, "=== End call stack ===\n");
	}
	else
	{
		for(Uptr frameIndex = 0; frameIndex < maxUnwindSteps; ++frameIndex)
		{
			Uptr ip = *state.getRegister(HostCPURegister::IP);

			if(!result.frames.isFull()) { result.frames.push_back(CallStack::Frame{ip}); }

			if(state.step() != UnwindStepResult::success) { break; }
		}
	}

	return result;
}
