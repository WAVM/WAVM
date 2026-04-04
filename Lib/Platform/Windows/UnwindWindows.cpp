#if WAVM_PLATFORM_WINDOWS

#include <cinttypes>
#include <cstring>
#include <optional>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/StringBuilder.h"
#include "WAVM/Platform/CPU.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Unwind.h"
#include "WindowsPrivate.h"

#include <basetsd.h>

using namespace WAVM;
using namespace WAVM::Platform;

#if defined(_M_ARM64)
inline DWORD64 getContextIP(const CONTEXT& ctx) { return ctx.Pc; }
#elif defined(_M_X64)
inline DWORD64 getContextIP(const CONTEXT& ctx) { return ctx.Rip; }
#endif

UnwindRegistration* Platform::registerUnwindData(const U8* imageBase,
												 Uptr imageNumPages,
												 const UnwindInfo& unwindInfo)
{
#ifdef _WIN64
	if(!unwindInfo.data) { return nullptr; }

	const U32 numFunctions = (U32)(unwindInfo.dataNumBytes / sizeof(RUNTIME_FUNCTION));

	// Register our manually fixed up copy of the function table.
	RUNTIME_FUNCTION* runtimeFunctions = (RUNTIME_FUNCTION*)unwindInfo.data;
	if(!RtlAddFunctionTable(runtimeFunctions, numFunctions, reinterpret_cast<ULONG_PTR>(imageBase)))
	{
		Errors::fatal("RtlAddFunctionTable failed");
	}
	return reinterpret_cast<UnwindRegistration*>(runtimeFunctions);
#else
	Errors::fatal("registerUnwindData isn't implemented on 32-bit Windows");
#endif
}

void Platform::deregisterUnwindData(UnwindRegistration* registration)
{
#ifdef _WIN64
	RtlDeleteFunctionTable(reinterpret_cast<RUNTIME_FUNCTION*>(registration));
#else
	Errors::fatal("deregisterUnwindData isn't implemented on 32-bit Windows");
#endif
}

//
// Platform-independent unwind API implementation
//

// The internal structure stored in UnwindState::storage
struct UnwindStateImpl
{
	CONTEXT context;
	DWORD64 imageBase;
	RUNTIME_FUNCTION* runtimeFunction;
	Iptr ipAdjustment;
	bool valid;
};

static_assert(sizeof(UnwindStateImpl) <= WAVM_PLATFORM_UNWIND_STATE_SIZE,
			  "UnwindStateImpl exceeds WAVM_PLATFORM_UNWIND_STATE_SIZE");
static_assert(alignof(UnwindStateImpl) <= WAVM_PLATFORM_UNWIND_STATE_ALIGN,
			  "UnwindStateImpl alignment exceeds WAVM_PLATFORM_UNWIND_STATE_ALIGN");

static UnwindStateImpl& getImpl(U8* storage)
{
	return *reinterpret_cast<UnwindStateImpl*>(storage);
}

static const UnwindStateImpl& getImpl(const U8* storage)
{
	return *reinterpret_cast<const UnwindStateImpl*>(storage);
}

UnwindState::UnwindState(const UnwindState& other) { getImpl(storage) = getImpl(other.storage); }

UnwindState::UnwindState(UnwindState&& other) noexcept
{
	getImpl(storage) = getImpl(other.storage);
}

UnwindState& UnwindState::operator=(const UnwindState& other)
{
	getImpl(storage) = getImpl(other.storage);
	return *this;
}

UnwindState& UnwindState::operator=(UnwindState&& other) noexcept
{
	getImpl(storage) = getImpl(other.storage);
	return *this;
}

static void setUnwindStateRuntimeFunctionAndImageBase(UnwindStateImpl& impl)
{
	// Look up the unwind info for the current position
	impl.runtimeFunction
		= RtlLookupFunctionEntry(getContextIP(impl.context), &impl.imageBase, nullptr);
}

// Must not be inlined: the step() call below skips capture() itself, so if capture() were inlined
// into the caller, that step would skip the caller's frame instead.
WAVM_FORCENOINLINE UnwindState UnwindState::capture(Uptr numFramesToSkip)
{
	UnwindState state;
	UnwindStateImpl& impl = getImpl(state.storage);
	RtlCaptureContext(&impl.context);
	setUnwindStateRuntimeFunctionAndImageBase(impl);
	impl.valid = true;

	// Step past capture() itself so frame 0 is the caller.
	if(state.step() != UnwindStepResult::success)
	{
		impl.valid = false;
		return state;
	}

	// Skip additional frames as requested.
	for(Uptr i = 0; i < numFramesToSkip; ++i)
	{
		if(state.step() != UnwindStepResult::success) { break; }
	}

	return state;
}

UnwindState Platform::makeUnwindStateFromSignalContext(void* contextPtr)
{
	UnwindState state;
	const CONTEXT& context = *static_cast<const CONTEXT*>(contextPtr);
	UnwindStateImpl& impl = getImpl(state.storage);
	memcpy(&impl.context, &context, sizeof(CONTEXT));
	setUnwindStateRuntimeFunctionAndImageBase(impl);
	impl.ipAdjustment = 0;
	impl.valid = true;
	return state;
}

UnwindStepResult UnwindState::step()
{
	UnwindStateImpl& impl = getImpl(storage);
	WAVM_ASSERT(impl.valid);

	if(getContextIP(impl.context) == 0) { return UnwindStepResult::end; }

	if(!impl.runtimeFunction)
	{
#if defined(_M_ARM64)
		impl.context.Pc = impl.context.Lr;
#elif defined(_M_X64)
		impl.context.Rip = *reinterpret_cast<DWORD64*>(impl.context.Rsp);
		impl.context.Rsp += 8;
#endif
	}
	else
	{
		// Use SEH unwind info
		void* handlerData;
		U64 establisherFrame;
		RtlVirtualUnwind(UNW_FLAG_NHANDLER,
						 impl.imageBase,
						 getContextIP(impl.context),
						 impl.runtimeFunction,
						 &impl.context,
						 &handlerData,
						 &establisherFrame,
						 nullptr);
	}

	if(getContextIP(impl.context) == 0) { return UnwindStepResult::end; }

	// Look up unwind info for new position
	setUnwindStateRuntimeFunctionAndImageBase(impl);

	// After stepping, all subsequent frames are return addresses that need -1 to get the
	// call-site address.
	impl.ipAdjustment = 1;

	return UnwindStepResult::success;
}

bool UnwindState::getProcInfo(UnwindProcInfo& outInfo) const
{
	const UnwindStateImpl& impl = getImpl(storage);
	WAVM_ASSERT(impl.valid);

	if(!impl.runtimeFunction) { return false; }

	outInfo.startIP = Uptr(impl.imageBase + impl.runtimeFunction->BeginAddress);

#if defined(_M_ARM64)
	// ARM64 RUNTIME_FUNCTION uses a union: Flag (bits [1:0]) distinguishes packed vs xdata.
	DWORD unwindData = impl.runtimeFunction->UnwindData;
	DWORD flag = unwindData & 0x3;

	if(flag != 0)
	{
		// Packed format: FunctionLength in bits [12:2], in 4-byte instruction units
		DWORD functionLength = (unwindData >> 2) & 0x7FF;
		outInfo.endIP = outInfo.startIP + functionLength * 4;
		outInfo.unwindData = nullptr;
		outInfo.handler = 0;
		outInfo.lsda = 0;
	}
	else
	{
		// UnwindData is an RVA to .xdata record
		const U8* xdata = reinterpret_cast<const U8*>(impl.imageBase + unwindData);
		DWORD header = *reinterpret_cast<const DWORD*>(xdata);

		DWORD functionLength = header & 0x3FFFF; // bits [17:0], in 4-byte units
		outInfo.endIP = outInfo.startIP + functionLength * 4;
		outInfo.unwindData = xdata;

		bool hasExceptionData = (header >> 20) & 1; // X bit
		bool packedEpilog = (header >> 21) & 1;     // E bit
		DWORD epilogCount = (header >> 22) & 0x1F;  // bits [26:22]
		DWORD codeWords = (header >> 27) & 0x1F;    // bits [31:27]

		const U8* ptr = xdata + 4;

		// Extended header when codeWords == 0 and E == 0
		if(codeWords == 0 && !packedEpilog)
		{
			DWORD extHeader = *reinterpret_cast<const DWORD*>(ptr);
			epilogCount = extHeader & 0xFFFF;
			codeWords = (extHeader >> 16) & 0xFF;
			ptr += 4;
		}

		// Skip epilog scopes (4 bytes each, only present when E == 0)
		if(!packedEpilog) { ptr += epilogCount * 4; }

		// Skip unwind codes
		ptr += codeWords * 4;

		if(hasExceptionData)
		{
			outInfo.handler = Uptr(impl.imageBase + *reinterpret_cast<const DWORD*>(ptr));
			outInfo.lsda = Uptr(ptr + 4);
		}
		else
		{
			outInfo.handler = 0;
			outInfo.lsda = 0;
		}
	}
#elif defined(_M_X64)
	outInfo.endIP = Uptr(impl.imageBase + impl.runtimeFunction->EndAddress);

	// Get UNWIND_INFO pointer
	const U8* unwindInfo
		= reinterpret_cast<const U8*>(impl.imageBase + impl.runtimeFunction->UnwindData);
	outInfo.unwindData = unwindInfo;

	// Parse UNWIND_INFO to find LSDA and handler
	// UNWIND_INFO structure:
	// Offset 0: Version:3, Flags:5
	// Offset 1: SizeOfProlog
	// Offset 2: CountOfUnwindCodes
	// Offset 3: FrameRegister:4, FrameOffset:4
	// Offset 4: UnwindCodes[CountOfUnwindCodes] (each 2 bytes)
	// After codes (aligned): ExceptionHandler (if UNW_FLAG_EHANDLER or UNW_FLAG_UHANDLER)
	// After handler: ExceptionData (LSDA)

	U8 flags = (unwindInfo[0] >> 3) & 0x1F;
	U8 countOfUnwindCodes = unwindInfo[2];

	// Round up to even number for alignment
	Uptr codesSize = ((countOfUnwindCodes + 1) & ~1) * 2;
	const U8* afterCodes = unwindInfo + 4 + codesSize;

	if(flags & (UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER))
	{
		outInfo.handler = Uptr(impl.imageBase + *reinterpret_cast<const U32*>(afterCodes));
		outInfo.lsda = Uptr(afterCodes + 4);
	}
	else
	{
		outInfo.handler = 0;
		outInfo.lsda = 0;
	}
#endif

	outInfo.unwindDataSize = 0;

	return true;
}

std::optional<Uptr> UnwindState::getRegister(HostCPURegister reg) const
{
	const UnwindStateImpl& impl = getImpl(storage);
	WAVM_ASSERT(impl.valid);

#if defined(_M_ARM64)
	switch(reg)
	{
	case AArch64CPURegister::X0: return Uptr(impl.context.X[0]);
	case AArch64CPURegister::X1: return Uptr(impl.context.X[1]);
	case AArch64CPURegister::X2: return Uptr(impl.context.X[2]);
	case AArch64CPURegister::X3: return Uptr(impl.context.X[3]);
	case AArch64CPURegister::X4: return Uptr(impl.context.X[4]);
	case AArch64CPURegister::X5: return Uptr(impl.context.X[5]);
	case AArch64CPURegister::X6: return Uptr(impl.context.X[6]);
	case AArch64CPURegister::X7: return Uptr(impl.context.X[7]);
	case AArch64CPURegister::X8: return Uptr(impl.context.X[8]);
	case AArch64CPURegister::X9: return Uptr(impl.context.X[9]);
	case AArch64CPURegister::X10: return Uptr(impl.context.X[10]);
	case AArch64CPURegister::X11: return Uptr(impl.context.X[11]);
	case AArch64CPURegister::X12: return Uptr(impl.context.X[12]);
	case AArch64CPURegister::X13: return Uptr(impl.context.X[13]);
	case AArch64CPURegister::X14: return Uptr(impl.context.X[14]);
	case AArch64CPURegister::X15: return Uptr(impl.context.X[15]);
	case AArch64CPURegister::X16: return Uptr(impl.context.X[16]);
	case AArch64CPURegister::X17: return Uptr(impl.context.X[17]);
	case AArch64CPURegister::X18: return Uptr(impl.context.X[18]);
	case AArch64CPURegister::X19: return Uptr(impl.context.X[19]);
	case AArch64CPURegister::X20: return Uptr(impl.context.X[20]);
	case AArch64CPURegister::X21: return Uptr(impl.context.X[21]);
	case AArch64CPURegister::X22: return Uptr(impl.context.X[22]);
	case AArch64CPURegister::X23: return Uptr(impl.context.X[23]);
	case AArch64CPURegister::X24: return Uptr(impl.context.X[24]);
	case AArch64CPURegister::X25: return Uptr(impl.context.X[25]);
	case AArch64CPURegister::X26: return Uptr(impl.context.X[26]);
	case AArch64CPURegister::X27: return Uptr(impl.context.X[27]);
	case AArch64CPURegister::X28: return Uptr(impl.context.X[28]);
	case AArch64CPURegister::FP: return Uptr(impl.context.Fp);
	case AArch64CPURegister::LR: return Uptr(impl.context.Lr);
	case AArch64CPURegister::SP: return Uptr(impl.context.Sp);
	case AArch64CPURegister::PC: return Uptr(impl.context.Pc) - impl.ipAdjustment;
	default: WAVM_UNREACHABLE();
	}
#elif defined(_M_X64)
	switch(reg)
	{
	case X86CPURegister::RAX: return Uptr(impl.context.Rax);
	case X86CPURegister::RBX: return Uptr(impl.context.Rbx);
	case X86CPURegister::RCX: return Uptr(impl.context.Rcx);
	case X86CPURegister::RDX: return Uptr(impl.context.Rdx);
	case X86CPURegister::RSI: return Uptr(impl.context.Rsi);
	case X86CPURegister::RDI: return Uptr(impl.context.Rdi);
	case X86CPURegister::RBP: return Uptr(impl.context.Rbp);
	case X86CPURegister::RSP: return Uptr(impl.context.Rsp);
	case X86CPURegister::R8: return Uptr(impl.context.R8);
	case X86CPURegister::R9: return Uptr(impl.context.R9);
	case X86CPURegister::R10: return Uptr(impl.context.R10);
	case X86CPURegister::R11: return Uptr(impl.context.R11);
	case X86CPURegister::R12: return Uptr(impl.context.R12);
	case X86CPURegister::R13: return Uptr(impl.context.R13);
	case X86CPURegister::R14: return Uptr(impl.context.R14);
	case X86CPURegister::R15: return Uptr(impl.context.R15);
	case X86CPURegister::RIP: return Uptr(impl.context.Rip) - impl.ipAdjustment;
	default: WAVM_UNREACHABLE();
	}
#endif

	return std::nullopt;
}

//
// UnwindOp iteration for Windows
//

#if defined(_M_ARM64)

// ARM64 unwind opcode types, decoded from xdata byte codes
enum ARM64UnwindOpType : U8
{
	ARM64_UOP_ALLOC_S = 0,
	ARM64_UOP_SAVE_R19R20_X,
	ARM64_UOP_SAVE_FPLR,
	ARM64_UOP_SAVE_FPLR_X,
	ARM64_UOP_ALLOC_M,
	ARM64_UOP_SAVE_REGP,
	ARM64_UOP_SAVE_REGP_X,
	ARM64_UOP_SAVE_REG,
	ARM64_UOP_SAVE_REG_X,
	ARM64_UOP_SAVE_LRPAIR,
	ARM64_UOP_SAVE_FREGP,
	ARM64_UOP_SAVE_FREGP_X,
	ARM64_UOP_SAVE_FREG,
	ARM64_UOP_SAVE_FREG_X,
	ARM64_UOP_ALLOC_L,
	ARM64_UOP_SET_FP,
	ARM64_UOP_ADD_FP,
	ARM64_UOP_NOP,
	ARM64_UOP_END,
	ARM64_UOP_END_C,
	ARM64_UOP_SAVE_NEXT,
	ARM64_UOP_PAC_SIGN_LR,
};

struct DecodedARM64Op
{
	U8 type;
	U32 reg;
	I64 operand;
};

// Returns the byte size of an ARM64 unwind code given its first byte.
static Uptr getARM64UnwindCodeSize(U8 b0)
{
	if(b0 <= 0xBF) { return 1; }
	if(b0 <= 0xDF) { return 2; }
	switch(b0)
	{
	case 0xE0: return 4; // alloc_l
	case 0xE2: return 2; // add_fp
	case 0xE7: return 3; // save_any_reg
	case 0xF8: return 2;
	case 0xF9: return 3;
	case 0xFA: return 4;
	case 0xFB: return 5;
	default: return 1;
	}
}

// Decode one ARM64 unwind code at the given byte offset.
// Returns the number of bytes consumed, or 0 on error.
static Uptr decodeARM64UnwindCode(const U8* codes, Uptr offset, Uptr codeBytes, DecodedARM64Op* out)
{
	if(offset >= codeBytes) { return 0; }
	U8 b0 = codes[offset];
	Uptr size = getARM64UnwindCodeSize(b0);
	if(offset + size > codeBytes) { return 0; }
	if(!out) { return size; }

	out->type = ARM64_UOP_NOP;
	out->reg = 0;
	out->operand = 0;

	U8 b1 = size >= 2 ? codes[offset + 1] : 0;

	// 1-byte opcodes: 0x00-0xBF
	if(b0 <= 0x1F)
	{
		out->type = ARM64_UOP_ALLOC_S;
		out->operand = (b0 & 0x1F) * 16;
	}
	else if(b0 <= 0x3F)
	{
		out->type = ARM64_UOP_SAVE_R19R20_X;
		out->reg = 19;
		out->operand = (b0 & 0x1F) * 8;
	}
	else if(b0 <= 0x7F)
	{
		out->type = ARM64_UOP_SAVE_FPLR;
		out->reg = 29;
		out->operand = (b0 & 0x3F) * 8;
	}
	else if(b0 <= 0xBF)
	{
		out->type = ARM64_UOP_SAVE_FPLR_X;
		out->reg = 29;
		out->operand = I64((b0 & 0x3F) + 1) * 8;
	}
	// 2-byte opcodes: 0xC0-0xDF
	else if(b0 <= 0xC7)
	{
		out->type = ARM64_UOP_ALLOC_M;
		out->operand = I64(((U32)(b0 & 0x07) << 8) | b1) * 16;
	}
	else if(b0 <= 0xCB)
	{
		U32 X = ((b0 & 0x03) << 2) | (b1 >> 6);
		out->type = ARM64_UOP_SAVE_REGP;
		out->reg = 19 + X;
		out->operand = (b1 & 0x3F) * 8;
	}
	else if(b0 <= 0xCF)
	{
		U32 X = ((b0 & 0x03) << 2) | (b1 >> 6);
		out->type = ARM64_UOP_SAVE_REGP_X;
		out->reg = 19 + X;
		out->operand = I64((b1 & 0x3F) + 1) * 8;
	}
	else if(b0 <= 0xD3)
	{
		U32 X = ((b0 & 0x03) << 2) | (b1 >> 6);
		out->type = ARM64_UOP_SAVE_REG;
		out->reg = 19 + X;
		out->operand = (b1 & 0x3F) * 8;
	}
	else if(b0 <= 0xD5)
	{
		U32 X = ((b0 & 0x01) << 3) | (b1 >> 5);
		out->type = ARM64_UOP_SAVE_REG_X;
		out->reg = 19 + X;
		out->operand = I64((b1 & 0x1F) + 1) * 8;
	}
	else if(b0 <= 0xD7)
	{
		U32 X = ((b0 & 0x01) << 2) | (b1 >> 6);
		out->type = ARM64_UOP_SAVE_LRPAIR;
		out->reg = 19 + 2 * X;
		out->operand = (b1 & 0x3F) * 8;
	}
	else if(b0 <= 0xD9)
	{
		U32 X = ((b0 & 0x01) << 2) | (b1 >> 6);
		out->type = ARM64_UOP_SAVE_FREGP;
		out->reg = 8 + X;
		out->operand = (b1 & 0x3F) * 8;
	}
	else if(b0 <= 0xDB)
	{
		U32 X = ((b0 & 0x01) << 2) | (b1 >> 6);
		out->type = ARM64_UOP_SAVE_FREGP_X;
		out->reg = 8 + X;
		out->operand = I64((b1 & 0x3F) + 1) * 8;
	}
	else if(b0 <= 0xDD)
	{
		U32 X = ((b0 & 0x01) << 2) | (b1 >> 6);
		out->type = ARM64_UOP_SAVE_FREG;
		out->reg = 8 + X;
		out->operand = (b1 & 0x3F) * 8;
	}
	else if(b0 == 0xDE)
	{
		out->type = ARM64_UOP_SAVE_FREG_X;
		out->reg = 8 + (b1 >> 5);
		out->operand = I64((b1 & 0x1F) + 1) * 8;
	}
	// Fixed opcodes 0xE0+
	else
	{
		switch(b0)
		{
		case 0xE0:
			out->type = ARM64_UOP_ALLOC_L;
			out->operand
				= I64(((U32)b1 << 16) | ((U32)codes[offset + 2] << 8) | codes[offset + 3]) * 16;
			break;
		case 0xE1:
			out->type = ARM64_UOP_SET_FP;
			out->reg = 29;
			break;
		case 0xE2:
			out->type = ARM64_UOP_ADD_FP;
			out->reg = 29;
			out->operand = b1 * 8;
			break;
		case 0xE3: out->type = ARM64_UOP_NOP; break;
		case 0xE4: out->type = ARM64_UOP_END; break;
		case 0xE5: out->type = ARM64_UOP_END_C; break;
		case 0xE6: out->type = ARM64_UOP_SAVE_NEXT; break;
		case 0xFC:
			out->type = ARM64_UOP_PAC_SIGN_LR;
			out->reg = 30;
			break;
		default: break;
		}
	}

	return size;
}

Uptr UnwindProcInfo::decodeUnwindOps(UnwindOp* outOps, Uptr maxOps) const
{
	if(!unwindData) { return 0; }

	// Re-parse xdata header to locate the unwind code bytes.
	// Same header format as getProcInfo, but here we need the code stream.
	DWORD header = *reinterpret_cast<const DWORD*>(unwindData);
	bool packedEpilog = (header >> 21) & 1;
	DWORD epilogCount = (header >> 22) & 0x1F;
	DWORD codeWords = (header >> 27) & 0x1F;

	const U8* ptr = unwindData + 4;
	if(codeWords == 0 && !packedEpilog)
	{
		DWORD extHeader = *reinterpret_cast<const DWORD*>(ptr);
		epilogCount = extHeader & 0xFFFF;
		codeWords = (extHeader >> 16) & 0xFF;
		ptr += 4;
	}
	if(!packedEpilog) { ptr += epilogCount * 4; }

	const U8* codes = ptr;
	Uptr codeBytes = codeWords * 4;

	// Count prolog ops and check for end/end_c terminator.
	Uptr numPrologOps = 0;
	bool hasEnd = false;
	Uptr byteOffset = 0;
	while(byteOffset < codeBytes)
	{
		U8 b0 = codes[byteOffset];
		Uptr codeSize = getARM64UnwindCodeSize(b0);
		if(byteOffset + codeSize > codeBytes) { break; }
		if(b0 == 0xE4 || b0 == 0xE5)
		{
			hasEnd = true;
			break;
		}
		++numPrologOps;
		byteOffset += codeSize;
	}
	Uptr numOps = numPrologOps + (hasEnd ? 1 : 0);

	if(!outOps) { return numOps; }

	// ARM64 xdata stores prolog codes in descending offset order (last prolog
	// instruction first). Write prolog codes to reversed positions; redirect
	// the end/end_c terminator to the end of the output array.
	Uptr prologToWrite = (numPrologOps < maxOps) ? numPrologOps : maxOps;
	Uptr writeIndex = prologToWrite;
	byteOffset = 0;
	while(byteOffset < codeBytes)
	{
		DecodedARM64Op decoded;
		Uptr consumed = decodeARM64UnwindCode(codes, byteOffset, codeBytes, &decoded);
		if(consumed == 0) { break; }

		bool isEnd = codes[byteOffset] == 0xE4 || codes[byteOffset] == 0xE5;
		Uptr dest = isEnd ? prologToWrite : --writeIndex;
		if(dest >= maxOps) { break; }

		outOps[dest].codeOffset = dest * 4;
		outOps[dest].opcode = decoded.type;
		outOps[dest].reg = decoded.reg;
		outOps[dest].operand = decoded.operand;

		byteOffset += consumed;
		if(isEnd) { break; }
	}

	return numOps;
}

void UnwindOp::format(StringBuilderBase& sb) const
{
	switch(opcode)
	{
	case ARM64_UOP_ALLOC_S:
		sb.appendf("@%" WAVM_PRIuPTR ": alloc_s #%" PRId64, codeOffset, operand);
		break;
	case ARM64_UOP_SAVE_R19R20_X:
		sb.appendf("@%" WAVM_PRIuPTR ": save_r19r20_x [sp, #-%" PRId64 "]!", codeOffset, operand);
		break;
	case ARM64_UOP_SAVE_FPLR:
		sb.appendf("@%" WAVM_PRIuPTR ": save_fplr [sp, #%" PRId64 "]", codeOffset, operand);
		break;
	case ARM64_UOP_SAVE_FPLR_X:
		sb.appendf("@%" WAVM_PRIuPTR ": save_fplr_x [sp, #-%" PRId64 "]!", codeOffset, operand);
		break;
	case ARM64_UOP_ALLOC_M:
		sb.appendf("@%" WAVM_PRIuPTR ": alloc_m #%" PRId64, codeOffset, operand);
		break;
	case ARM64_UOP_SAVE_REGP:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": save_regp x%u [sp, #%" PRId64 "]", codeOffset, reg, operand);
		break;
	case ARM64_UOP_SAVE_REGP_X:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": save_regp_x x%u [sp, #-%" PRId64 "]!", codeOffset, reg, operand);
		break;
	case ARM64_UOP_SAVE_REG:
		sb.appendf("@%" WAVM_PRIuPTR ": save_reg x%u [sp, #%" PRId64 "]", codeOffset, reg, operand);
		break;
	case ARM64_UOP_SAVE_REG_X:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": save_reg_x x%u [sp, #-%" PRId64 "]!", codeOffset, reg, operand);
		break;
	case ARM64_UOP_SAVE_LRPAIR:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": save_lrpair x%u, lr [sp, #%" PRId64 "]", codeOffset, reg, operand);
		break;
	case ARM64_UOP_SAVE_FREGP:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": save_fregp d%u [sp, #%" PRId64 "]", codeOffset, reg, operand);
		break;
	case ARM64_UOP_SAVE_FREGP_X:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": save_fregp_x d%u [sp, #-%" PRId64 "]!", codeOffset, reg, operand);
		break;
	case ARM64_UOP_SAVE_FREG:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": save_freg d%u [sp, #%" PRId64 "]", codeOffset, reg, operand);
		break;
	case ARM64_UOP_SAVE_FREG_X:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": save_freg_x d%u [sp, #-%" PRId64 "]!", codeOffset, reg, operand);
		break;
	case ARM64_UOP_ALLOC_L:
		sb.appendf("@%" WAVM_PRIuPTR ": alloc_l #%" PRId64, codeOffset, operand);
		break;
	case ARM64_UOP_SET_FP: sb.appendf("@%" WAVM_PRIuPTR ": set_fp", codeOffset); break;
	case ARM64_UOP_ADD_FP:
		sb.appendf("@%" WAVM_PRIuPTR ": add_fp #%" PRId64, codeOffset, operand);
		break;
	case ARM64_UOP_NOP: sb.appendf("@%" WAVM_PRIuPTR ": nop", codeOffset); break;
	case ARM64_UOP_END: sb.appendf("@%" WAVM_PRIuPTR ": end", codeOffset); break;
	case ARM64_UOP_END_C: sb.appendf("@%" WAVM_PRIuPTR ": end_c", codeOffset); break;
	case ARM64_UOP_SAVE_NEXT: sb.appendf("@%" WAVM_PRIuPTR ": save_next", codeOffset); break;
	case ARM64_UOP_PAC_SIGN_LR: sb.appendf("@%" WAVM_PRIuPTR ": pac_sign_lr", codeOffset); break;
	default: sb.appendf("@%" WAVM_PRIuPTR ": unknown (opcode 0x%02x)", codeOffset, opcode); break;
	}
}

#elif defined(_M_X64)

// x86-64 UNWIND_CODE operation types
enum WindowsUnwindOpType
{
	UWOP_PUSH_NONVOL = 0,
	UWOP_ALLOC_LARGE = 1,
	UWOP_ALLOC_SMALL = 2,
	UWOP_SET_FPREG = 3,
	UWOP_SAVE_NONVOL = 4,
	UWOP_SAVE_NONVOL_FAR = 5,
	UWOP_EPILOG = 6,
	UWOP_SPARE_CODE = 7,
	UWOP_SAVE_XMM128 = 8,
	UWOP_SAVE_XMM128_FAR = 9,
	UWOP_PUSH_MACHFRAME = 10
};

struct DecodedSlot
{
	U8 codeOffset;
	U8 opcode;
	U8 reg;
	Uptr offset;
};

// Decode an unwind op at the given slot index, returning the number of slots consumed.
// If outSlot is non-null, fills it with the decoded values.
static Uptr decodeUnwindOpAtSlot(const U8* codes, Uptr slotIndex, DecodedSlot* outSlot)
{
	U8 codeOffset = codes[slotIndex * 2];
	U8 opInfo = codes[slotIndex * 2 + 1];
	U8 opType = opInfo & 0x0F;
	U8 opReg = opInfo >> 4;

	Uptr slotsConsumed = 1;
	Uptr offset = 0;

	switch(opType)
	{
	case UWOP_PUSH_NONVOL: offset = 8; break;
	case UWOP_ALLOC_LARGE:
		if(opReg == 0)
		{
			offset = *reinterpret_cast<const U16*>(&codes[(slotIndex + 1) * 2]) * 8;
			slotsConsumed = 2;
		}
		else
		{
			offset = *reinterpret_cast<const U32*>(&codes[(slotIndex + 1) * 2]);
			slotsConsumed = 3;
		}
		break;
	case UWOP_ALLOC_SMALL: offset = (opReg + 1) * 8; break;
	case UWOP_SET_FPREG: break;
	case UWOP_SAVE_NONVOL:
		offset = *reinterpret_cast<const U16*>(&codes[(slotIndex + 1) * 2]) * 8;
		slotsConsumed = 2;
		break;
	case UWOP_SAVE_NONVOL_FAR:
		offset = *reinterpret_cast<const U32*>(&codes[(slotIndex + 1) * 2]);
		slotsConsumed = 3;
		break;
	case UWOP_SAVE_XMM128:
		offset = *reinterpret_cast<const U16*>(&codes[(slotIndex + 1) * 2]) * 16;
		slotsConsumed = 2;
		break;
	case UWOP_SAVE_XMM128_FAR:
		offset = *reinterpret_cast<const U32*>(&codes[(slotIndex + 1) * 2]);
		slotsConsumed = 3;
		break;
	case UWOP_PUSH_MACHFRAME: break;
	default: break;
	}

	if(outSlot)
	{
		outSlot->codeOffset = codeOffset;
		outSlot->opcode = opType;
		outSlot->reg = opReg;
		outSlot->offset = offset;
	}

	return slotsConsumed;
}

Uptr UnwindProcInfo::decodeUnwindOps(UnwindOp* outOps, Uptr maxOps) const
{
	if(!unwindData) { return 0; }

	Uptr slotCount = unwindData[2]; // CountOfUnwindCodes
	const U8* codes = unwindData + 4;

	// First pass: count ops
	Uptr numOps = 0;
	Uptr slotIndex = 0;
	while(slotIndex < slotCount)
	{
		++numOps;
		slotIndex += decodeUnwindOpAtSlot(codes, slotIndex, nullptr);
	}

	if(!outOps) { return numOps; }

	// Second pass: decode ops in reverse order (Windows stores descending, we want ascending)
	Uptr writeIndex = (numOps < maxOps) ? numOps : maxOps;
	slotIndex = 0;
	while(slotIndex < slotCount && writeIndex > 0)
	{
		--writeIndex;
		DecodedSlot slot;
		slotIndex += decodeUnwindOpAtSlot(codes, slotIndex, &slot);
		outOps[writeIndex].codeOffset = slot.codeOffset;
		outOps[writeIndex].opcode = slot.opcode;
		outOps[writeIndex].reg = slot.reg;
		outOps[writeIndex].operand = I64(slot.offset);
	}

	return numOps;
}

static const char* getWindowsRegisterName(U32 reg)
{
	static const char* names[] = {"RAX",
								  "RCX",
								  "RDX",
								  "RBX",
								  "RSP",
								  "RBP",
								  "RSI",
								  "RDI",
								  "R8",
								  "R9",
								  "R10",
								  "R11",
								  "R12",
								  "R13",
								  "R14",
								  "R15"};
	if(reg < 16) { return names[reg]; }
	return "???";
}

void UnwindOp::format(StringBuilderBase& sb) const
{
	const char* regName = getWindowsRegisterName(reg);
	switch(opcode)
	{
	case UWOP_PUSH_NONVOL:
		sb.appendf("@%" WAVM_PRIuPTR ": PUSH_NONVOL %s", codeOffset, regName);
		break;
	case UWOP_ALLOC_SMALL:
		sb.appendf("@%" WAVM_PRIuPTR ": ALLOC_SMALL %" PRId64, codeOffset, operand);
		break;
	case UWOP_ALLOC_LARGE:
		sb.appendf("@%" WAVM_PRIuPTR ": ALLOC_LARGE %" PRId64, codeOffset, operand);
		break;
	case UWOP_SET_FPREG: sb.appendf("@%" WAVM_PRIuPTR ": SET_FPREG %s", codeOffset, regName); break;
	case UWOP_SAVE_NONVOL:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": SAVE_NONVOL %s at RSP+%" PRId64, codeOffset, regName, operand);
		break;
	case UWOP_SAVE_NONVOL_FAR:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": SAVE_NONVOL_FAR %s at RSP+%" PRId64, codeOffset, regName, operand);
		break;
	case UWOP_SAVE_XMM128:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": SAVE_XMM128 XMM%u at RSP+%" PRId64, codeOffset, reg, operand);
		break;
	case UWOP_SAVE_XMM128_FAR:
		sb.appendf(
			"@%" WAVM_PRIuPTR ": SAVE_XMM128_FAR XMM%u at RSP+%" PRId64, codeOffset, reg, operand);
		break;
	case UWOP_PUSH_MACHFRAME: sb.appendf("@%" WAVM_PRIuPTR ": PUSH_MACHFRAME", codeOffset); break;
	default: sb.appendf("@%" WAVM_PRIuPTR ": unknown (opcode 0x%02x)", codeOffset, opcode); break;
	}
}

#endif // _M_X64

#endif // WAVM_PLATFORM_WINDOWS
