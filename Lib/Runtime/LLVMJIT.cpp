#include "LLVMJIT.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/HashMap.h"
#include "Inline/Lock.h"
#include "Inline/Timing.h"
#include "Logging/Logging.h"
#include "RuntimePrivate.h"

#include "LLVMPreInclude.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/ObjectMemoryBuffer.h"
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

// This needs to be 1 to allow debuggers such as Visual Studio to place breakpoints and step through
// the JITed code.
#define USE_WRITEABLE_JIT_CODE_PAGES WAVM_DEBUG

#define DUMP_UNOPTIMIZED_MODULE WAVM_DEBUG
#define VERIFY_MODULE WAVM_DEBUG
#define DUMP_OPTIMIZED_MODULE WAVM_DEBUG
#define DUMP_OBJECT WAVM_DEBUG
#define PRINT_DISASSEMBLY 0

using namespace IR;
using namespace LLVMJIT;

llvm::LLVMContext* LLVMJIT::llvmContext = nullptr;
llvm::Type* LLVMJIT::llvmValueTypes[(Uptr)ValueType::num];

llvm::Type* LLVMJIT::llvmI8Type;
llvm::Type* LLVMJIT::llvmI16Type;
llvm::Type* LLVMJIT::llvmI32Type;
llvm::Type* LLVMJIT::llvmI64Type;
llvm::Type* LLVMJIT::llvmI128Type;
llvm::Type* LLVMJIT::llvmF32Type;
llvm::Type* LLVMJIT::llvmF64Type;
llvm::Type* LLVMJIT::llvmVoidType;
llvm::Type* LLVMJIT::llvmBoolType;
llvm::Type* LLVMJIT::llvmI8PtrType;

llvm::Type* LLVMJIT::llvmI8x16Type;
llvm::Type* LLVMJIT::llvmI16x8Type;
llvm::Type* LLVMJIT::llvmI32x4Type;
llvm::Type* LLVMJIT::llvmI64x2Type;
llvm::Type* LLVMJIT::llvmI128x1Type;
llvm::Type* LLVMJIT::llvmF32x4Type;
llvm::Type* LLVMJIT::llvmF64x2Type;

llvm::Type* LLVMJIT::llvmExceptionPointersStructType;

llvm::Constant* LLVMJIT::typedZeroConstants[(Uptr)ValueType::num];

static Platform::Mutex llvmMutex;

static llvm::TargetMachine* targetMachine = nullptr;

static llvm::JITEventListener* gdbRegistrationListener = nullptr;

// A map from address to loaded JIT symbols.
static Platform::Mutex addressToSymbolMapMutex;
static std::map<Uptr, struct JITSymbol*> addressToSymbolMap;

// A map from function types to JIT symbols for cached invoke thunks (C++ -> WASM)
static HashMap<FunctionType, struct JITSymbol*> invokeThunkTypeToSymbolMap;

// A map from function types to JIT symbols for cached native thunks (WASM -> C++)
static HashMap<void*, struct JITSymbol*> intrinsicFunctionToThunkSymbolMap;

static void initLLVM();

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
struct UnitMemoryManager : llvm::RTDyldMemoryManager
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
		const Platform::MemoryAccess codeAccess = USE_WRITEABLE_JIT_CODE_PAGES
													  ? Platform::MemoryAccess::readWriteExecute
													  : Platform::MemoryAccess::execute;
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

// A unit of JIT compilation.
// Encapsulates the LLVM JIT compilation pipeline but allows subclasses to define how the resulting
// code is used.
struct JITUnit
{
	JITUnit(bool inShouldLogMetrics = true) : shouldLogMetrics(inShouldLogMetrics) {}
	~JITUnit() {}

	void compileAndLoad(llvm::Module&& llvmModule);

	virtual void notifySymbolLoaded(const char* name,
									Uptr baseAddress,
									Uptr numBytes,
									std::map<U32, U32>&& offsetToOpIndexMap)
		= 0;

private:
	UnitMemoryManager memoryManager;
	bool shouldLogMetrics;

	typedef llvm::SmallVector<char, 0> ObjectBytes;

	ObjectBytes compile(llvm::Module&& llvmModule);
	void load(ObjectBytes&& objectBytes);
};

// The JIT compilation unit for a WebAssembly module instance.
struct JITModule : JITUnit, JITModuleBase
{
	ModuleInstance* moduleInstance;

	std::vector<JITSymbol*> functionDefSymbols;

	JITModule(ModuleInstance* inModuleInstance) : moduleInstance(inModuleInstance) {}
	~JITModule() override
	{
		// Delete the module's symbols, and remove them from the global address-to-symbol map.
		Lock<Platform::Mutex> addressToSymbolMapLock(addressToSymbolMapMutex);
		for(auto symbol : functionDefSymbols)
		{
			addressToSymbolMap.erase(
				addressToSymbolMap.find(symbol->baseAddress + symbol->numBytes));
			delete symbol;
		}
	}

	void notifySymbolLoaded(const char* name,
							Uptr baseAddress,
							Uptr numBytes,
							std::map<U32, U32>&& offsetToOpIndexMap) override
	{
		// Save the address range this function was loaded at for future address->symbol lookups.
		Uptr functionDefIndex;
		if(getFunctionIndexFromExternalName(name, functionDefIndex))
		{
			wavmAssert(moduleInstance);
			wavmAssert(functionDefIndex < moduleInstance->functionDefs.size());
			FunctionInstance* functionInstance = moduleInstance->functionDefs[functionDefIndex];
			auto symbol                        = new JITSymbol(
                functionInstance, baseAddress, numBytes, std::move(offsetToOpIndexMap));
			functionDefSymbols.push_back(symbol);
			functionInstance->nativeFunction = reinterpret_cast<void*>(baseAddress);

			{
				Lock<Platform::Mutex> addressToSymbolMapLock(addressToSymbolMapMutex);
				addressToSymbolMap[baseAddress + numBytes] = symbol;
			}
		}
	}
};

// The JIT compilation unit for a single invoke thunk.
struct JITThunkUnit : JITUnit
{
	FunctionType functionType;

	JITSymbol* symbol;

	JITThunkUnit(FunctionType inFunctionType)
	: JITUnit(false), functionType(inFunctionType), symbol(nullptr)
	{
	}

	void notifySymbolLoaded(const char* name,
							Uptr baseAddress,
							Uptr numBytes,
							std::map<U32, U32>&& offsetToOpIndexMap) override
	{
#if(defined(_WIN32) && !defined(_WIN64))
		wavmAssert(!strcmp(name, "_thunk"));
#else
		wavmAssert(!strcmp(name, "thunk"));
#endif
		symbol = new JITSymbol(functionType, baseAddress, numBytes, std::move(offsetToOpIndexMap));
	}
};

static std::map<std::string, const char*> runtimeSymbolMap = {
#ifdef _WIN32
	// the LLVM X86 code generator calls __chkstk when allocating more than 4KB of stack space
	{"__chkstk", "__chkstk"},
	{"__C_specific_handler", "__C_specific_handler"},
#ifndef _WIN64
	{"__aullrem", "_aullrem"},
	{"__allrem", "_allrem"},
	{"__aulldiv", "_aulldiv"},
	{"__alldiv", "_alldiv"},
#endif
#else
	{"__CxxFrameHandler3", "__CxxFrameHandler3"},
	{"__cxa_begin_catch", "__cxa_begin_catch"},
	{"__gxx_personality_v0", "__gxx_personality_v0"},
#endif
#ifdef __arm__
	{"__aeabi_uidiv", "__aeabi_uidiv"},
	{"__aeabi_idiv", "__aeabi_idiv"},
	{"__aeabi_idivmod", "__aeabi_idivmod"},
	{"__aeabi_uldiv", "__aeabi_uldiv"},
	{"__aeabi_uldivmod", "__aeabi_uldivmod"},
	{"__aeabi_unwind_cpp_pr0", "__aeabi_unwind_cpp_pr0"},
	{"__aeabi_unwind_cpp_pr1", "__aeabi_unwind_cpp_pr1"},
#endif
};

llvm::JITEvaluatedSymbol LLVMJIT::resolveJITImport(llvm::StringRef name)
{
	// Allow some intrinsics used by LLVM
	auto runtimeSymbolNameIt = runtimeSymbolMap.find(name);
	if(runtimeSymbolNameIt == runtimeSymbolMap.end()) { return llvm::JITEvaluatedSymbol(nullptr); }

	const char* lookupName = runtimeSymbolNameIt->second;
	void* addr             = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(lookupName);
	if(!addr)
	{
		Errors::fatalf("LLVM generated code references undefined external symbol: %s\n",
					   lookupName);
	}
	return llvm::JITEvaluatedSymbol(reinterpret_cast<Uptr>(addr), llvm::JITSymbolFlags::None);
}

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

static Uptr printedModuleId = 0;

static void printModule(const llvm::Module& llvmModule, const char* filename)
{
	std::error_code errorCode;
	std::string augmentedFilename
		= std::string(filename) + std::to_string(printedModuleId++) + ".ll";
	llvm::raw_fd_ostream dumpFileStream(
		augmentedFilename, errorCode, llvm::sys::fs::OpenFlags::F_Text);
	llvmModule.print(dumpFileStream, nullptr);
	Log::printf(Log::debug, "Dumped LLVM module to: %s\n", augmentedFilename.c_str());
}

void JITUnit::compileAndLoad(llvm::Module&& llvmModule) { load(compile(std::move(llvmModule))); }

JITUnit::ObjectBytes JITUnit::compile(llvm::Module&& llvmModule)
{
	// Get a target machine object for this host, and set the module to use its data layout.
	llvmModule.setDataLayout(targetMachine->createDataLayout());

	// Verify the module.
	if(shouldLogMetrics && DUMP_UNOPTIMIZED_MODULE) { printModule(llvmModule, "llvmDump"); }
	if(shouldLogMetrics && VERIFY_MODULE)
	{
		std::string verifyOutputString;
		llvm::raw_string_ostream verifyOutputStream(verifyOutputString);
		if(llvm::verifyModule(llvmModule, &verifyOutputStream))
		{
			verifyOutputStream.flush();
			Errors::fatalf("LLVM verification errors:\n%s\n", verifyOutputString.c_str());
		}
		Log::printf(Log::debug, "Verified LLVM module\n");
	}

	// Run some optimization on the module's functions.
	Timing::Timer optimizationTimer;

	auto fpm = new llvm::legacy::FunctionPassManager(&llvmModule);
	fpm->add(llvm::createPromoteMemoryToRegisterPass());
	fpm->add(llvm::createInstructionCombiningPass());
	fpm->add(llvm::createCFGSimplificationPass());
	fpm->add(llvm::createJumpThreadingPass());
	fpm->add(llvm::createConstantPropagationPass());
	fpm->doInitialization();
	for(auto functionIt = llvmModule.begin(); functionIt != llvmModule.end(); ++functionIt)
	{ fpm->run(*functionIt); }
	delete fpm;

	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond(
			"Optimized LLVM module", optimizationTimer, (F64)llvmModule.size(), "functions");
	}

	if(shouldLogMetrics && DUMP_OPTIMIZED_MODULE) { printModule(llvmModule, "llvmOptimizedDump"); }

	// Generate machine code for the module.
	Timing::Timer machineCodeTimer;
	llvm::SmallVector<char, 0> objectBytes;
	{
		llvm::legacy::PassManager passManager;
		llvm::MCContext* mcContext;
		llvm::raw_svector_ostream objectStream(objectBytes);
		errorUnless(!targetMachine->addPassesToEmitMC(passManager, mcContext, objectStream));
		passManager.run(llvmModule);
	}
	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond(
			"Generated machine code", machineCodeTimer, (F64)llvmModule.size(), "functions");
	}

	if(shouldLogMetrics && DUMP_OBJECT)
	{
		// Dump the object file.
		std::error_code errorCode;
		static Uptr dumpedObjectId = 0;
		std::string augmentedFilename
			= std::string("jitObject") + std::to_string(dumpedObjectId++) + ".o";
		llvm::raw_fd_ostream dumpFileStream(
			augmentedFilename, errorCode, llvm::sys::fs::OpenFlags::F_None);
		dumpFileStream.write((const char*)objectBytes.data(), objectBytes.size());
		Log::printf(Log::Category::debug, "Dumped object file to: %s\n", augmentedFilename.c_str());
	}

	return objectBytes;
}

void JITUnit::load(ObjectBytes&& objectBytes)
{
	Timing::Timer loadObjectTimer;

	llvm::ObjectMemoryBuffer objectBuffer(std::move(objectBytes));

	auto object
		= cantFail(llvm::object::ObjectFile::createObjectFile(objectBuffer.getMemBufferRef()));

	// Create the LLVM object loader.
	struct SymbolResolver : llvm::JITSymbolResolver
	{
		virtual llvm::JITSymbol findSymbolInLogicalDylib(const std::string& name) override
		{
			return resolveJITImport(name);
		}
		virtual llvm::JITSymbol findSymbol(const std::string& name) override
		{
			return resolveJITImport(name);
		}
	};
	SymbolResolver symbolResolver;
	llvm::RuntimeDyld loader(memoryManager, symbolResolver);

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
		U8* trampolineBytes = memoryManager.allocateCodeSection(16, 16, 0, "seh_trampoline");
		trampolineBytes[0]  = 0xff;
		trampolineBytes[1]  = 0x25;
		memset(trampolineBytes + 2, 0, 4);
		memcpy(trampolineBytes + 6, &sehHandlerAddress, sizeof(U64));

		processSEHTables(memoryManager.getImageBaseAddress(),
						 *loadedObject,
						 pdataSection,
						 pdataCopy,
						 pdataNumBytes,
						 xdataSection,
						 xdataCopy,
						 reinterpret_cast<Uptr>(trampolineBytes));

		Platform::registerEHFrames(
			memoryManager.getImageBaseAddress(),
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
	memoryManager.reallyFinalizeMemory();

	// Notify GDB of the new object.
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
		notifySymbolLoaded(name->data(),
						   loadedAddress,
						   Uptr(symbolSizePair.second),
						   std::move(offsetToOpIndexMap));
	}

	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond("Loaded object",
								 loadObjectTimer,
								 (F64)objectBuffer.getBufferSize() / 1024.0 / 1024.0,
								 "MB");
	}
}

void LLVMJIT::instantiateModule(const IR::Module& module, ModuleInstance* moduleInstance)
{
	Lock<Platform::Mutex> llvmLock(llvmMutex);

	initLLVM();

	// Emit LLVM IR for the module.
	llvm::Module llvmModule("", *llvmContext);
	emitModule(module, moduleInstance, llvmModule);

	// Construct the JIT compilation pipeline for this module.
	auto jitModule            = new JITModule(moduleInstance);
	moduleInstance->jitModule = jitModule;

	// Compile the module.
	jitModule->compileAndLoad(std::move(llvmModule));
}

std::string LLVMJIT::getExternalFunctionName(ModuleInstance* moduleInstance, Uptr functionDefIndex)
{
	wavmAssert(functionDefIndex < moduleInstance->functionDefs.size());
	return "wasmFunc" + std::to_string(functionDefIndex) + "_"
		   + moduleInstance->functionDefs[functionDefIndex]->debugName;
}

bool LLVMJIT::getFunctionIndexFromExternalName(const char* externalName, Uptr& outFunctionDefIndex)
{
#if(defined(_WIN32) && !defined(_WIN64))
	const char wasmFuncPrefix[] = "_wasmFunc";
#else
	const char wasmFuncPrefix[] = "wasmFunc";
#endif
	const Uptr numPrefixChars = sizeof(wasmFuncPrefix) - 1;
	if(!strncmp(externalName, wasmFuncPrefix, numPrefixChars))
	{
		char* numberEnd        = nullptr;
		U64 functionDefIndex64 = std::strtoull(externalName + numPrefixChars, &numberEnd, 10);
		if(functionDefIndex64 > UINTPTR_MAX) { return false; }
		outFunctionDefIndex = Uptr(functionDefIndex64);
		return true;
	}
	else
	{
		return false;
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

InvokeFunctionPointer LLVMJIT::getInvokeThunk(FunctionType functionType,
											  CallingConvention callingConvention)
{
	Lock<Platform::Mutex> llvmLock(llvmMutex);

	initLLVM();

	// Reuse cached invoke thunks for the same function type.
	JITSymbol*& invokeThunkSymbol = invokeThunkTypeToSymbolMap.getOrAdd(functionType, nullptr);
	if(invokeThunkSymbol)
	{ return reinterpret_cast<InvokeFunctionPointer>(invokeThunkSymbol->baseAddress); }

	llvm::Module llvmModule("", *llvmContext);
	auto llvmFunctionType = llvm::FunctionType::get(
		llvmI8PtrType,
		{asLLVMType(functionType, callingConvention)->getPointerTo(), llvmI8PtrType},
		false);
	auto llvmFunction = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	llvm::Value* functionPointer = &*(llvmFunction->args().begin() + 0);
	llvm::Value* contextPointer  = &*(llvmFunction->args().begin() + 1);

	EmitContext emitContext(nullptr, nullptr);
	emitContext.irBuilder.SetInsertPoint(
		llvm::BasicBlock::Create(*llvmContext, "entry", llvmFunction));

	emitContext.contextPointerVariable = emitContext.irBuilder.CreateAlloca(llvmI8PtrType);
	emitContext.irBuilder.CreateStore(contextPointer, emitContext.contextPointerVariable);

	// Load the function's arguments from an array of 64-bit values at an address provided by the
	// caller.
	std::vector<llvm::Value*> arguments;
	Uptr argDataOffset = 0;
	for(ValueType parameterType : functionType.params())
	{
		if(parameterType == ValueType::v128)
		{
			// Use 16-byte alignment for V128 arguments.
			argDataOffset = (argDataOffset + 15) & ~15;
		}

		arguments.push_back(emitContext.loadFromUntypedPointer(
			emitContext.irBuilder.CreateInBoundsGEP(
				contextPointer,
				{emitLiteral(argDataOffset + offsetof(ContextRuntimeData, thunkArgAndReturnData))}),
			asLLVMType(parameterType)));

		argDataOffset += parameterType == ValueType::v128 ? 16 : 8;
	}

	// Call the function.
	ValueVector results
		= emitContext.emitCallOrInvoke(functionPointer, arguments, functionType, callingConvention);

	// If the function has a return value, write it to the context invoke return memory.
	wavmAssert(results.size() == functionType.results().size());
	auto newContextPointer = emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable);
	Uptr resultOffset      = 0;
	for(Uptr resultIndex = 0; resultIndex < results.size(); ++resultIndex)
	{
		const ValueType resultType = functionType.results()[resultIndex];
		const U8 resultNumBytes    = getTypeByteWidth(resultType);

		resultOffset = (resultOffset + resultNumBytes - 1) & -I8(resultNumBytes);
		wavmAssert(resultOffset < maxThunkArgAndReturnBytes);

		emitContext.irBuilder.CreateStore(results[resultIndex],
										  emitContext.irBuilder.CreatePointerCast(
											  emitContext.irBuilder.CreateInBoundsGEP(
												  newContextPointer, {emitLiteral(resultOffset)}),
											  asLLVMType(resultType)->getPointerTo()));

		resultOffset += resultNumBytes;
	}

	emitContext.irBuilder.CreateRet(
		emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable));

	// Compile the invoke thunk.
	auto jitUnit = new JITThunkUnit(functionType);
	jitUnit->compileAndLoad(std::move(llvmModule));

	wavmAssert(jitUnit->symbol);
	invokeThunkSymbol = jitUnit->symbol;

	{
		Lock<Platform::Mutex> addressToSymbolMapLock(addressToSymbolMapMutex);
		addressToSymbolMap[jitUnit->symbol->baseAddress + jitUnit->symbol->numBytes]
			= jitUnit->symbol;
	}

	return reinterpret_cast<InvokeFunctionPointer>(invokeThunkSymbol->baseAddress);
}

void* LLVMJIT::getIntrinsicThunk(void* nativeFunction,
								 FunctionType functionType,
								 CallingConvention callingConvention)
{
	wavmAssert(callingConvention == CallingConvention::intrinsic
			   || callingConvention == CallingConvention::intrinsicWithContextSwitch
			   || callingConvention == CallingConvention::intrinsicWithMemAndTable);

	Lock<Platform::Mutex> llvmLock(llvmMutex);

	initLLVM();

	// Reuse cached intrinsic thunks for the same function type.
	JITSymbol*& intrinsicThunkSymbol
		= intrinsicFunctionToThunkSymbolMap.getOrAdd(nativeFunction, nullptr);
	if(intrinsicThunkSymbol) { return reinterpret_cast<void*>(intrinsicThunkSymbol->baseAddress); }

	// Create a LLVM module containing a single function with the same signature as the native
	// function, but with the WASM calling convention.
	llvm::Module llvmModule("", *llvmContext);
	auto llvmFunctionType = asLLVMType(functionType, CallingConvention::wasm);
	auto llvmFunction     = llvm::Function::Create(
        llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	llvmFunction->setCallingConv(asLLVMCallingConv(callingConvention));

	EmitContext emitContext(nullptr, nullptr);
	emitContext.irBuilder.SetInsertPoint(
		llvm::BasicBlock::Create(*llvmContext, "entry", llvmFunction));

	emitContext.contextPointerVariable = emitContext.irBuilder.CreateAlloca(llvmI8PtrType);
	emitContext.irBuilder.CreateStore(&*llvmFunction->args().begin(),
									  emitContext.contextPointerVariable);

	llvm::SmallVector<llvm::Value*, 8> args;
	for(auto argIt = llvmFunction->args().begin() + 1; argIt != llvmFunction->args().end(); ++argIt)
	{ args.push_back(&*argIt); }

	llvm::Type* llvmNativeFunctionType
		= asLLVMType(functionType, callingConvention)->getPointerTo();
	llvm::Value* llvmNativeFunction = emitLiteralPointer(nativeFunction, llvmNativeFunctionType);
	ValueVector results
		= emitContext.emitCallOrInvoke(llvmNativeFunction, args, functionType, callingConvention);

	// Emit the function return.
	emitContext.emitReturn(functionType.results(), results);

	// Compile the LLVM IR to machine code.
	auto jitUnit = new JITThunkUnit(functionType);
	jitUnit->compileAndLoad(std::move(llvmModule));

	wavmAssert(jitUnit->symbol);
	intrinsicThunkSymbol = jitUnit->symbol;

	{
		Lock<Platform::Mutex> addressToSymbolMapLock(addressToSymbolMapMutex);
		addressToSymbolMap[jitUnit->symbol->baseAddress + jitUnit->symbol->numBytes]
			= jitUnit->symbol;
	}

	return reinterpret_cast<void*>(intrinsicThunkSymbol->baseAddress);
}

static void initLLVM()
{
	if(llvmContext) { return; }
	llvmContext = new llvm::LLVMContext();

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetDisassembler();
	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

	auto targetTriple = llvm::sys::getProcessTriple();
#ifdef __APPLE__
	// Didn't figure out exactly why, but this works around a problem with the MacOS dynamic loader.
	// Without it, our symbols can't be found in the JITed object file.
	targetTriple += "-elf";
#endif
	llvm::SmallVector<std::string, 0> machineAttrs = {LLVM_TARGET_ATTRIBUTES};
	targetMachine                                  = llvm::EngineBuilder().selectTarget(
        llvm::Triple(targetTriple), "", llvm::sys::getHostCPUName(), machineAttrs);

	llvmI8Type    = llvm::Type::getInt8Ty(*llvmContext);
	llvmI16Type   = llvm::Type::getInt16Ty(*llvmContext);
	llvmI32Type   = llvm::Type::getInt32Ty(*llvmContext);
	llvmI64Type   = llvm::Type::getInt64Ty(*llvmContext);
	llvmI128Type  = llvm::Type::getInt128Ty(*llvmContext);
	llvmF32Type   = llvm::Type::getFloatTy(*llvmContext);
	llvmF64Type   = llvm::Type::getDoubleTy(*llvmContext);
	llvmVoidType  = llvm::Type::getVoidTy(*llvmContext);
	llvmBoolType  = llvm::Type::getInt1Ty(*llvmContext);
	llvmI8PtrType = llvmI8Type->getPointerTo();

	auto llvmExceptionRecordStructType = llvm::StructType::create({
		llvmI32Type,   // DWORD ExceptionCode
		llvmI32Type,   // DWORD ExceptionFlags
		llvmI8PtrType, // _EXCEPTION_RECORD* ExceptionRecord
		llvmI8PtrType, // PVOID ExceptionAddress
		llvmI32Type,   // DWORD NumParameters
		llvm::ArrayType::get(llvmI64Type,
							 15) // ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS]
	});
	llvmExceptionPointersStructType
		= llvm::StructType::create({llvmExceptionRecordStructType->getPointerTo(), llvmI8PtrType});

	llvmI8x16Type  = llvm::VectorType::get(llvmI8Type, 16);
	llvmI16x8Type  = llvm::VectorType::get(llvmI16Type, 8);
	llvmI32x4Type  = llvm::VectorType::get(llvmI32Type, 4);
	llvmI64x2Type  = llvm::VectorType::get(llvmI64Type, 2);
	llvmI128x1Type = llvm::VectorType::get(llvmI128Type, 1);
	llvmF32x4Type  = llvm::VectorType::get(llvmF32Type, 4);
	llvmF64x2Type  = llvm::VectorType::get(llvmF64Type, 2);

	llvmValueTypes[(Uptr)ValueType::i32]  = llvmI32Type;
	llvmValueTypes[(Uptr)ValueType::i64]  = llvmI64Type;
	llvmValueTypes[(Uptr)ValueType::f32]  = llvmF32Type;
	llvmValueTypes[(Uptr)ValueType::f64]  = llvmF64Type;
	llvmValueTypes[(Uptr)ValueType::v128] = llvmI128x1Type;

	// Create zero constants of each type.
	typedZeroConstants[(Uptr)ValueType::any] = nullptr;
	typedZeroConstants[(Uptr)ValueType::i32] = emitLiteral((U32)0);
	typedZeroConstants[(Uptr)ValueType::i64] = emitLiteral((U64)0);
	typedZeroConstants[(Uptr)ValueType::f32] = emitLiteral((F32)0.0f);
	typedZeroConstants[(Uptr)ValueType::f64] = emitLiteral((F64)0.0);

	U64 i64x2Zero[2]                          = {0, 0};
	typedZeroConstants[(Uptr)ValueType::v128] = llvm::ConstantVector::get(
		{llvm::ConstantInt::get(llvmI128Type, llvm::APInt(128, 2, i64x2Zero))});

	if(!gdbRegistrationListener)
	{ gdbRegistrationListener = llvm::JITEventListener::createGDBRegistrationListener(); }
}

namespace LLVMJIT
{
	RUNTIME_API void deinit()
	{
		Lock<Platform::Mutex> llvmLock(llvmMutex);

		if(llvmContext)
		{
			delete llvmContext;
			llvmContext = nullptr;

			delete targetMachine;
			targetMachine = nullptr;

			llvmI8Type = llvmI16Type = llvmI32Type = llvmI64Type = nullptr;
			llvmF32Type = llvmF64Type = nullptr;
			llvmVoidType = llvmBoolType = llvmI8PtrType = nullptr;
			llvmExceptionPointersStructType             = nullptr;
			llvmI8x16Type = llvmI16x8Type = llvmI32x4Type = llvmI64x2Type = nullptr;

			memset(llvmValueTypes, 0, sizeof(llvmValueTypes));
			memset(typedZeroConstants, 0, sizeof(typedZeroConstants));
		}
	}
}
