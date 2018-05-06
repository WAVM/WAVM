#include "LLVMJIT.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/HashMap.h"
#include "Inline/Timing.h"
#include "Logging/Logging.h"
#include "RuntimePrivate.h"

#include "llvm/ExecutionEngine/JITEventListener.h"

// This needs to be 1 to allow debuggers such as Visual Studio to place breakpoints and step through the JITed code.
#define USE_WRITEABLE_JIT_CODE_PAGES WAVM_DEBUG

#define DUMP_UNOPTIMIZED_MODULE WAVM_DEBUG
#define VERIFY_MODULE WAVM_DEBUG
#define DUMP_OPTIMIZED_MODULE WAVM_DEBUG
#define DUMP_OBJECT WAVM_DEBUG
#define PRINT_DISASSEMBLY 0

#if PRINT_DISASSEMBLY
#include "llvm-c/Disassembler.h"
#endif

namespace LLVMJIT
{
	Platform::Mutex llvmMutex;
	llvm::LLVMContext* llvmContext = nullptr;
	llvm::TargetMachine* targetMachine = nullptr;
	llvm::Type* llvmResultTypes[(Uptr)ResultType::num];
	llvm::JITEventListener* gdbRegistrationListener = nullptr;

	llvm::Type* llvmI8Type;
	llvm::Type* llvmI16Type;
	llvm::Type* llvmI32Type;
	llvm::Type* llvmI64Type;
	llvm::Type* llvmF32Type;
	llvm::Type* llvmF64Type;
	llvm::Type* llvmVoidType;
	llvm::Type* llvmBoolType;
	llvm::Type* llvmI8PtrType;
	
	llvm::Type* llvmI8x16Type;
	llvm::Type* llvmI16x8Type;
	llvm::Type* llvmI32x4Type;
	llvm::Type* llvmI64x2Type;
	llvm::Type* llvmF32x4Type;
	llvm::Type* llvmF64x2Type;

	#if defined(_WIN64)
	llvm::Type* llvmExceptionPointersStructType;
	#endif

	llvm::Constant* typedZeroConstants[(Uptr)ValueType::num];
	
	// A map from address to loaded JIT symbols.
	Platform::Mutex addressToSymbolMapMutex;
	std::map<Uptr,struct JITSymbol*> addressToSymbolMap;

	// A map from function types to JIT symbols for cached invoke thunks (C++ -> WASM)
	HashMap<const FunctionType*,struct JITSymbol*> invokeThunkTypeToSymbolMap;

	// A map from function types to JIT symbols for cached native thunks (WASM -> C++)
	HashMap<void*,struct JITSymbol*> intrinsicFunctionToThunkSymbolMap;

	void initLLVM();

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
			const FunctionType* invokeThunkType;
		};
		Uptr baseAddress;
		Uptr numBytes;
		std::map<U32,U32> offsetToOpIndexMap;
		
		JITSymbol(FunctionInstance* inFunctionInstance,Uptr inBaseAddress,Uptr inNumBytes,std::map<U32,U32>&& inOffsetToOpIndexMap)
		: type(Type::functionInstance), functionInstance(inFunctionInstance), baseAddress(inBaseAddress), numBytes(inNumBytes), offsetToOpIndexMap(inOffsetToOpIndexMap) {}

		JITSymbol(const FunctionType* inInvokeThunkType,Uptr inBaseAddress,Uptr inNumBytes,std::map<U32,U32>&& inOffsetToOpIndexMap)
		: type(Type::invokeThunk), invokeThunkType(inInvokeThunkType), baseAddress(inBaseAddress), numBytes(inNumBytes), offsetToOpIndexMap(inOffsetToOpIndexMap) {}
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
		{}
		virtual ~UnitMemoryManager() override
		{
			// Deregister the exception handling frame info.
			if(hasRegisteredEHFrames)
			{
				hasRegisteredEHFrames = false;
				deregisterEHFrames();
			}

			// Decommit the image pages, but leave them reserved to catch any references to them that might erroneously remain.
			Platform::decommitVirtualPages(imageBaseAddress,numAllocatedImagePages);
		}
		
		void registerEHFrames(U8* addr, U64 loadAddr,uintptr_t numBytes) override
		{
			llvm::RTDyldMemoryManager::registerEHFrames(addr,loadAddr,numBytes);
			hasRegisteredEHFrames = true;
			ehFramesAddr = addr;
			ehFramesLoadAddr = loadAddr;
			ehFramesNumBytes = numBytes;
		}
		void deregisterEHFrames() override
		{
			llvm::RTDyldMemoryManager::deregisterEHFrames();
		}
		
		virtual bool needsToReserveAllocationSpace() override { return true; }
		virtual void reserveAllocationSpace(uintptr_t numCodeBytes,U32 codeAlignment,uintptr_t numReadOnlyBytes,U32 readOnlyAlignment,uintptr_t numReadWriteBytes,U32 readWriteAlignment) override
		{
			// Pad the code section to allow for the SEH trampoline.
			numCodeBytes += 32;

			// Calculate the number of pages to be used by each section.
			codeSection.numPages = shrAndRoundUp(numCodeBytes,Platform::getPageSizeLog2());
			readOnlySection.numPages = shrAndRoundUp(numReadOnlyBytes,Platform::getPageSizeLog2());
			readWriteSection.numPages = shrAndRoundUp(numReadWriteBytes,Platform::getPageSizeLog2());
			numAllocatedImagePages = codeSection.numPages + readOnlySection.numPages + readWriteSection.numPages;
			if(numAllocatedImagePages)
			{
				// Reserve enough contiguous pages for all sections.
				imageBaseAddress = Platform::allocateVirtualPages(numAllocatedImagePages);
				if(!imageBaseAddress || !Platform::commitVirtualPages(imageBaseAddress,numAllocatedImagePages)) { Errors::fatal("memory allocation for JIT code failed"); }
				codeSection.baseAddress = imageBaseAddress;
				readOnlySection.baseAddress = codeSection.baseAddress + (codeSection.numPages << Platform::getPageSizeLog2());
				readWriteSection.baseAddress = readOnlySection.baseAddress + (readOnlySection.numPages << Platform::getPageSizeLog2());
			}
		}
		virtual U8* allocateCodeSection(uintptr_t numBytes,U32 alignment,U32 sectionID,llvm::StringRef sectionName) override
		{
			return allocateBytes((Uptr)numBytes,alignment,codeSection);
		}
		virtual U8* allocateDataSection(uintptr_t numBytes,U32 alignment,U32 sectionID,llvm::StringRef SectionName,bool isReadOnly) override
		{
			return allocateBytes((Uptr)numBytes,alignment,isReadOnly ? readOnlySection : readWriteSection);
		}
		virtual bool finalizeMemory(std::string* ErrMsg = nullptr) override
		{
			wavmAssert(!isFinalized);
			isFinalized = true;
			// Set the requested final memory access for each section's pages.
			#if 0
			const Platform::MemoryAccess codeAccess = USE_WRITEABLE_JIT_CODE_PAGES ? Platform::MemoryAccess::readWriteExecute : Platform::MemoryAccess::execute;
			if(codeSection.numPages && !Platform::setVirtualPageAccess(codeSection.baseAddress,codeSection.numPages,codeAccess)) { return false; }
			if(readOnlySection.numPages && !Platform::setVirtualPageAccess(readOnlySection.baseAddress,readOnlySection.numPages,Platform::MemoryAccess::readOnly)) { return false; }
			if(readWriteSection.numPages && !Platform::setVirtualPageAccess(readWriteSection.baseAddress,readWriteSection.numPages,Platform::MemoryAccess::readWrite)) { return false; }
			#else
			const Platform::MemoryAccess codeAccess = Platform::MemoryAccess::readWriteExecute;
			if(codeSection.numPages && !Platform::setVirtualPageAccess(codeSection.baseAddress,codeSection.numPages,codeAccess)) { return false; }
			if(readOnlySection.numPages && !Platform::setVirtualPageAccess(readOnlySection.baseAddress,readOnlySection.numPages,Platform::MemoryAccess::readWrite)) { return false; }
			if(readWriteSection.numPages && !Platform::setVirtualPageAccess(readWriteSection.baseAddress,readWriteSection.numPages,Platform::MemoryAccess::readWrite)) { return false; }
			#endif
			return true;
		}
		virtual void invalidateInstructionCache()
		{
			// Invalidate the instruction cache for the whole image.
			llvm::sys::Memory::InvalidateInstructionCache(imageBaseAddress,numAllocatedImagePages << Platform::getPageSizeLog2());
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
		U64 ehFramesLoadAddr;
		Uptr ehFramesNumBytes;

		U8* allocateBytes(Uptr numBytes,Uptr alignment,Section& section)
		{
			wavmAssert(section.baseAddress);
			wavmAssert(!(alignment & (alignment - 1)));
			wavmAssert(!isFinalized);
			
			// Allocate the section at the lowest uncommitted byte of image memory.
			U8* allocationBaseAddress = section.baseAddress + align(section.numCommittedBytes,alignment);
			wavmAssert(!(reinterpret_cast<Uptr>(allocationBaseAddress) & (alignment-1)));
			section.numCommittedBytes = align(section.numCommittedBytes,alignment) + align(numBytes,alignment);

			// Check that enough space was reserved in the section.
			if(section.numCommittedBytes > (section.numPages << Platform::getPageSizeLog2())) { Errors::fatal("didn't reserve enough space in section"); }

			return allocationBaseAddress;
		}
		
		static Uptr align(Uptr size,Uptr alignment) { return (size + alignment - 1) & ~(alignment - 1); }
		static Uptr shrAndRoundUp(Uptr value,Uptr shift) { return (value + (Uptr(1)<<shift) - 1) >> shift; }

		UnitMemoryManager(const UnitMemoryManager&) = delete;
		void operator=(const UnitMemoryManager&) = delete;
	};

	// A unit of JIT compilation.
	// Encapsulates the LLVM JIT compilation pipeline but allows subclasses to define how the resulting code is used.
	struct JITUnit
	{
		JITUnit(bool inShouldLogMetrics = true)
		: shouldLogMetrics(inShouldLogMetrics)
		#ifdef _WIN32
			, pdataCopy(nullptr)
			, xdataCopy(nullptr)
		#endif
		{
			memoryManager = std::make_shared<UnitMemoryManager>();
			objectLayer = llvm::make_unique<ObjectLayer>(
				[this]() { return this->memoryManager; },
				NotifyLoadedFunctor(this),
				NotifyFinalizedFunctor(this));
			#ifndef _WIN64
				objectLayer->setProcessAllSections(true);
			#endif
			compileLayer = llvm::make_unique<CompileLayer>(
				*objectLayer,
				llvm::orc::SimpleCompiler(*targetMachine));
		}
		~JITUnit()
		{
			cantFail(compileLayer->removeModule(handle));
			#ifdef _WIN64
				if(pdataCopy) { Platform::deregisterSEHUnwindInfo(reinterpret_cast<Uptr>(pdataCopy)); }
			#endif
		}

		void compile(const std::shared_ptr<llvm::Module>& llvmModule);

		virtual void notifySymbolLoaded(const char* name,Uptr baseAddress,Uptr numBytes,std::map<U32,U32>&& offsetToOpIndexMap) = 0;

	private:
		
		// Functor that receives notifications when an object produced by the JIT is loaded.
		struct NotifyLoadedFunctor
		{
			JITUnit* jitUnit;
			NotifyLoadedFunctor(JITUnit* inJITUnit): jitUnit(inJITUnit) {}
			void operator()(
				llvm::orc::RTDyldObjectLinkingLayer::ObjHandleT objectHandle,
				const std::shared_ptr<llvm::object::OwningBinary<llvm::object::ObjectFile>>& object,
				const llvm::RuntimeDyld::LoadedObjectInfo& loadedObject
				);
		};
		
		// Functor that receives notifications when an object produced by the JIT is finalized.
		struct NotifyFinalizedFunctor
		{
			JITUnit* jitUnit;
			NotifyFinalizedFunctor(JITUnit* inJITUnit): jitUnit(inJITUnit) {}
			void operator()(const llvm::orc::RTDyldObjectLinkingLayerBase::ObjHandleT& objectHandle);
		};
		typedef llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
		typedef llvm::orc::IRCompileLayer<ObjectLayer,llvm::orc::SimpleCompiler> CompileLayer;

		std::shared_ptr<UnitMemoryManager> memoryManager;
		std::unique_ptr<ObjectLayer> objectLayer;
		std::unique_ptr<CompileLayer> compileLayer;
		CompileLayer::ModuleHandleT handle;
		bool shouldLogMetrics;

		struct LoadedObject
		{
			llvm::object::ObjectFile* object;
			const llvm::RuntimeDyld::LoadedObjectInfo* loadedObject;
		};

		std::vector<LoadedObject> loadedObjects;

		#ifdef _WIN32
			llvm::object::SectionRef pdataSection;
			U8* pdataCopy;
			Uptr pdataNumBytes;
			
			llvm::object::SectionRef xdataSection;
			U8* xdataCopy;
			Uptr xdataNumBytes;

			Uptr sehTrampolineAddress;
		#endif
	};

	// The JIT compilation unit for a WebAssembly module instance.
	struct JITModule : JITUnit, JITModuleBase
	{
		ModuleInstance* moduleInstance;

		std::vector<JITSymbol*> functionDefSymbols;

		JITModule(ModuleInstance* inModuleInstance): moduleInstance(inModuleInstance) {}
		~JITModule() override
		{
			// Delete the module's symbols, and remove them from the global address-to-symbol map.
			Platform::Lock addressToSymbolMapLock(addressToSymbolMapMutex);
			for(auto symbol : functionDefSymbols)
			{
				addressToSymbolMap.erase(addressToSymbolMap.find(symbol->baseAddress + symbol->numBytes));
				delete symbol;
			}
		}

		void notifySymbolLoaded(const char* name,Uptr baseAddress,Uptr numBytes,std::map<U32,U32>&& offsetToOpIndexMap) override
		{
			// Save the address range this function was loaded at for future address->symbol lookups.
			Uptr functionDefIndex;
			if(getFunctionIndexFromExternalName(name,functionDefIndex))
			{
				wavmAssert(moduleInstance);
				wavmAssert(functionDefIndex < moduleInstance->functionDefs.size());
				FunctionInstance* functionInstance = moduleInstance->functionDefs[functionDefIndex];
				auto symbol = new JITSymbol(functionInstance,baseAddress,numBytes,std::move(offsetToOpIndexMap));
				functionDefSymbols.push_back(symbol);
				functionInstance->nativeFunction = reinterpret_cast<void*>(baseAddress);

				{
					Platform::Lock addressToSymbolMapLock(addressToSymbolMapMutex);
					addressToSymbolMap[baseAddress + numBytes] = symbol;
				}
			}
		}
	};

	// The JIT compilation unit for a single invoke thunk.
	struct JITThunkUnit : JITUnit
	{
		const FunctionType* functionType;

		JITSymbol* symbol;

		JITThunkUnit(const FunctionType* inFunctionType): JITUnit(false), functionType(inFunctionType), symbol(nullptr) {}

		void notifySymbolLoaded(const char* name,Uptr baseAddress,Uptr numBytes,std::map<U32,U32>&& offsetToOpIndexMap) override
		{
			#if (defined(_WIN32) && !defined(_WIN64))
				wavmAssert(!strcmp(name,"_thunk"));
			#else
				wavmAssert(!strcmp(name,"thunk"));
			#endif
			symbol = new JITSymbol(functionType,baseAddress,numBytes,std::move(offsetToOpIndexMap));
		}
	};
	
	static std::map<std::string,const char*> runtimeSymbolMap =
	{
		#ifdef _WIN32
			// the LLVM X86 code generator calls __chkstk when allocating more than 4KB of stack space
			{"__chkstk","__chkstk"},
			{"__C_specific_handler","__C_specific_handler"},
			#ifndef _WIN64
			{"__aullrem","_aullrem"},
			{"__allrem","_allrem"},
			{"__aulldiv","_aulldiv"},
			{"__alldiv","_alldiv"},
			#endif
		#else
			{"__CxxFrameHandler3","__CxxFrameHandler3"},
			{"__cxa_begin_catch","__cxa_begin_catch"},
			{"__gxx_personality_v0","__gxx_personality_v0"},
		#endif
		#ifdef __arm__
		{"__aeabi_uidiv","__aeabi_uidiv"},
		{"__aeabi_idiv","__aeabi_idiv"},
		{"__aeabi_idivmod","__aeabi_idivmod"},
		{"__aeabi_uldiv","__aeabi_uldiv"},
		{"__aeabi_uldivmod","__aeabi_uldivmod"},
		{"__aeabi_unwind_cpp_pr0","__aeabi_unwind_cpp_pr0"},
		{"__aeabi_unwind_cpp_pr1","__aeabi_unwind_cpp_pr1"},
		#endif
	};

	std::shared_ptr<NullResolver> NullResolver::singleton = std::make_shared<NullResolver>();
	
	llvm::JITSymbol NullResolver::findSymbol(const std::string& name)
	{
		// Allow some intrinsics used by LLVM
		auto runtimeSymbolNameIt = runtimeSymbolMap.find(name);
		if(runtimeSymbolNameIt != runtimeSymbolMap.end())
		{
			const char* lookupName = runtimeSymbolNameIt->second;
			void *addr = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(lookupName);
			if(!addr) { Errors::fatalf("LLVM generated code references undefined external symbol: %s\n",lookupName); }
			return llvm::JITSymbol(reinterpret_cast<Uptr>(addr),llvm::JITSymbolFlags::None);
		}

		Errors::fatalf("LLVM generated code references disallowed external symbol: %s\n",name.c_str());
	}
	llvm::JITSymbol NullResolver::findSymbolInLogicalDylib(const std::string& name) { return llvm::JITSymbol(nullptr); }
	
	void JITUnit::NotifyLoadedFunctor::operator()(
		llvm::orc::RTDyldObjectLinkingLayerBase::ObjHandleT objectHandle,
		const std::shared_ptr<llvm::object::OwningBinary<llvm::object::ObjectFile>>& object,
		const llvm::RuntimeDyld::LoadedObjectInfo& loadedObject
		)
	{
		// Make a copy of the loaded object info for use by the finalizer.
		jitUnit->loadedObjects.push_back(LoadedObject {object.get()->getBinary(),&loadedObject});

		if(jitUnit->shouldLogMetrics && DUMP_OBJECT)
		{
			// Dump the object file.
			std::error_code errorCode;
			static Uptr dumpedObjectId = 0;
			std::string augmentedFilename = std::string("jitObject") + std::to_string(dumpedObjectId++) + ".o";
			llvm::raw_fd_ostream dumpFileStream(augmentedFilename,errorCode,llvm::sys::fs::OpenFlags::F_None);
			dumpFileStream.write(
				(const char*)object->getBinary()->getData().bytes_begin(),
				object->getBinary()->getData().size());
			Log::printf(Log::Category::debug,"Dumped object file to: %s\n",augmentedFilename.c_str()); 
		}

		#ifdef _WIN64
			// The LLVM dynamic loader doesn't correctly apply the IMAGE_REL_AMD64_ADDR32NB relocations
			// in the pdata and xdata sections (https://github.com/llvm-mirror/llvm/blob/e84d8c12d5157a926db15976389f703809c49aa5/lib/ExecutionEngine/RuntimeDyld/Targets/RuntimeDyldCOFFX86_64.h#L96)
			// Make a copy of those sections before they are clobbered, so we can do the fixup ourselves later.
			for(auto section : object->getBinary()->sections())
			{
				llvm::StringRef sectionName;
				if(!section.getName(sectionName))
				{
					const U8* loadedSection = reinterpret_cast<U8*>(Uptr(loadedObject.getSectionLoadAddress(section)));
					if(sectionName == ".pdata")
					{
						jitUnit->pdataCopy = new U8[section.getSize()];
						jitUnit->pdataNumBytes = section.getSize();
						jitUnit->pdataSection = section;
						memcpy(jitUnit->pdataCopy,loadedSection,section.getSize());
				}
					else if(sectionName == ".xdata")
					{
						jitUnit->xdataCopy = new U8[section.getSize()];
						jitUnit->xdataNumBytes = section.getSize();
						jitUnit->xdataSection = section;
						memcpy(jitUnit->xdataCopy,loadedSection,section.getSize());
			}
				}
			}
				
			// Create a trampoline within the image's 2GB address space that jumps to __C_specific_handler.
			// jmp [rip+0]
			// <64-bit address>
			U8* trampolineBytes = jitUnit->memoryManager->allocateCodeSection(16,16,0,"seh_trampoline");
			trampolineBytes[0] = 0xff;
			trampolineBytes[1] = 0x25;
			*(U32*)&trampolineBytes[2] = 0;
			*(U64*)&trampolineBytes[6] = U64(cantFail(NullResolver::singleton->findSymbol("__C_specific_handler").getAddress()));
			jitUnit->sehTrampolineAddress = reinterpret_cast<Uptr>(trampolineBytes);
		#endif
	}

	#if PRINT_DISASSEMBLY
	void disassembleFunction(U8* bytes,Uptr numBytes)
	{
		LLVMDisasmContextRef disasmRef = LLVMCreateDisasm(llvm::sys::getProcessTriple().c_str(),nullptr,0,nullptr,nullptr);

		U8* nextByte = bytes;
		Uptr numBytesRemaining = numBytes;
		while(numBytesRemaining)
		{
			char instructionBuffer[256];
			Uptr numInstructionBytes = LLVMDisasmInstruction(
				disasmRef,
				nextByte,
				numBytesRemaining,
				reinterpret_cast<Uptr>(nextByte),
				instructionBuffer,
				sizeof(instructionBuffer)
				);
			if(numInstructionBytes == 0) { numInstructionBytes = 1; }
			wavmAssert(numInstructionBytes <= numBytesRemaining);
			numBytesRemaining -= numInstructionBytes;
			nextByte += numInstructionBytes;

			Log::printf(Log::Category::error,"\t\t0x%04x %s\n",(nextByte - bytes - numInstructionBytes),instructionBuffer);
		};

		LLVMDisasmDispose(disasmRef);
	}
	#endif

	void JITUnit::NotifyFinalizedFunctor::operator()(const llvm::orc::RTDyldObjectLinkingLayerBase::ObjHandleT& objectHandle)
	{
		for(Uptr objectIndex = 0;objectIndex < jitUnit->loadedObjects.size();++objectIndex)
		{
			llvm::object::ObjectFile* object = jitUnit->loadedObjects[objectIndex].object;
			const llvm::RuntimeDyld::LoadedObjectInfo* loadedObject = jitUnit->loadedObjects[objectIndex].loadedObject;

			// Notify GDB of the new object.
			gdbRegistrationListener->NotifyObjectEmitted(*object,*loadedObject);
			
			// Create a DWARF context to interpret the debug information in this compilation unit.
#if LLVM_VERSION_MAJOR < 6
			auto dwarfContext = llvm::make_unique<llvm::DWARFContextInMemory>(*object,loadedObject);
#else
			auto dwarfContext = llvm::DWARFContext::create(*object,loadedObject);
#endif

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
				{
					loadedAddress += (Uptr)loadedObject->getSectionLoadAddress(*symbolSection.get());
				}

				// Get the DWARF line info for this symbol, which maps machine code addresses to WebAssembly op indices.
				llvm::DILineInfoTable lineInfoTable = dwarfContext->getLineInfoForAddressRange(loadedAddress, symbolSizePair.second);
				std::map<U32, U32> offsetToOpIndexMap;
				for (auto lineInfo : lineInfoTable) { offsetToOpIndexMap.emplace(U32(lineInfo.first - loadedAddress), lineInfo.second.Line); }

				#if PRINT_DISASSEMBLY
				if(jitUnit->shouldLogMetrics)
				{
					Log::printf(Log::Category::error,"Disassembly for function %s\n",name.get().data());
					disassembleFunction(reinterpret_cast<U8*>(loadedAddress),Uptr(symbolSizePair.second));
				}
				#endif

				// Notify the JIT unit that the symbol was loaded.
				wavmAssert(symbolSizePair.second <= UINTPTR_MAX);
				jitUnit->notifySymbolLoaded(
				name->data(), loadedAddress,
					Uptr(symbolSizePair.second),
					std::move(offsetToOpIndexMap)
					);
			}
		
			#ifdef _WIN64
			processSEHTables(
				reinterpret_cast<Uptr>(jitUnit->memoryManager->getImageBaseAddress()),
				loadedObject,
				jitUnit->pdataSection,jitUnit->pdataCopy,jitUnit->pdataNumBytes,
				jitUnit->xdataSection,jitUnit->xdataCopy,
				jitUnit->sehTrampolineAddress
				);
			if(jitUnit->pdataCopy) { delete [] jitUnit->pdataCopy; jitUnit->pdataCopy = nullptr; }
			if(jitUnit->xdataCopy) { delete [] jitUnit->xdataCopy; jitUnit->xdataCopy = nullptr; }
			#endif
		}

		jitUnit->loadedObjects.clear();
	}

	static Uptr printedModuleId = 0;

	void printModule(const llvm::Module* llvmModule,const char* filename)
	{
		std::error_code errorCode;
		std::string augmentedFilename = std::string(filename) + std::to_string(printedModuleId++) + ".ll";
		llvm::raw_fd_ostream dumpFileStream(augmentedFilename,errorCode,llvm::sys::fs::OpenFlags::F_Text);
		llvmModule->print(dumpFileStream,nullptr);
		Log::printf(Log::Category::debug,"Dumped LLVM module to: %s\n",augmentedFilename.c_str());
	}

	void JITUnit::compile(const std::shared_ptr<llvm::Module>& llvmModule)
	{
		// Get a target machine object for this host, and set the module to use its data layout.
		llvmModule->setDataLayout(targetMachine->createDataLayout());

		// Verify the module.
		if(shouldLogMetrics && DUMP_UNOPTIMIZED_MODULE) { printModule(llvmModule.get(),"llvmDump"); }
		if(shouldLogMetrics && VERIFY_MODULE)
		{
			std::string verifyOutputString;
			llvm::raw_string_ostream verifyOutputStream(verifyOutputString);
			if(llvm::verifyModule(*llvmModule,&verifyOutputStream))
			{ verifyOutputStream.flush(); Errors::fatalf("LLVM verification errors:\n%s\n",verifyOutputString.c_str()); }
			Log::printf(Log::Category::debug,"Verified LLVM module\n");
		}

		// Run some optimization on the module's functions.
		Timing::Timer optimizationTimer;

		auto fpm = new llvm::legacy::FunctionPassManager(llvmModule.get());
		fpm->add(llvm::createPromoteMemoryToRegisterPass());
		fpm->add(llvm::createInstructionCombiningPass());
		fpm->add(llvm::createCFGSimplificationPass());
		fpm->add(llvm::createJumpThreadingPass());
		fpm->add(llvm::createConstantPropagationPass());
		fpm->doInitialization();
		for(auto functionIt = llvmModule->begin();functionIt != llvmModule->end();++functionIt)
		{ fpm->run(*functionIt); }
		delete fpm;
		
		if(shouldLogMetrics)
		{
			Timing::logRatePerSecond("Optimized LLVM module",optimizationTimer,(F64)llvmModule->size(),"functions");
		}

		if(shouldLogMetrics && DUMP_OPTIMIZED_MODULE) { printModule(llvmModule.get(),"llvmOptimizedDump"); }

		// Pass the module to the JIT compiler.
		Timing::Timer machineCodeTimer;
		handle = cantFail(compileLayer->addModule(
			llvmModule,
			NullResolver::singleton));
		cantFail(compileLayer->emitAndFinalize(handle));

		if(shouldLogMetrics)
		{
			Timing::logRatePerSecond("Generated machine code",machineCodeTimer,(F64)llvmModule->size(),"functions");
		}
	}

	void instantiateModule(const IR::Module& module,ModuleInstance* moduleInstance)
	{
		Platform::Lock llvmLock(llvmMutex);

		initLLVM();

		// Emit LLVM IR for the module.
		auto llvmModule = emitModule(module,moduleInstance);

		// Construct the JIT compilation pipeline for this module.
		auto jitModule = new JITModule(moduleInstance);
		moduleInstance->jitModule = jitModule;

		// Compile the module.
		jitModule->compile(llvmModule);
	}

	std::string getExternalFunctionName(ModuleInstance* moduleInstance,Uptr functionDefIndex)
	{
		wavmAssert(functionDefIndex < moduleInstance->functionDefs.size());
		return "wasmFunc" + std::to_string(functionDefIndex)
			+ "_" + moduleInstance->functionDefs[functionDefIndex]->debugName;
	}

	bool getFunctionIndexFromExternalName(const char* externalName,Uptr& outFunctionDefIndex)
	{
		#if (defined(_WIN32) && !defined(_WIN64))
			const char wasmFuncPrefix[] = "_wasmFunc";
		#else
			const char wasmFuncPrefix[] = "wasmFunc";
		#endif
		const Uptr numPrefixChars = sizeof(wasmFuncPrefix) - 1;
		if(!strncmp(externalName,wasmFuncPrefix,numPrefixChars))
		{
			char* numberEnd = nullptr;
			U64 functionDefIndex64 = std::strtoull(externalName + numPrefixChars,&numberEnd,10);
			if(functionDefIndex64 > UINTPTR_MAX) { return false; }
			outFunctionDefIndex = Uptr(functionDefIndex64);
			return true;
		}
		else { return false; }
	}

	bool describeInstructionPointer(Uptr ip,std::string& outDescription)
	{
		JITSymbol* symbol;
		{
			Platform::Lock addressToSymbolMapLock(addressToSymbolMapMutex);
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
			
			// Find the highest entry in the offsetToOpIndexMap whose offset is <= the symbol-relative IP.
			U32 ipOffset = (U32)(ip - symbol->baseAddress);
			Iptr opIndex = -1;
			for(auto offsetMapIt : symbol->offsetToOpIndexMap)
			{
				if(offsetMapIt.first <= ipOffset) { opIndex = offsetMapIt.second; }
				else { break; }
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

	InvokeFunctionPointer getInvokeThunk(const FunctionType* functionType,CallingConvention callingConvention)
	{
		Platform::Lock llvmLock(llvmMutex);

		initLLVM();

		// Reuse cached invoke thunks for the same function type.
		JITSymbol*& invokeThunkSymbol = invokeThunkTypeToSymbolMap.getOrAdd(functionType,nullptr);
		if(invokeThunkSymbol) { return reinterpret_cast<InvokeFunctionPointer>(invokeThunkSymbol->baseAddress); }

		auto llvmModuleSharedPtr = std::make_shared<llvm::Module>("",*llvmContext);
		auto llvmModule = llvmModuleSharedPtr.get();
		auto llvmFunctionType = llvm::FunctionType::get(
			llvmI8PtrType,
			{
				asLLVMType(functionType,callingConvention)->getPointerTo(),
				llvmI8PtrType
			},
			false);
		auto llvmFunction = llvm::Function::Create(llvmFunctionType,llvm::Function::ExternalLinkage,"thunk",llvmModule);
		llvm::Value* functionPointer = &*(llvmFunction->args().begin() + 0);
		llvm::Value* contextPointer = &*(llvmFunction->args().begin() + 1);

		EmitContext emitContext(nullptr,nullptr);
		emitContext.irBuilder.SetInsertPoint(llvm::BasicBlock::Create(*llvmContext,"entry",llvmFunction));

		emitContext.contextPointerVariable = emitContext.irBuilder.CreateAlloca(llvmI8PtrType);
		emitContext.irBuilder.CreateStore(contextPointer,emitContext.contextPointerVariable);

		// Load the function's arguments from an array of 64-bit values at an address provided by the caller.
		std::vector<llvm::Value*> arguments;
		Uptr argDataOffset = 0;
		for(Uptr parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
		{
			ValueType parameterType = functionType->parameters[parameterIndex];
			if(parameterType == ValueType::v128)
			{
				// Use 16-byte alignment for V128 arguments.
				argDataOffset = (argDataOffset + 15) & ~15;
			}

			arguments.push_back(emitContext.loadFromUntypedPointer(
					emitContext.irBuilder.CreateInBoundsGEP(
						contextPointer,
						{emitLiteral(argDataOffset + offsetof(ContextRuntimeData,thunkArgAndReturnData))}),
					asLLVMType(parameterType)));

			argDataOffset += parameterType == ValueType::v128 ? 16 : 8;
		}

		// Call the function.
		llvm::Value* result = emitContext.emitCallOrInvoke(functionPointer,arguments,functionType,callingConvention);

		// If the function has a return value, write it to the context invoke return memory.
		if(functionType->ret != ResultType::none)
		{
			auto llvmResultType = asLLVMType(functionType->ret);
			emitContext.irBuilder.CreateStore(
				result,
				emitContext.irBuilder.CreatePointerCast(
					emitContext.irBuilder.CreateInBoundsGEP(
						emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable),
						{emitLiteral(U64(offsetof(ContextRuntimeData,thunkArgAndReturnData)))}
						),
					llvmResultType->getPointerTo()
					)
				);
		}

		emitContext.irBuilder.CreateRet(emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable));

		// Compile the invoke thunk.
		auto jitUnit = new JITThunkUnit(functionType);
		jitUnit->compile(llvmModuleSharedPtr);

		wavmAssert(jitUnit->symbol);
		invokeThunkSymbol = jitUnit->symbol;

		{
			Platform::Lock addressToSymbolMapLock(addressToSymbolMapMutex);
			addressToSymbolMap[jitUnit->symbol->baseAddress + jitUnit->symbol->numBytes] = jitUnit->symbol;
		}

		return reinterpret_cast<InvokeFunctionPointer>(invokeThunkSymbol->baseAddress);
	}

	void* getIntrinsicThunk(void* nativeFunction,const FunctionType* functionType,CallingConvention callingConvention)
	{
		wavmAssert(callingConvention == CallingConvention::intrinsic
		|| callingConvention == CallingConvention::intrinsicWithContextSwitch
		|| callingConvention == CallingConvention::intrinsicWithMemAndTable);

		Platform::Lock llvmLock(llvmMutex);

		initLLVM();

		// Reuse cached intrinsic thunks for the same function type.
		JITSymbol*& intrinsicThunkSymbol = intrinsicFunctionToThunkSymbolMap.getOrAdd(nativeFunction, nullptr);
		if(intrinsicThunkSymbol) { return reinterpret_cast<void*>(intrinsicThunkSymbol->baseAddress); }

		// Create a LLVM module containing a single function with the same signature as the native
		// function, but with the WASM calling convention.
		auto llvmModuleSharedPtr = std::make_shared<llvm::Module>("",*llvmContext);
		auto llvmModule = llvmModuleSharedPtr.get();
		auto llvmFunctionType = asLLVMType(functionType,CallingConvention::wasm);
		auto llvmFunction = llvm::Function::Create(llvmFunctionType,llvm::Function::ExternalLinkage,"thunk",llvmModule);
		llvmFunction->setCallingConv(asLLVMCallingConv(callingConvention));

		EmitContext emitContext(nullptr,nullptr);
		emitContext.irBuilder.SetInsertPoint(llvm::BasicBlock::Create(*llvmContext,"entry",llvmFunction));

		emitContext.contextPointerVariable = emitContext.irBuilder.CreateAlloca(llvmI8PtrType);
		emitContext.irBuilder.CreateStore(&*llvmFunction->args().begin(),emitContext.contextPointerVariable);

		llvm::SmallVector<llvm::Value*,8> args;
		for(auto argIt = llvmFunction->args().begin() + 1;
			argIt != llvmFunction->args().end();
			++argIt)
		{
			args.push_back(&*argIt);
		}

		llvm::Type* llvmNativeFunctionType = asLLVMType(functionType,callingConvention)->getPointerTo();
		llvm::Value* llvmNativeFunction = emitLiteralPointer(nativeFunction,llvmNativeFunctionType);
		llvm::Value* result = emitContext.emitCallOrInvoke(llvmNativeFunction,args,functionType,callingConvention);

		// Package the context pointer and result in a struct.
		llvm::Value* thunkReturnStruct = getZeroedLLVMReturnStruct(functionType->ret);
		llvm::Value* contextPointer = emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable);
		thunkReturnStruct = emitContext.irBuilder.CreateInsertValue(thunkReturnStruct,contextPointer,{0});
		if(functionType->ret != ResultType::none)
		{
			thunkReturnStruct = emitContext.irBuilder.CreateInsertValue(thunkReturnStruct,result,{1});
		}

		emitContext.irBuilder.CreateRet(thunkReturnStruct);

		// Compile the LLVM IR to machine code.
		auto jitUnit = new JITThunkUnit(functionType);
		jitUnit->compile(llvmModuleSharedPtr);

		wavmAssert(jitUnit->symbol);
		intrinsicThunkSymbol = jitUnit->symbol;

		{
			Platform::Lock addressToSymbolMapLock(addressToSymbolMapMutex);
			addressToSymbolMap[jitUnit->symbol->baseAddress + jitUnit->symbol->numBytes] = jitUnit->symbol;
		}

		return reinterpret_cast<void*>(intrinsicThunkSymbol->baseAddress);
	}

	void initLLVM()
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
			// Didn't figure out exactly why, but this works around a problem with the MacOS dynamic loader. Without it,
			// our symbols can't be found in the JITed object file.
			targetTriple += "-elf";
		#endif
		llvm::SmallVector<std::string,0> machineAttrs = { LLVM_TARGET_ATTRIBUTES };
		targetMachine = llvm::EngineBuilder().selectTarget(
			llvm::Triple(targetTriple),"",llvm::sys::getHostCPUName(),machineAttrs);

		llvmI8Type = llvm::Type::getInt8Ty(*llvmContext);
		llvmI16Type = llvm::Type::getInt16Ty(*llvmContext);
		llvmI32Type = llvm::Type::getInt32Ty(*llvmContext);
		llvmI64Type = llvm::Type::getInt64Ty(*llvmContext);
		llvmF32Type = llvm::Type::getFloatTy(*llvmContext);
		llvmF64Type = llvm::Type::getDoubleTy(*llvmContext);
		llvmVoidType = llvm::Type::getVoidTy(*llvmContext);
		llvmBoolType = llvm::Type::getInt1Ty(*llvmContext);
		llvmI8PtrType = llvmI8Type->getPointerTo();
		
		#if defined(_WIN64)
		auto llvmExceptionRecordStructType = llvm::StructType::create({
			llvmI32Type, // DWORD ExceptionCode
			llvmI32Type, // DWORD ExceptionFlags
			llvmI8PtrType, // _EXCEPTION_RECORD* ExceptionRecord
			llvmI8PtrType, // PVOID ExceptionAddress
			llvmI32Type, // DWORD NumParameters
			llvm::ArrayType::get(llvmI64Type,15) // ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS]
			});
		llvmExceptionPointersStructType = llvm::StructType::create(
			{llvmExceptionRecordStructType->getPointerTo(),llvmI8PtrType});
		#endif
		
		llvmI8x16Type = llvm::VectorType::get(llvmI8Type,16);
		llvmI16x8Type = llvm::VectorType::get(llvmI16Type,8);
		llvmI32x4Type = llvm::VectorType::get(llvmI32Type,4);
		llvmI64x2Type = llvm::VectorType::get(llvmI64Type,2);
		llvmF32x4Type = llvm::VectorType::get(llvmF32Type,4);
		llvmF64x2Type = llvm::VectorType::get(llvmF64Type,2);

		llvmResultTypes[(Uptr)ResultType::none] = llvm::Type::getVoidTy(*llvmContext);
		llvmResultTypes[(Uptr)ResultType::i32] = llvmI32Type;
		llvmResultTypes[(Uptr)ResultType::i64] = llvmI64Type;
		llvmResultTypes[(Uptr)ResultType::f32] = llvmF32Type;
		llvmResultTypes[(Uptr)ResultType::f64] = llvmF64Type;
		llvmResultTypes[(Uptr)ResultType::v128] = llvmI64x2Type;

		// Create zero constants of each type.
		typedZeroConstants[(Uptr)ValueType::any] = nullptr;
		typedZeroConstants[(Uptr)ValueType::i32] = emitLiteral((U32)0);
		typedZeroConstants[(Uptr)ValueType::i64] = emitLiteral((U64)0);
		typedZeroConstants[(Uptr)ValueType::f32] = emitLiteral((F32)0.0f);
		typedZeroConstants[(Uptr)ValueType::f64] = emitLiteral((F64)0.0);
		typedZeroConstants[(Uptr)ValueType::v128] = llvm::ConstantVector::get({typedZeroConstants[(Uptr)ValueType::i64],typedZeroConstants[(Uptr)ValueType::i64]});

		gdbRegistrationListener = llvm::JITEventListener::createGDBRegistrationListener();
	}
}
