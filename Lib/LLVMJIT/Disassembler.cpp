#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Platform/Mutex.h"

#include "LLVMJITPrivate.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm-c/Disassembler.h>
#include <llvm-c/DisassemblerTypes.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/Error.h>
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

using namespace WAVM;
using namespace WAVM::LLVMJIT;

struct LLVMJIT::Disassembler
{
	LLVMDisasmContextRef context;
	TargetArch arch;
	Platform::Mutex mutex;

	Disassembler(LLVMDisasmContextRef inContext, TargetArch inArch)
	: context(inContext), arch(inArch)
	{
	}
	~Disassembler()
	{
		if(context) { LLVMDisasmDispose(context); }
	}

	Disassembler(const Disassembler&) = delete;
	Disassembler& operator=(const Disassembler&) = delete;
};

std::shared_ptr<LLVMJIT::Disassembler> LLVMJIT::createDisassembler(const TargetSpec& targetSpec)
{
	initLLVM();
	std::string tripleStr = getTriple(targetSpec);
	LLVMDisasmContextRef context
		= LLVMCreateDisasm(tripleStr.c_str(), nullptr, 0, nullptr, nullptr);
	if(!context) { return nullptr; }
	return std::make_shared<Disassembler>(context, targetSpec.arch);
}

LLVMJIT::DisassembledInstruction LLVMJIT::disassembleInstruction(
	const std::shared_ptr<Disassembler>& disasm,
	const U8* bytes,
	Uptr numBytes)
{
	DisassembledInstruction result;
	result.numBytes = 0;
	result.mnemonic[0] = '\0';

	if(!disasm) { return result; }

	Platform::Mutex::Lock lock(disasm->mutex);

	result.numBytes = LLVMDisasmInstruction(disasm->context,
											const_cast<U8*>(bytes),
											numBytes,
											reinterpret_cast<Uptr>(bytes),
											result.mnemonic,
											sizeof(result.mnemonic));
	return result;
}

Uptr LLVMJIT::findInstructionBoundary(const std::shared_ptr<Disassembler>& disasm,
									  const U8* code,
									  Uptr codeSize,
									  Uptr targetOffset)
{
	if(!disasm || targetOffset >= codeSize) { return targetOffset; }

	if(disasm->arch == TargetArch::aarch64)
	{
		// AArch64: fixed 4-byte instructions, just align.
		return targetOffset & ~Uptr(3);
	}

	// Variable-length ISA (x86-64): disassemble from the start to find boundaries.
	Uptr offset = 0;
	while(offset < targetOffset)
	{
		DisassembledInstruction instr
			= disassembleInstruction(disasm, code + offset, codeSize - offset);
		offset += instr.numBytes ? instr.numBytes : 1;
	}
	return offset;
}

LLVMJIT::DisassembledInstruction LLVMJIT::disassembleInstruction(const TargetSpec& targetSpec,
																 const U8* bytes,
																 Uptr numBytes)
{
	auto disasm = createDisassembler(targetSpec);
	return disassembleInstruction(disasm, bytes, numBytes);
}

std::string LLVMJIT::disassembleObject(const TargetSpec& targetSpec,
									   const std::vector<U8>& objectBytes)
{
	std::string result;

	std::string tripleStr = getTriple(targetSpec);
	LLVMDisasmContextRef disasmRef
		= LLVMCreateDisasm(tripleStr.c_str(), nullptr, 0, nullptr, nullptr);

	// Disabled because it triggers an "unsupported variant scheduling class" assertion in LLVM's
	// scheduling model on both x86 and AArch64.
	constexpr bool printLatency = false;
	if(printLatency)
	{
		WAVM_ERROR_UNLESS(LLVMSetDisasmOptions(disasmRef, LLVMDisassembler_Option_PrintLatency));
	}

	std::unique_ptr<llvm::object::ObjectFile> object
		= cantFail(llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef(
			llvm::StringRef((const char*)objectBytes.data(), objectBytes.size()), "memory")));

	// Iterate over the functions in the loaded object.
	for(std::pair<llvm::object::SymbolRef, U64> symbolSizePair :
		llvm::object::computeSymbolSizes(*object))
	{
		llvm::object::SymbolRef symbol = symbolSizePair.first;

		// Only process global symbols, which excludes SEH funclets.
		auto maybeFlags = symbol.getFlags();
		if(!(maybeFlags && *maybeFlags & llvm::object::SymbolRef::SF_Global)) { continue; }

		// Get the type, name, and address of the symbol. Need to be careful not to get the
		// Expected<T> for each value unless it will be checked for success before continuing.
		llvm::Expected<llvm::object::SymbolRef::Type> type = symbol.getType();
		if(!type || *type != llvm::object::SymbolRef::ST_Function) { continue; }
		llvm::Expected<llvm::StringRef> name = symbol.getName();
		if(!name) { continue; }
		llvm::Expected<U64> addressInSection = symbol.getAddress();
		if(!addressInSection) { continue; }

		// Compute the address the function was loaded at.
		llvm::StringRef sectionContents((const char*)objectBytes.data(), objectBytes.size());
		if(llvm::Expected<llvm::object::section_iterator> symbolSection = symbol.getSection())
		{
			if(llvm::Expected<llvm::StringRef> maybeSectionContents
			   = (*symbolSection)->getContents())
			{
				sectionContents = maybeSectionContents.get();
			}
		}

		WAVM_ERROR_UNLESS(addressInSection.get() + symbolSizePair.second >= addressInSection.get()
						  && addressInSection.get() + symbolSizePair.second
								 <= sectionContents.size());

		result += name.get().str();
		result += ": # ";
		result += std::to_string(addressInSection.get());
		result += '-';
		result += std::to_string(addressInSection.get() + symbolSizePair.second);
		result += '\n';

		const U8* nextByte = (const U8*)sectionContents.data() + addressInSection.get();
		Uptr numBytesRemaining = Uptr(symbolSizePair.second);
		while(numBytesRemaining)
		{
			char instructionBuffer[256];
			Uptr numInstructionBytes = LLVMDisasmInstruction(disasmRef,
															 const_cast<U8*>(nextByte),
															 numBytesRemaining,
															 reinterpret_cast<Uptr>(nextByte),
															 instructionBuffer,
															 sizeof(instructionBuffer));
			if(numInstructionBytes == 0)
			{
				numInstructionBytes = 1;
				result += "\t# skipped ";
				result += std::to_string(numInstructionBytes);
				result += " bytes.\n";
			}
			WAVM_ASSERT(numInstructionBytes <= numBytesRemaining);
			numBytesRemaining -= numInstructionBytes;
			nextByte += numInstructionBytes;

			result += instructionBuffer;
			result += '\n';
		};
	}

	LLVMDisasmDispose(disasmRef);

	return result;
}
