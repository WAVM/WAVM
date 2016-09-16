#include "LLVMJIT.h"

// This needs to be 1 to allow debuggers such as Visual Studio to place breakpoints and step through the JITed code.
#define USE_WRITEABLE_JIT_CODE_PAGES _DEBUG

#define DUMP_UNOPTIMIZED_MODULE _DEBUG
#define VERIFY_MODULE _DEBUG
#define DUMP_OPTIMIZED_MODULE _DEBUG

namespace LLVMJIT
{
	llvm::LLVMContext& context = llvm::getGlobalContext();
	llvm::TargetMachine* targetMachine = nullptr;
	llvm::Type* llvmResultTypes[(size_t)ResultType::num];
	llvm::Type* llvmI8Type;
	llvm::Type* llvmI16Type;
	llvm::Type* llvmI32Type;
	llvm::Type* llvmI64Type;
	llvm::Type* llvmF32Type;
	llvm::Type* llvmF64Type;
	llvm::Type* llvmVoidType;
	llvm::Type* llvmBoolType;
	llvm::Type* llvmI8PtrType;
	llvm::Constant* typedZeroConstants[(size_t)ValueType::num];
	
	// A map from address to loaded JIT symbols.
	std::map<uintptr,struct JITSymbol*> addressToSymbolMap;

	// A map from function types to function indices in the invoke thunk unit.
	std::map<const FunctionType*,struct JITSymbol*> invokeThunkTypeToSymbolMap;

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
		uintptr baseAddress;
		size_t numBytes;
		
		JITSymbol(FunctionInstance* inFunctionInstance,uintptr inBaseAddress,size_t inNumBytes)
		: type(Type::functionInstance), functionInstance(inFunctionInstance), baseAddress(inBaseAddress), numBytes(inNumBytes) {}

		JITSymbol(const FunctionType* inInvokeThunkType,uintptr inBaseAddress,size_t inNumBytes)
		: type(Type::invokeThunk), invokeThunkType(inInvokeThunkType), baseAddress(inBaseAddress), numBytes(inNumBytes) {}
	};

	// A unit of JIT compilation.
	// Encapsulates the LLVM JIT compilation pipeline but allows subclasses to define how the resulting code is used.
	struct JITUnit
	{
		JITUnit()
		{
			objectLayer = llvm::make_unique<ObjectLayer>(NotifyLoadedFunctor(this));
			compileLayer = llvm::make_unique<CompileLayer>(*objectLayer,llvm::orc::SimpleCompiler(*targetMachine));
		}

		bool compile(llvm::Module* llvmModule);

		virtual void notifySymbolLoaded(const char* name,uintptr baseAddress,size_t numBytes) = 0;

	private:
		
		// Functor that receives notifications when an object produced by the JIT is loaded.
		struct NotifyLoadedFunctor
		{
			JITUnit* jitUnit;
			NotifyLoadedFunctor(JITUnit* inJITUnit): jitUnit(inJITUnit) {}
			void operator()(
				const llvm::orc::ObjectLinkingLayerBase::ObjSetHandleT& objectSetHandle,
				const std::vector<std::unique_ptr<llvm::object::ObjectFile>>& objectSet,
				const std::vector<std::unique_ptr<llvm::RuntimeDyld::LoadedObjectInfo>>& loadResult
				);
		};

		typedef llvm::orc::ObjectLinkingLayer<NotifyLoadedFunctor> ObjectLayer;
		typedef llvm::orc::IRCompileLayer<ObjectLayer> CompileLayer;

		std::unique_ptr<ObjectLayer> objectLayer;
		std::unique_ptr<CompileLayer> compileLayer;
		CompileLayer::ModuleSetHandleT handle;
	};

	// The JIT compilation unit for a WebAssembly module instance.
	struct JITModule : JITUnit
	{
		ModuleInstance* moduleInstance;

		std::vector<JITSymbol*> functionDefSymbols;

		JITModule(ModuleInstance* inModuleInstance): moduleInstance(inModuleInstance) {}

		void notifySymbolLoaded(const char* name,uintptr baseAddress,size_t numBytes) override
		{
			// Save the address range this function was loaded at for future address->symbol lookups.
			uintptr functionDefIndex;
			if(getFunctionIndexFromExternalName(name,functionDefIndex))
			{
				assert(moduleInstance);
				assert(functionDefIndex < moduleInstance->functionDefs.size());
				FunctionInstance* functionInstance = moduleInstance->functionDefs[functionDefIndex];
				auto symbol = new JITSymbol(functionInstance,baseAddress,numBytes);
				functionDefSymbols.push_back(symbol);
				addressToSymbolMap[baseAddress + numBytes] = symbol;
				functionInstance->nativeFunction = reinterpret_cast<void*>(baseAddress);
			}
		}
	};

	// The JIT compilation unit for a single invoke thunk.
	struct JITInvokeThunkUnit : JITUnit
	{
		const FunctionType* functionType;

		JITSymbol* symbol;

		JITInvokeThunkUnit(const FunctionType* inFunctionType): functionType(inFunctionType), symbol(nullptr) {}

		void notifySymbolLoaded(const char* name,uintptr baseAddress,size_t numBytes) override
		{
			assert(!strcmp(name,"invokeThunk"));
			symbol = new JITSymbol(functionType,baseAddress,numBytes);
		}
	};
	
	// Used to override LLVM's default behavior of looking up unresolved symbols in DLL exports.
	struct NullResolver : llvm::RuntimeDyld::SymbolResolver
	{
		static NullResolver singleton;
		virtual llvm::RuntimeDyld::SymbolInfo findSymbol(const std::string& name) override;
		virtual llvm::RuntimeDyld::SymbolInfo findSymbolInLogicalDylib(const std::string& name) override;
	};
	
	NullResolver NullResolver::singleton;
	llvm::RuntimeDyld::SymbolInfo NullResolver::findSymbol(const std::string& name) { throw InstantiationException(InstantiationException::Cause::codeGenFailed); }
	llvm::RuntimeDyld::SymbolInfo NullResolver::findSymbolInLogicalDylib(const std::string& name) { throw InstantiationException(InstantiationException::Cause::codeGenFailed); }
	
	// Allocates memory for the LLVM object loader.
	struct SectionMemoryManager : llvm::RTDyldMemoryManager
	{
		static SectionMemoryManager singleton;

		SectionMemoryManager()
		{
			// Allocate 2GB of virtual pages for the image up front. This ensures that no address within the image
			// is more than a 32-bit offset from any other address in the image. LLVM on Windows produces COFF
			// files that assume the entire image can be addressed by a 32-bit offset, but the built-in LLVM
			// SectionMemoryManager doesn't fulfill that assumption.
			numAllocatedImagePages = ((size_t)1) << (31 - Platform::getPageSizeLog2());
			imageBaseAddress = Platform::allocateVirtualPages(numAllocatedImagePages);
			numCommittedImagePages = 0;
		}
		virtual ~SectionMemoryManager() override
		{
			// Free the allocated and possibly committed image pages.
			Platform::freeVirtualPages(imageBaseAddress,numAllocatedImagePages);
		}

		virtual uint8* allocateCodeSection(uintptr_t numBytes,uint32 alignment,uint32 sectionID,llvm::StringRef sectionName) override
		{
			auto finalAccess = USE_WRITEABLE_JIT_CODE_PAGES ? Platform::MemoryAccess::ReadWriteExecute : Platform::MemoryAccess::Execute;
			return allocateSection((uintptr)numBytes,alignment,finalAccess);
		}
		virtual uint8* allocateDataSection(uintptr_t numBytes,uint32 alignment,uint32 sectionID,llvm::StringRef SectionName,bool isReadOnly) override
		{
			return allocateSection((uintptr)numBytes,alignment,isReadOnly ? Platform::MemoryAccess::ReadOnly : Platform::MemoryAccess::ReadWrite);
		}
		virtual bool finalizeMemory(std::string* ErrMsg = nullptr) override
		{
			// Set the requested final memory access for each section's pages.
			for(auto section : sections)
			{
				if(!section.isFinalized)
				{
					if(!Platform::setVirtualPageAccess(section.baseAddress,section.numPages,section.finalAccess)) { return false; }
					section.isFinalized = true;
				}
			}
			return true;
		}
		virtual void invalidateInstructionCache()
		{
			// Invalidate the instruction cache for the whole image.
			llvm::sys::Memory::InvalidateInstructionCache(imageBaseAddress,numAllocatedImagePages << Platform::getPageSizeLog2());
		}

	private:
		struct Section
		{
			uint8* baseAddress;
			size_t numPages;
			Platform::MemoryAccess finalAccess;
			bool isFinalized;
		};
		
		std::vector<Section> sections;

		uint8* imageBaseAddress;
		size_t numAllocatedImagePages;
		uintptr numCommittedImagePages;

		uint8* allocateSection(uintptr numBytes,uintptr alignment,Platform::MemoryAccess finalAccess)
		{
			assert(!(alignment & (alignment - 1)));

			// Allocate the section at the lowest uncommitted byte of image memory.
			uint8* unalignedSectionBaseAddress = imageBaseAddress + (numCommittedImagePages << Platform::getPageSizeLog2());

			// If there's larger alignment than the page size requested, adjust the base pointer accordingly.
			uint8* sectionBaseAddress = (uint8*)((uintptr)(unalignedSectionBaseAddress + alignment - 1) & ~(alignment - 1));
			const size_t numAlignmentPages = (sectionBaseAddress - unalignedSectionBaseAddress) >> Platform::getPageSizeLog2();
			assert(!((sectionBaseAddress - unalignedSectionBaseAddress) & ((1 << Platform::getPageSizeLog2()) - 1)));

			// Round the allocation size up to the next page.
			const size_t numPages = (numBytes + (((size_t)1) << Platform::getPageSizeLog2()) - 1) >> Platform::getPageSizeLog2();

			// Check that the image memory allocation hasn't been exhausted.
			if(numCommittedImagePages + numAlignmentPages + numPages > numAllocatedImagePages) { return nullptr; }

			// Commit the section's pages.
			if(!Platform::commitVirtualPages(sectionBaseAddress,numPages)) { return nullptr; }
			
			// Record the section so finalizeMemory can update its access level.
			sections.push_back({sectionBaseAddress,numPages,finalAccess,false});

			// Update the allocated page count.
			numCommittedImagePages += numAlignmentPages + numPages;

			return sectionBaseAddress;
		}

		SectionMemoryManager(const SectionMemoryManager&) = delete;
		void operator=(const SectionMemoryManager&) = delete;
	};
	SectionMemoryManager SectionMemoryManager::singleton;

	void JITUnit::NotifyLoadedFunctor::operator()(
		const llvm::orc::ObjectLinkingLayerBase::ObjSetHandleT& objectSetHandle,
		const std::vector<std::unique_ptr<llvm::object::ObjectFile>>& objectSet,
		const std::vector<std::unique_ptr<llvm::RuntimeDyld::LoadedObjectInfo>>& loadResult
		)
	{
		assert(objectSet.size() == loadResult.size());
		for(uintptr objectIndex = 0;objectIndex < loadResult.size();++objectIndex)
		{
			auto& object = objectSet[objectIndex];
			auto& loadedObject = loadResult[objectIndex];

			// Iterate over the functions in the loaded object.
			for(auto symbolSizePair : llvm::object::computeSymbolSizes(*object.get()))
			{
				auto symbol = symbolSizePair.first;
				auto name = symbol.getName();
				auto address = symbol.getAddress();
				if(	symbol.getType() == llvm::object::SymbolRef::ST_Function
				&&	name
				&&	!address.getError())
				{
					// Compute the address the functions was loaded at.
					uintptr loadedAddress = *address;
					auto symbolSection = symbol.getSection();
					if(symbolSection)
					{
						loadedAddress += (uintptr)loadedObject->getSectionLoadAddress(*symbolSection.get());
					}

					// Notify the JIT unit that the symbol was loaded.
					jitUnit->notifySymbolLoaded(name->data(),loadedAddress,symbolSizePair.second);
				}
			}
			
			#ifdef _WIN32
				// On Windows, look for .pdata and .xdata sections containing information about how to unwind the stack.
				
				// Find the text, pdata, and xdata sections.
				llvm::object::SectionRef textSection;
				llvm::object::SectionRef pdataSection;
				llvm::object::SectionRef xdataSection;
				for(auto section : object->sections())
				{
					llvm::StringRef sectionName;
					if(!section.getName(sectionName))
					{
						if(sectionName == ".pdata") { pdataSection = section; }
						else if(sectionName == ".xdata") { xdataSection = section; }
						else if(sectionName == ".text") { textSection = section; }
					}
				}

				// Pass the sections to the platform to register unwind info.
				if(textSection.getObject() && pdataSection.getObject() && xdataSection.getObject())
				{
					RuntimePlatform::registerSEHUnwindInfo(
						(uintptr)loadedObject->getSectionLoadAddress(textSection),
						(uintptr)loadedObject->getSectionLoadAddress(xdataSection),
						(uintptr)loadedObject->getSectionLoadAddress(pdataSection),
						pdataSection.getSize()
						);
				}
			#endif
		}
	}

	static uintptr printedModuleId = 0;

	void printModule(const llvm::Module* llvmModule,const char* filename)
	{
		std::error_code errorCode;
		std::string augmentedFilename = std::string(filename) + std::to_string(printedModuleId++) + ".ll";
		llvm::raw_fd_ostream dumpFileStream(augmentedFilename,errorCode,llvm::sys::fs::OpenFlags::F_Text);
		llvmModule->print(dumpFileStream,nullptr);
		Log::printf(Log::Category::debug,"Dumped LLVM module to: %s\n",augmentedFilename.c_str());
	}

	bool JITUnit::compile(llvm::Module* llvmModule)
	{
		// Get a target machine object for this host, and set the module to use its data layout.
		llvmModule->setDataLayout(targetMachine->createDataLayout());

		// Verify the module.
		#if DUMP_UNOPTIMIZED_MODULE
			printModule(llvmModule,"llvmDump");
		#endif

		#if VERIFY_MODULE
			std::string verifyOutputString;
			llvm::raw_string_ostream verifyOutputStream(verifyOutputString);
			if(llvm::verifyModule(*llvmModule,&verifyOutputStream))
			{
				Log::printf(Log::Category::error,"LLVM verification errors:\n%s\n",verifyOutputString.c_str());
				return false;
			}
			Log::printf(Log::Category::debug,"Verified LLVM module\n");
		#endif

		// Run some optimization on the module's functions.
		Core::Timer optimizationTimer;

		auto fpm = new llvm::legacy::FunctionPassManager(llvmModule);
		fpm->add(llvm::createPromoteMemoryToRegisterPass());
		fpm->add(llvm::createInstructionCombiningPass());
		fpm->add(llvm::createCFGSimplificationPass());
		fpm->add(llvm::createJumpThreadingPass());
		fpm->add(llvm::createConstantPropagationPass());
		fpm->doInitialization();
		for(auto functionIt = llvmModule->begin();functionIt != llvmModule->end();++functionIt)
		{ fpm->run(*functionIt); }
		delete fpm;
		
		Log::logRatePerSecond("Optimized LLVM module",optimizationTimer,(float64)llvmModule->size(),"functions");

		#if DUMP_OPTIMIZED_MODULE
			printModule(llvmModule,"llvmOptimizedDump");
		#endif

		// Pass the module to the JIT compiler.
		Core::Timer machineCodeTimer;
		handle = compileLayer->addModuleSet(
			std::vector<llvm::Module*>{llvmModule},
			&SectionMemoryManager::singleton,
			&NullResolver::singleton);
		compileLayer->emitAndFinalize(handle);
		Log::logRatePerSecond("Generated machine code",machineCodeTimer,(float64)llvmModule->size(),"functions");

		return true;
	}

	bool instantiateModule(const WebAssembly::Module& module,ModuleInstance* moduleInstance)
	{
		// Emit LLVM IR for the module.
		auto llvmModule = emitModule(module,moduleInstance);

		// Construct the JIT compilation pipeline for this module.
		moduleInstance->jitModule = new JITModule(moduleInstance);

		// Compile the module.
		return moduleInstance->jitModule->compile(llvmModule);
	}

	std::string getExternalFunctionName(ModuleInstance* moduleInstance,uintptr functionDefIndex)
	{
		assert(functionDefIndex < moduleInstance->functionDefs.size());
		return "wasmFunc" + std::to_string(functionDefIndex)
			+ "_" + moduleInstance->functionDefs[functionDefIndex]->debugName;
	}

	bool getFunctionIndexFromExternalName(const char* externalName,uintptr& outFunctionDefIndex)
	{
		if(!strncmp(externalName,"wasmFunc",8))
		{
			char* numberEnd = nullptr;
			outFunctionDefIndex = std::strtoull(externalName + 8,&numberEnd,10);
			return true;
		}
		else { return false; }
	}

	bool describeInstructionPointer(uintptr ip,std::string& outDescription)
	{
		auto symbolIt = addressToSymbolMap.upper_bound(ip);
		if(symbolIt == addressToSymbolMap.end()) { return false; }

		JITSymbol* symbol = symbolIt->second;
		if(ip < symbol->baseAddress || ip >= symbol->baseAddress + symbol->numBytes) { return false; }

		switch(symbol->type)
		{
		case JITSymbol::Type::functionInstance:
			outDescription = symbol->functionInstance->debugName;
			if(!outDescription.size()) { outDescription = "<unnamed function>"; }
			return true;
		case JITSymbol::Type::invokeThunk:
			outDescription = "<invoke thunk : " + getTypeName(symbol->invokeThunkType) + ">";
			return true;
		default: Core::unreachable();
		};
	}

	InvokeFunctionPointer getInvokeThunk(const FunctionType* functionType)
	{
		// Reuse cached invoke thunks for the same function type.
		auto mapIt = invokeThunkTypeToSymbolMap.find(functionType);
		if(mapIt != invokeThunkTypeToSymbolMap.end()) { return reinterpret_cast<InvokeFunctionPointer>(mapIt->second->baseAddress); }

		auto llvmModule = new llvm::Module("",context);
		auto llvmFunctionType = llvm::FunctionType::get(
			llvmVoidType,
			{asLLVMType(functionType)->getPointerTo(),llvmI64Type->getPointerTo()},
			false);
		auto llvmFunction = llvm::Function::Create(llvmFunctionType,llvm::Function::ExternalLinkage,"invokeThunk",llvmModule);
		auto argIt = llvmFunction->args().begin();
		llvm::Value* functionPointer = &*argIt++;
		llvm::Value* argBaseAddress = &*argIt;
		auto entryBlock = llvm::BasicBlock::Create(context,"entry",llvmFunction);
		llvm::IRBuilder<> irBuilder(entryBlock);

		// Load the function's arguments from an array of 64-bit values at an address provided by the caller.
		std::vector<llvm::Value*> structArgLoads;
		for(uintptr parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
		{
			structArgLoads.push_back(irBuilder.CreateLoad(
				irBuilder.CreatePointerCast(
					irBuilder.CreateInBoundsGEP(argBaseAddress,{emitLiteral((uintptr)parameterIndex)}),
					asLLVMType(functionType->parameters[parameterIndex])->getPointerTo()
					)
				));
		}

		// Call the llvm function with the actual implementation.
		auto returnValue = irBuilder.CreateCall(functionPointer,structArgLoads);

		// If the function has a return value, write it to the end of the argument array.
		if(functionType->ret != ResultType::none)
		{
			auto llvmResultType = asLLVMType(functionType->ret);
			irBuilder.CreateStore(
				returnValue,
				irBuilder.CreatePointerCast(
					irBuilder.CreateInBoundsGEP(argBaseAddress,{emitLiteral((uintptr)functionType->parameters.size())}),
					llvmResultType->getPointerTo()
					)
				);
		}

		irBuilder.CreateRetVoid();

		// Compile the invoke thunk.
		JITInvokeThunkUnit jitUnit(functionType);
		if(!jitUnit.compile(llvmModule)) { Core::fatalError("error compiling invoke thunk"); }

		assert(jitUnit.symbol);
		invokeThunkTypeToSymbolMap[functionType] = jitUnit.symbol;
		addressToSymbolMap[jitUnit.symbol->baseAddress + jitUnit.symbol->numBytes] = jitUnit.symbol;
		return reinterpret_cast<InvokeFunctionPointer>(jitUnit.symbol->baseAddress);
	}
	
	void init()
	{
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

		auto targetTriple = llvm::sys::getProcessTriple();
		#ifdef __APPLE__
			// Didn't figure out exactly why, but this works around a problem with the MacOS dynamic loader. Without it,
			// our symbols can't be found in the JITed object file.
			targetTriple += "-elf";
		#endif
		targetMachine = llvm::EngineBuilder().selectTarget(llvm::Triple(targetTriple),"","",llvm::SmallVector<std::string,0>());

		llvmI8Type = llvm::Type::getInt8Ty(context);
		llvmI16Type = llvm::Type::getInt16Ty(context);
		llvmI32Type = llvm::Type::getInt32Ty(context);
		llvmI64Type = llvm::Type::getInt64Ty(context);
		llvmF32Type = llvm::Type::getFloatTy(context);
		llvmF64Type = llvm::Type::getDoubleTy(context);
		llvmVoidType = llvm::Type::getVoidTy(context);
		llvmBoolType = llvm::Type::getInt1Ty(context);
		llvmI8PtrType = llvmI8Type->getPointerTo();

		llvmResultTypes[(size_t)ResultType::none] = llvm::Type::getVoidTy(context);
		llvmResultTypes[(size_t)ResultType::i32] = llvmI32Type;
		llvmResultTypes[(size_t)ResultType::i64] = llvmI64Type;
		llvmResultTypes[(size_t)ResultType::f32] = llvmF32Type;
		llvmResultTypes[(size_t)ResultType::f64] = llvmF64Type;

		// Create zero constants of each type.
		typedZeroConstants[(size_t)ValueType::invalid] = nullptr;
		typedZeroConstants[(size_t)ValueType::i32] = emitLiteral((uint32)0);
		typedZeroConstants[(size_t)ValueType::i64] = emitLiteral((uint64)0);
		typedZeroConstants[(size_t)ValueType::f32] = emitLiteral((float32)0.0f);
		typedZeroConstants[(size_t)ValueType::f64] = emitLiteral((float64)0.0);
	}
}
