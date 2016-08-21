#include "LLVMJIT.h"

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
	
	// Used to resolve references to intrinsics in the LLVM IR.
	struct IntrinsicResolver : llvm::RuntimeDyld::SymbolResolver
	{
		static IntrinsicResolver singleton;

		void* getSymbolAddress(const std::string& name) const;

		virtual llvm::RuntimeDyld::SymbolInfo findSymbol(const std::string& name) override;
		virtual llvm::RuntimeDyld::SymbolInfo findSymbolInLogicalDylib(const std::string& name) override;
	};

	// Allocates memory for the LLVM object loader.
	struct SectionMemoryManager : llvm::RTDyldMemoryManager
	{
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
			return allocateSection((uintptr)numBytes,alignment,Platform::MemoryAccess::Execute);
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

	struct JITFunction
	{
		std::string name;
		uintptr baseAddress;
		size_t size;
	};

	struct JITModule
	{
		const AST::Module* astModule;

		typedef llvm::orc::ObjectLinkingLayer<NotifyLoadedFunctor> ObjectLayer;
		std::unique_ptr<ObjectLayer> objectLayer;

		typedef llvm::orc::IRCompileLayer<ObjectLayer> CompileLayer;
		std::unique_ptr<CompileLayer> compileLayer;

		CompileLayer::ModuleSetHandleT handle;

		std::vector<JITFunction> functions;
		
		JITModule(const AST::Module* inASTModule) : astModule(inASTModule) {}
	};

	// All the modules that have been JITted.
	std::vector<JITModule*> jitModules;

	IntrinsicResolver IntrinsicResolver::singleton;
	void* IntrinsicResolver::getSymbolAddress(const std::string& name) const
	{
		const Intrinsics::Function* intrinsicFunction = Intrinsics::findFunction(name.c_str());
		if(intrinsicFunction) { return intrinsicFunction->value; }

		void *addr = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(name);
		if (addr) { return addr; }

		std::cerr << "getSymbolAddress: " << name << " not found" << std::endl;
		return nullptr;
	}

	llvm::RuntimeDyld::SymbolInfo IntrinsicResolver::findSymbol(const std::string& name)
	{
		return llvm::RuntimeDyld::SymbolInfo(reinterpret_cast<uint64>(getSymbolAddress(name)),llvm::JITSymbolFlags::None);
	}

	llvm::RuntimeDyld::SymbolInfo IntrinsicResolver::findSymbolInLogicalDylib(const std::string& name)
	{
		return llvm::RuntimeDyld::SymbolInfo(reinterpret_cast<uint64>(getSymbolAddress(name)),llvm::JITSymbolFlags::None);
	}
	
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
					jitModule->functions.push_back({*name,loadedAddress,symbolSizePair.second});
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

	void printModule(const llvm::Module* llvmModule,const char* filename)
	{
		std::error_code errorCode;
		llvm::raw_fd_ostream dumpFileStream(llvm::StringRef(filename),errorCode,llvm::sys::fs::OpenFlags::F_Text);
		llvmModule->print(dumpFileStream,nullptr);
	}

	bool compileModule(const AST::Module* astModule)
	{
		auto llvmModule = emitModule(astModule);
		
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
		#ifdef _DEBUG
			printModule(llvmModule,"llvmDump.ll");

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

		#ifdef _DEBUG
			printModule(llvmModule,"llvmOptimizedDump.ll");
		#endif

		// Pass the module to the JIT compiler.
		#if WAVM_TIMER_OUTPUT
		Core::Timer machineCodeTimer;
		#endif
		std::vector<llvm::Module*> moduleSet;
		moduleSet.push_back(llvmModule);

		// Construct the JIT module and compile layers.
		auto jitModule = new JITModule(astModule);
		jitModules.push_back(jitModule);
		jitModule->objectLayer = llvm::make_unique<JITModule::ObjectLayer>(NotifyLoadedFunctor(jitModule));
		jitModule->compileLayer = llvm::make_unique<JITModule::CompileLayer>(*jitModule->objectLayer,llvm::orc::SimpleCompiler(*targetMachine));

		// Compile the module.
		jitModule->handle = jitModule->compileLayer->addModuleSet(moduleSet,llvm::make_unique<SectionMemoryManager>(),&IntrinsicResolver::singleton);
		#if WAVM_TIMER_OUTPUT
		std::cout << "Generated machine code in " << machineCodeTimer.getMilliseconds() << "ms" << std::endl;
		#endif
		
		return true;
	}

	std::string getExternalFunctionName(uintptr_t functionIndex,bool invokeThunk)
	{
		return invokeThunk
			? "invokeThunk" + std::to_string(functionIndex)
			: "wasmFunc" + std::to_string(functionIndex);
	}

	bool getFunctionIndexFromExternalName(const char* externalName,uintptr_t& outFunctionIndex)
	{
		if(!strncmp(externalName,"wasmFunc",8))
		{
			char* numberEnd = nullptr;
			outFunctionIndex = std::strtoull(externalName + 8,&numberEnd,10);
			return *numberEnd == 0;
		}
		else if(!strncmp(externalName,"invokeThunk",11))
		{
			char* numberEnd = nullptr;
			outFunctionIndex = std::strtoull(externalName + 11,&numberEnd,10);
			return *numberEnd == 0;
		}
		else { return false; }
	}

	InvokeFunctionPointer getInvokeFunctionPointer(const AST::Module* module,uintptr functionIndex,bool invokeThunk)
	{
		for(auto jitModule : jitModules)
		{
			if(jitModule->astModule == module)
			{
				return (InvokeFunctionPointer)jitModule->compileLayer->findSymbolIn(jitModule->handle,getExternalFunctionName(functionIndex,invokeThunk),false).getAddress();
			}
		}
		return nullptr;
	}
	
	bool describeInstructionPointer(uintptr_t ip,std::string& outDescription)
	{
		for(auto jitModule : jitModules)
		{
			for(auto function : jitModule->functions)
			{
				if(ip >= function.baseAddress && ip <= (function.baseAddress + function.size))
				{
					uintptr_t functionIndex;
					if(getFunctionIndexFromExternalName(function.name.data(),functionIndex))
					{
						assert(functionIndex < jitModule->astModule->functions.size());
						const AST::Function& astFunction = jitModule->astModule->functions[functionIndex];
						if(astFunction.name) { outDescription = astFunction.name; return true; }
					}

					outDescription = function.name;
					return true;
				}
			}
		}
		return false;
	}
}
