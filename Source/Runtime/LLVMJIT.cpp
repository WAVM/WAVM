#include "LLVMJIT.h"

// This needs to be 1 to allow debuggers such as Visual Studio to place breakpoints and step through the JITed code.
#define USE_WRITEABLE_JIT_CODE_PAGES 0

#define DUMP_UNOPTIMIZED_MODULE _DEBUG
#define VERIFY_MODULE _DEBUG
#define DUMP_OPTIMIZED_MODULE _DEBUG

namespace LLVMJIT
{
	// Functor that receives notifications when an object produced by the JIT is loaded.
	struct NotifyLoadedFunctor
	{
		struct JITModule* jitModule;
		NotifyLoadedFunctor(struct JITModule* inJITModule): jitModule(inJITModule) {}
		void operator()(
			const llvm::orc::ObjectLinkingLayerBase::ObjSetHandleT& objectSetHandle,
			const std::vector<std::unique_ptr<llvm::object::ObjectFile>>& objectSet,
			const std::vector<std::unique_ptr<llvm::RuntimeDyld::LoadedObjectInfo>>& loadResult
			);
	};
	
	// Used to ensure that 
	struct NullResolver : llvm::RuntimeDyld::SymbolResolver
	{
		static NullResolver singleton;
		virtual llvm::RuntimeDyld::SymbolInfo findSymbol(const std::string& name) override;
		virtual llvm::RuntimeDyld::SymbolInfo findSymbolInLogicalDylib(const std::string& name) override;
	};

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
				if(!Platform::setVirtualPageAccess(section.baseAddress,section.numPages,section.finalAccess)) { return false; }
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
			sections.push_back({sectionBaseAddress,numPages,finalAccess});

			// Update the allocated page count.
			numCommittedImagePages += numAlignmentPages + numPages;

			return sectionBaseAddress;
		}

		SectionMemoryManager(const SectionMemoryManager&) = delete;
		void operator=(const SectionMemoryManager&) = delete;
	};
	SectionMemoryManager SectionMemoryManager::singleton;

	struct JITFunctionSymbol
	{
		uintptr functionIndex;
		uintptr baseAddress;
		size_t size;
		bool isInvokeThunk;
	};

	struct JITModule
	{
		ModuleInstance* moduleInstance;

		typedef llvm::orc::ObjectLinkingLayer<NotifyLoadedFunctor> ObjectLayer;
		std::unique_ptr<ObjectLayer> objectLayer;

		typedef llvm::orc::IRCompileLayer<ObjectLayer> CompileLayer;
		std::unique_ptr<CompileLayer> compileLayer;

		CompileLayer::ModuleSetHandleT handle;

		std::vector<JITFunctionSymbol> functionSymbols;
		
		JITModule(ModuleInstance* inModuleInstance) : moduleInstance(inModuleInstance) {}
	};

	// All the modules that have been JITted.
	std::vector<JITModule*> jitModules;

	NullResolver NullResolver::singleton;
	llvm::RuntimeDyld::SymbolInfo NullResolver::findSymbol(const std::string& name) { throw InstantiationException(InstantiationException::Cause::codeGenFailed); }
	llvm::RuntimeDyld::SymbolInfo NullResolver::findSymbolInLogicalDylib(const std::string& name) { throw InstantiationException(InstantiationException::Cause::codeGenFailed); }
	
	void NotifyLoadedFunctor::operator()(
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

					// Save the address range this function was loaded at for future address->symbol lookups.
					uintptr functionIndex;
					bool isInvokeThunk;
					if(getFunctionIndexFromExternalName(name->data(),functionIndex,isInvokeThunk))
					{
						jitModule->functionSymbols.push_back({functionIndex,loadedAddress,symbolSizePair.second,isInvokeThunk});
					}
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
		std::cout << "Dumped LLVM module to: " << augmentedFilename << std::endl;
	}

	bool instantiateModule(const WebAssembly::Module& module,ModuleInstance* moduleInstance)
	{
		auto llvmModule = emitModule(module,moduleInstance);

		// Get a target machine object for this host, and set the module to use its data layout.
		auto targetTriple = llvm::sys::getProcessTriple();
		#ifdef __APPLE__
			// Didn't figure out exactly why, but this works around a problem with the MacOS dynamic loader. Without it,
			// our symbols can't be found in the JITed object file.
			targetTriple += "-elf";
		#endif
		auto targetMachine = llvm::EngineBuilder().selectTarget(llvm::Triple(targetTriple),"","",llvm::SmallVector<std::string,0>());
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
				std::cerr << "LLVM verification errors:\n" << verifyOutputStream.str() << std::endl;
				return false;
			}
		#endif

		// Run some optimization on the module's functions.
		#if WAVM_TIMER_OUTPUT
		Core::Timer optimizationTimer;
		#endif

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

		#if WAVM_TIMER_OUTPUT
		std::cout << "Optimized LLVM code in " << optimizationTimer.getMilliseconds() << "ms" << std::endl;
		#endif

		#if DUMP_OPTIMIZED_MODULE
			printModule(llvmModule,"llvmOptimizedDump");
		#endif

		// Pass the module to the JIT compiler.
		#if WAVM_TIMER_OUTPUT
		Core::Timer machineCodeTimer;
		#endif
		std::vector<llvm::Module*> moduleSet;
		moduleSet.push_back(llvmModule);

		// Construct the JIT module and compile layers.
		auto jitModule = new JITModule(moduleInstance);
		jitModules.push_back(jitModule);
		jitModule->objectLayer = llvm::make_unique<JITModule::ObjectLayer>(NotifyLoadedFunctor(jitModule));
		jitModule->compileLayer = llvm::make_unique<JITModule::CompileLayer>(*jitModule->objectLayer,llvm::orc::SimpleCompiler(*targetMachine));

		// Compile the module.
		jitModule->handle = jitModule->compileLayer->addModuleSet(moduleSet,&SectionMemoryManager::singleton,&NullResolver::singleton);
		#if WAVM_TIMER_OUTPUT
		std::cout << "Generated machine code in " << machineCodeTimer.getMilliseconds() << "ms" << std::endl;
		#endif
		
		// Find any thunks created for function imports that were reexported, and update the imported FunctionInstance.
		for(uintptr functionIndex = 0;functionIndex < moduleInstance->functions.size();++functionIndex)
		{
			FunctionInstance* functionInstance = moduleInstance->functions[functionIndex];
			if(functionIndex >= moduleInstance->functions.size() - module.functionDefs.size())
			{
				functionInstance->nativeFunction = (void*)jitModule->compileLayer->findSymbolIn(jitModule->handle,getExternalFunctionName(moduleInstance,functionIndex,false),false).getAddress();
			}

			if(!functionInstance->invokeThunk)
			{
				functionInstance->invokeThunk = (InvokeThunk)jitModule->compileLayer->findSymbolIn(jitModule->handle,getExternalFunctionName(moduleInstance,functionIndex,true),false).getAddress();
			}
		}

		return true;
	}

	std::string getExternalFunctionName(ModuleInstance* moduleInstance,uintptr_t functionIndex,bool invokeThunk)
	{
		std::string result = invokeThunk
			? "invokeThunk" + std::to_string(functionIndex)
			: "wasmFunc" + std::to_string(functionIndex);
		if(functionIndex  < moduleInstance->functions.size())
		{
			result += "_";
			result += moduleInstance->functions[functionIndex]->debugName.c_str();
		}
		return result;
	}

	bool getFunctionIndexFromExternalName(const char* externalName,uintptr_t& outFunctionIndex,bool& outIsInvokeThunk)
	{
		if(!strncmp(externalName,"wasmFunc",8))
		{
			char* numberEnd = nullptr;
			outFunctionIndex = std::strtoull(externalName + 8,&numberEnd,10);
			outIsInvokeThunk = false;
			return true;
		}
		else if(!strncmp(externalName,"invokeThunk",11))
		{
			char* numberEnd = nullptr;
			outFunctionIndex = std::strtoull(externalName + 11,&numberEnd,10);
			outIsInvokeThunk = true;
			return true;
		}
		else { return false; }
	}

	bool describeInstructionPointer(uintptr_t ip,std::string& outDescription)
	{
		for(auto jitModule : jitModules)
		{
			for(auto functionSymbol : jitModule->functionSymbols)
			{
				if(ip >= functionSymbol.baseAddress && ip <= (functionSymbol.baseAddress + functionSymbol.size))
				{
					assert(functionSymbol.functionIndex < jitModule->moduleInstance->functions.size());
					outDescription = jitModule->moduleInstance->functions[functionSymbol.functionIndex]->debugName;
					if(!outDescription.size()) { outDescription = "function #" + std::to_string(functionSymbol.functionIndex); }
					if(functionSymbol.isInvokeThunk) { outDescription += " (invoke thunk)"; }
					return true;
				}
			}
		}
		return false;
	}
}
