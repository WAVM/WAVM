#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/HashMap.h"
#include "Inline/Lock.h"
#include "Inline/Timing.h"
#include "LLVMJIT.h"
#include "Logging/Logging.h"
#include "RuntimePrivate.h"

#include "LLVMPreInclude.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolSize.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

#include "LLVMPostInclude.h"

#include <map>

#define PRINT_DISASSEMBLY 0

#if PRINT_DISASSEMBLY

#include "LLVMPreInclude.h"

#include "llvm-c/Disassembler.h"

#include "LLVMPostInclude.h"

static void disassembleFunction(U8* bytes, Uptr numBytes)
{
	LLVMDisasmContextRef disasmRef
		= LLVMCreateDisasm(llvm::sys::getProcessTriple().c_str(), nullptr, 0, nullptr, nullptr);

	U8* nextByte           = bytes;
	Uptr numBytesRemaining = numBytes;
	while(numBytesRemaining)
	{
		char instructionBuffer[256];
		Uptr numInstructionBytes = LLVMDisasmInstruction(disasmRef,
														 nextByte,
														 numBytesRemaining,
														 reinterpret_cast<Uptr>(nextByte),
														 instructionBuffer,
														 sizeof(instructionBuffer));
		if(numInstructionBytes == 0) { numInstructionBytes = 1; }
		wavmAssert(numInstructionBytes <= numBytesRemaining);
		numBytesRemaining -= numInstructionBytes;
		nextByte += numInstructionBytes;

		Log::printf(Log::error,
					"\t\t0x%04x %s\n",
					(nextByte - bytes - numInstructionBytes),
					instructionBuffer);
	};

	LLVMDisasmDispose(disasmRef);
}
#endif

using namespace IR;
using namespace LLVMJIT;
using namespace Runtime;

static llvm::JITEventListener* gdbRegistrationListener = nullptr;

// A map from address to loaded JIT symbols.
static Platform::Mutex addressToSymbolMapMutex;
static std::map<Uptr, struct JITSymbol*> addressToSymbolMap;

// Information about a JIT symbol, used to map instruction pointers to descriptive names.
struct JITSymbol
{
	enum class Type
	{
		functionInstance,
		invokeThunk
	};
	Type type;
	union
	{
		FunctionInstance* functionInstance;
		FunctionType invokeThunkType;
	};
	Uptr baseAddress;
	Uptr numBytes;
	std::map<U32, U32> offsetToOpIndexMap;

	JITSymbol(FunctionInstance* inFunctionInstance,
			  Uptr inBaseAddress,
			  Uptr inNumBytes,
			  std::map<U32, U32>&& inOffsetToOpIndexMap)
	: type(Type::functionInstance)
	, functionInstance(inFunctionInstance)
	, baseAddress(inBaseAddress)
	, numBytes(inNumBytes)
	, offsetToOpIndexMap(inOffsetToOpIndexMap)
	{
	}

	JITSymbol(FunctionType inInvokeThunkType,
			  Uptr inBaseAddress,
			  Uptr inNumBytes,
			  std::map<U32, U32>&& inOffsetToOpIndexMap)
	: type(Type::invokeThunk)
	, invokeThunkType(inInvokeThunkType)
	, baseAddress(inBaseAddress)
	, numBytes(inNumBytes)
	, offsetToOpIndexMap(inOffsetToOpIndexMap)
	{
	}
};

// Allocates memory for the LLVM object loader.
struct LLVMJIT::UnitMemoryManager : llvm::RTDyldMemoryManager
{
	UnitMemoryManager()
	: imageBaseAddress(nullptr)
	, isFinalized(false)
	, codeSection({0})
	, readOnlySection({0})
	, readWriteSection({0})
	, hasRegisteredEHFrames(false)
	{
	}
	virtual ~UnitMemoryManager() override
	{
		// Deregister the exception handling frame info.
		deregisterEHFrames();

		// Decommit the image pages, but leave them reserved to catch any references to them that
		// might erroneously remain.
		Platform::decommitVirtualPages(imageBaseAddress, numAllocatedImagePages);
	}

	void registerEHFrames(U8* addr, U64 loadAddr, uintptr_t numBytes) override
	{
		if(!USE_WINDOWS_SEH)
		{
			Platform::registerEHFrames(imageBaseAddress, addr, numBytes);
			hasRegisteredEHFrames = true;
			ehFramesAddr          = addr;
			ehFramesNumBytes      = numBytes;
		}
	}
	void deregisterEHFrames() override
	{
		if(hasRegisteredEHFrames)
		{
			hasRegisteredEHFrames = false;
			Platform::deregisterEHFrames(imageBaseAddress, ehFramesAddr, ehFramesNumBytes);
		}
	}

	virtual bool needsToReserveAllocationSpace() override { return true; }
	virtual void reserveAllocationSpace(uintptr_t numCodeBytes,
										U32 codeAlignment,
										uintptr_t numReadOnlyBytes,
										U32 readOnlyAlignment,
										uintptr_t numReadWriteBytes,
										U32 readWriteAlignment) override
	{
		if(USE_WINDOWS_SEH)
		{
			// Pad the code section to allow for the SEH trampoline.
			numCodeBytes += 32;
		}

		// Calculate the number of pages to be used by each section.
		codeSection.numPages      = shrAndRoundUp(numCodeBytes, Platform::getPageSizeLog2());
		readOnlySection.numPages  = shrAndRoundUp(numReadOnlyBytes, Platform::getPageSizeLog2());
		readWriteSection.numPages = shrAndRoundUp(numReadWriteBytes, Platform::getPageSizeLog2());
		numAllocatedImagePages
			= codeSection.numPages + readOnlySection.numPages + readWriteSection.numPages;
		if(numAllocatedImagePages)
		{
			// Reserve enough contiguous pages for all sections.
			imageBaseAddress = Platform::allocateVirtualPages(numAllocatedImagePages);
			if(!imageBaseAddress
			   || !Platform::commitVirtualPages(imageBaseAddress, numAllocatedImagePages))
			{ Errors::fatal("memory allocation for JIT code failed"); }
			codeSection.baseAddress = imageBaseAddress;
			readOnlySection.baseAddress
				= codeSection.baseAddress + (codeSection.numPages << Platform::getPageSizeLog2());
			readWriteSection.baseAddress
				= readOnlySection.baseAddress
				  + (readOnlySection.numPages << Platform::getPageSizeLog2());
		}
	}
	virtual U8* allocateCodeSection(uintptr_t numBytes,
									U32 alignment,
									U32 sectionID,
									llvm::StringRef sectionName) override
	{
		return allocateBytes((Uptr)numBytes, alignment, codeSection);
	}
	virtual U8* allocateDataSection(uintptr_t numBytes,
									U32 alignment,
									U32 sectionID,
									llvm::StringRef SectionName,
									bool isReadOnly) override
	{
		return allocateBytes(
			(Uptr)numBytes, alignment, isReadOnly ? readOnlySection : readWriteSection);
	}
	virtual bool finalizeMemory(std::string* ErrMsg = nullptr) override
	{
		// finalizeMemory is called before we manually apply SEH relocations, so don't do anything
		// here and let the finalize callback call reallyFinalizeMemory when it's done applying the
		// SEH relocations.
		return true;
	}
	void reallyFinalizeMemory()
	{
		wavmAssert(!isFinalized);
		isFinalized                             = true;
		const Platform::MemoryAccess codeAccess = Platform::MemoryAccess::execute;
		if(codeSection.numPages)
		{
			errorUnless(Platform::setVirtualPageAccess(
				codeSection.baseAddress, codeSection.numPages, codeAccess));
		}
		if(readOnlySection.numPages)
		{
			errorUnless(Platform::setVirtualPageAccess(readOnlySection.baseAddress,
													   readOnlySection.numPages,
													   Platform::MemoryAccess::readOnly));
		}
		if(readWriteSection.numPages)
		{
			errorUnless(Platform::setVirtualPageAccess(readWriteSection.baseAddress,
													   readWriteSection.numPages,
													   Platform::MemoryAccess::readWrite));
		}
	}
	virtual void invalidateInstructionCache()
	{
		// Invalidate the instruction cache for the whole image.
		llvm::sys::Memory::InvalidateInstructionCache(
			imageBaseAddress, numAllocatedImagePages << Platform::getPageSizeLog2());
	}

	U8* getImageBaseAddress() const { return imageBaseAddress; }

private:
	struct Section
	{
		U8* baseAddress;
		Uptr numPages;
		Uptr numCommittedBytes;
	};

	U8* imageBaseAddress;
	Uptr numAllocatedImagePages;
	bool isFinalized;

	Section codeSection;
	Section readOnlySection;
	Section readWriteSection;

	bool hasRegisteredEHFrames;
	U8* ehFramesAddr;
	Uptr ehFramesNumBytes;

	U8* allocateBytes(Uptr numBytes, Uptr alignment, Section& section)
	{
		wavmAssert(section.baseAddress);
		wavmAssert(!(alignment & (alignment - 1)));
		wavmAssert(!isFinalized);

		// Allocate the section at the lowest uncommitted byte of image memory.
		U8* allocationBaseAddress
			= section.baseAddress + align(section.numCommittedBytes, alignment);
		wavmAssert(!(reinterpret_cast<Uptr>(allocationBaseAddress) & (alignment - 1)));
		section.numCommittedBytes
			= align(section.numCommittedBytes, alignment) + align(numBytes, alignment);

		// Check that enough space was reserved in the section.
		if(section.numCommittedBytes > (section.numPages << Platform::getPageSizeLog2()))
		{ Errors::fatal("didn't reserve enough space in section"); }

		return allocationBaseAddress;
	}

	static Uptr align(Uptr size, Uptr alignment)
	{
		return (size + alignment - 1) & ~(alignment - 1);
	}
	static Uptr shrAndRoundUp(Uptr value, Uptr shift)
	{
		return (value + (Uptr(1) << shift) - 1) >> shift;
	}

	UnitMemoryManager(const UnitMemoryManager&) = delete;
	void operator=(const UnitMemoryManager&) = delete;
};

LLVMJIT::JITUnit::JITUnit() : memoryManager(new UnitMemoryManager()) {}

LLVMJIT::JITUnit::~JITUnit()
{
	// Delete the unit's symbols, and remove them from the global address-to-symbol map.
	Lock<Platform::Mutex> addressToSymbolMapLock(addressToSymbolMapMutex);
	for(auto symbol : symbols)
	{
		addressToSymbolMap.erase(addressToSymbolMap.find(symbol->baseAddress + symbol->numBytes));
		delete symbol;
	}
	addressToSymbolMapLock.unlock();

	// Delete the memory manager.
	delete memoryManager;
}

void JITUnit::load(const std::vector<U8>& objectBytes,
				   const HashMap<std::string, Uptr>& importedSymbolMap,
				   bool shouldLogMetrics)
{
	Timing::Timer loadObjectTimer;

	auto object = cantFail(llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef(
		llvm::StringRef((const char*)objectBytes.data(), objectBytes.size()), "memory")));

	// Create the LLVM object loader.
	struct SymbolResolver : llvm::JITSymbolResolver
	{
		const HashMap<std::string, Uptr>& importedSymbolMap;

		SymbolResolver(const HashMap<std::string, Uptr>& inImportedSymbolMap)
		: importedSymbolMap(inImportedSymbolMap)
		{
		}

		virtual llvm::Expected<std::map<llvm::StringRef, llvm::JITEvaluatedSymbol>> lookup(
			const std::set<llvm::StringRef>& symbols) override
		{
			std::map<llvm::StringRef, llvm::JITEvaluatedSymbol> result;
			for(auto symbol : symbols)
			{ result.emplace(symbol, findSymbol(symbol)); }
			return result;
		}
		virtual llvm::Expected<std::map<llvm::StringRef, llvm::JITSymbolFlags>> lookupFlags(
			const std::set<llvm::StringRef>& symbols) override
		{
			std::map<llvm::StringRef, llvm::JITSymbolFlags> result;
			for(auto symbol : symbols)
			{ result.emplace(symbol, findSymbol(symbol).getFlags()); }
			return result;
		}
	private:
		llvm::JITEvaluatedSymbol findSymbol(llvm::StringRef name)
		{
			const Uptr* symbolValue = importedSymbolMap.get(name.str());
			if(!symbolValue) { return resolveJITImport(name); }
			else
			{
				// LLVM assumes that a symbol value of zero is a symbol that wasn't resolved.
				wavmAssert(*symbolValue);
				return llvm::JITEvaluatedSymbol(U64(*symbolValue), llvm::JITSymbolFlags::None);
			}
		}
	};
	SymbolResolver symbolResolver(importedSymbolMap);
	llvm::RuntimeDyld loader(*memoryManager, symbolResolver);

	// Process all sections on non-Windows platforms. On Windows, this triggers errors due to
	// unimplemented relocation types in the debug sections.
#ifndef _WIN32
	loader.setProcessAllSections(true);
#endif

	// The LLVM dynamic loader doesn't correctly apply the IMAGE_REL_AMD64_ADDR32NB relocations in
	// the pdata and xdata sections
	// (https://github.com/llvm-mirror/llvm/blob/e84d8c12d5157a926db15976389f703809c49aa5/lib/ExecutionEngine/RuntimeDyld/Targets/RuntimeDyldCOFFX86_64.h#L96)
	// Make a copy of those sections before they are clobbered, so we can do the fixup ourselves
	// later.
	llvm::object::SectionRef pdataSection;
	U8* pdataCopy      = nullptr;
	Uptr pdataNumBytes = 0;
	llvm::object::SectionRef xdataSection;
	U8* xdataCopy = nullptr;
	if(USE_WINDOWS_SEH)
	{
		for(auto section : object->sections())
		{
			llvm::StringRef sectionName;
			if(!section.getName(sectionName))
			{
				llvm::StringRef sectionContents;
				if(!section.getContents(sectionContents))
				{
					const U8* loadedSection = (const U8*)sectionContents.data();
					if(sectionName == ".pdata")
					{
						pdataCopy     = new U8[section.getSize()];
						pdataNumBytes = section.getSize();
						pdataSection  = section;
						memcpy(pdataCopy, loadedSection, section.getSize());
					}
					else if(sectionName == ".xdata")
					{
						xdataCopy    = new U8[section.getSize()];
						xdataSection = section;
						memcpy(xdataCopy, loadedSection, section.getSize());
					}
				}
			}
		}
	}

	// Use the LLVM object loader to load the object.
	std::unique_ptr<llvm::RuntimeDyld::LoadedObjectInfo> loadedObject = loader.loadObject(*object);
	loader.finalizeWithMemoryManagerLocking();
	if(loader.hasError())
	{ Errors::fatalf("RuntimeDyld failed: %s", loader.getErrorString().data()); }

	if(USE_WINDOWS_SEH && pdataCopy)
	{
		// Lookup the real address of __C_specific_handler.
		const llvm::JITEvaluatedSymbol sehHandlerSymbol = resolveJITImport("__C_specific_handler");
		errorUnless(sehHandlerSymbol);
		const U64 sehHandlerAddress = U64(sehHandlerSymbol.getAddress());

		// Create a trampoline within the image's 2GB address space that jumps to
		// __C_specific_handler. jmp [rip+0] <64-bit address>
		U8* trampolineBytes = memoryManager->allocateCodeSection(16, 16, 0, "seh_trampoline");
		trampolineBytes[0]  = 0xff;
		trampolineBytes[1]  = 0x25;
		memset(trampolineBytes + 2, 0, 4);
		memcpy(trampolineBytes + 6, &sehHandlerAddress, sizeof(U64));

		processSEHTables(memoryManager->getImageBaseAddress(),
						 *loadedObject,
						 pdataSection,
						 pdataCopy,
						 pdataNumBytes,
						 xdataSection,
						 xdataCopy,
						 reinterpret_cast<Uptr>(trampolineBytes));

		Platform::registerEHFrames(
			memoryManager->getImageBaseAddress(),
			reinterpret_cast<const U8*>(Uptr(loadedObject->getSectionLoadAddress(pdataSection))),
			pdataNumBytes);
	}

	// Free the copies of the Windows SEH sections created above.
	if(pdataCopy)
	{
		delete[] pdataCopy;
		pdataCopy = nullptr;
	}
	if(xdataCopy)
	{
		delete[] xdataCopy;
		xdataCopy = nullptr;
	}

	// After having a chance to manually apply relocations for the pdata/xdata sections, apply the
	// final non-writable memory permissions.
	memoryManager->reallyFinalizeMemory();

	// Notify GDB of the new object.
	if(!gdbRegistrationListener)
	{ gdbRegistrationListener = llvm::JITEventListener::createGDBRegistrationListener(); }
	gdbRegistrationListener->NotifyObjectEmitted(*object, *loadedObject);

	// Create a DWARF context to interpret the debug information in this compilation unit.
	auto dwarfContext = llvm::DWARFContext::create(*object, &*loadedObject);

	// Iterate over the functions in the loaded object.
	for(auto symbolSizePair : llvm::object::computeSymbolSizes(*object))
	{
		auto symbol = symbolSizePair.first;

		// Get the type, name, and address of the symbol. Need to be careful not to get the
		// Expected<T> for each value unless it will be checked for success before continuing.
		auto type = symbol.getType();
		if(!type || *type != llvm::object::SymbolRef::ST_Function) { continue; }
		auto name = symbol.getName();
		if(!name) { continue; }
		auto address = symbol.getAddress();
		if(!address) { continue; }

		// Compute the address the functions was loaded at.
		wavmAssert(*address <= UINTPTR_MAX);
		Uptr loadedAddress = Uptr(*address);
		auto symbolSection = symbol.getSection();
		if(symbolSection)
		{ loadedAddress += (Uptr)loadedObject->getSectionLoadAddress(*symbolSection.get()); }

		// Get the DWARF line info for this symbol, which maps machine code addresses to
		// WebAssembly op indices.
		llvm::DILineInfoTable lineInfoTable
			= dwarfContext->getLineInfoForAddressRange(loadedAddress, symbolSizePair.second);
		std::map<U32, U32> offsetToOpIndexMap;
		for(auto lineInfo : lineInfoTable)
		{ offsetToOpIndexMap.emplace(U32(lineInfo.first - loadedAddress), lineInfo.second.Line); }

#if PRINT_DISASSEMBLY
		if(shouldLogMetrics)
		{
			Log::printf(Log::error, "Disassembly for function %s\n", name.get().data());
			disassembleFunction(reinterpret_cast<U8*>(loadedAddress), Uptr(symbolSizePair.second));
		}
#endif

		// Notify the JIT unit that the symbol was loaded.
		wavmAssert(symbolSizePair.second <= UINTPTR_MAX);
		JITSymbol* jitSymbol = notifySymbolLoaded(name->data(),
												  loadedAddress,
												  Uptr(symbolSizePair.second),
												  std::move(offsetToOpIndexMap));

		if(jitSymbol)
		{
			symbols.push_back(jitSymbol);

			Lock<Platform::Mutex> addressToSymbolMapLock(addressToSymbolMapMutex);
			addressToSymbolMap[loadedAddress + symbolSizePair.second] = jitSymbol;
		}
	}

	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond(
			"Loaded object", loadObjectTimer, (F64)objectBytes.size() / 1024.0 / 1024.0, "MB");
	}
}

bool LLVMJIT::describeInstructionPointer(Uptr ip, std::string& outDescription)
{
	JITSymbol* symbol;
	{
		Lock<Platform::Mutex> addressToSymbolMapLock(addressToSymbolMapMutex);
		auto symbolIt = addressToSymbolMap.upper_bound(ip);
		if(symbolIt == addressToSymbolMap.end()) { return false; }
		symbol = symbolIt->second;
	}
	if(ip < symbol->baseAddress || ip >= symbol->baseAddress + symbol->numBytes) { return false; }

	switch(symbol->type)
	{
	case JITSymbol::Type::functionInstance:
	{
		outDescription = "wasm!";
		outDescription += symbol->functionInstance->moduleInstance->debugName;
		outDescription += '!';
		outDescription += symbol->functionInstance->debugName;
		outDescription += '+';

		// Find the highest entry in the offsetToOpIndexMap whose offset is <= the symbol-relative
		// IP.
		U32 ipOffset = (U32)(ip - symbol->baseAddress);
		Iptr opIndex = -1;
		for(auto offsetMapIt : symbol->offsetToOpIndexMap)
		{
			if(offsetMapIt.first <= ipOffset) { opIndex = offsetMapIt.second; }
			else
			{
				break;
			}
		}
		outDescription += std::to_string(opIndex >= 0 ? opIndex : 0);

		return true;
	}
	case JITSymbol::Type::invokeThunk:
		outDescription = "thnk!";
		outDescription += asString(symbol->invokeThunkType);
		outDescription += '+';
		outDescription += std::to_string(ip - symbol->baseAddress);
		return true;
	default: Errors::unreachable();
	};
}
