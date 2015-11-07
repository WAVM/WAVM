#define __STDC_FORMAT_MACROS
#include "LLVMJIT.h"

#ifdef __linux__
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#endif

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
					auto symbolSection = object->section_begin();
					if(!symbol.getSection(symbolSection))
					{
						llvm::StringRef sectionName;
						if(!symbolSection->getName(sectionName))
						{
							loadedAddress += (uintptr)loadedObject->getSectionLoadAddress(sectionName);
						}
					}

					// Save the address range this function was loaded at for future address->symbol lookups.
					jitModule->functions.push_back({*name,loadedAddress,symbolSizePair.second});
				}
			}
			
			#ifdef _WIN32
				// On Windows, look for .pdata and .xdata sections containing information about how to unwind the stack.
				auto textLoadAddr = loadedObject->getSectionLoadAddress(".text");
				auto pdataLoadAddr = loadedObject->getSectionLoadAddress(".pdata");
				auto xdataLoadAddr = loadedObject->getSectionLoadAddress(".xdata");
				if(pdataLoadAddr && xdataLoadAddr)
				{
					// Find the pdata section.
					llvm::object::SectionRef pdataSection;
					for(auto section : object->sections())
					{
						llvm::StringRef sectionName;
						if(!section.getName(sectionName) && sectionName == ".pdata")
						{
							pdataSection = section;
							break;
						}
					}

					RuntimePlatform::registerSEHUnwindInfo(
						(uintptr)textLoadAddr,
						(uintptr)xdataLoadAddr,
						(uintptr)pdataLoadAddr,
						pdataSection.getSize()
						);
				}
			#endif
		}
	}

#ifdef __linux__
	void writePerfMap()
	{
		static FILE* perfFile = nullptr;
		const ssize_t bufferLength = 256;
		char filename[bufferLength];
		if (snprintf(filename, bufferLength, "/tmp/perf-%d.map", getpid()) < bufferLength)
		{
		    perfFile = fopen(filename, "a");
		}
		if (!perfFile)
		  return;
		for(auto jitModule : jitModules)
		{
			for(auto function : jitModule->functions)
			{
				const char *name = function.name.data();
				uintptr_t functionIndex;
				if(getFunctionIndexFromExternalName(name,functionIndex))
				{
					assert(functionIndex < jitModule->astModule->functions.size());
					auto astFunction = jitModule->astModule->functions[functionIndex];
					if(astFunction->name) { name = astFunction->name;}
				}
				fprintf(perfFile, "%" PRIxPTR " %" PRIxPTR " %s\n", function.baseAddress, function.size, name);
			}
		}
		fclose(perfFile);
	}
#endif

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
			std::string verifyOutputString;
			llvm::raw_string_ostream verifyOutputStream(verifyOutputString);
			if(llvm::verifyModule(*llvmModule,&verifyOutputStream))
			{
				std::error_code errorCode;
				llvm::raw_fd_ostream dumpFileStream(llvm::StringRef("llvmDump.ll"),errorCode,llvm::sys::fs::OpenFlags::F_Text);
				llvmModule->print(dumpFileStream,nullptr);
				std::cerr << "LLVM verification errors:\n" << verifyOutputStream.str() << std::endl;
				return false;
			}
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
		fpm->doInitialization();
		for(auto functionIt = llvmModule->begin();functionIt != llvmModule->end();++functionIt)
		{ fpm->run(*functionIt); }
		delete fpm;

		std::cout << "Optimized LLVM code in " << optimizationTimer.getMilliseconds() << "ms" << std::endl;

		// Pass the module to the JIT compiler.
		Core::Timer machineCodeTimer;
		std::vector<llvm::Module*> moduleSet;
		moduleSet.push_back(llvmModule);

		// Construct the JIT module and compile layers.
		auto jitModule = new JITModule(astModule);
		jitModules.push_back(jitModule);
		jitModule->objectLayer = llvm::make_unique<JITModule::ObjectLayer>(NotifyLoadedFunctor(jitModule));
		jitModule->compileLayer = llvm::make_unique<JITModule::CompileLayer>(*jitModule->objectLayer,llvm::orc::SimpleCompiler(*targetMachine));

		// Compile the module.
		jitModule->handle = jitModule->compileLayer->addModuleSet(moduleSet,llvm::make_unique<llvm::SectionMemoryManager>(),&IntrinsicResolver::singleton);
#ifdef __linux__
		if (getenv("WAVM_PERF")) writePerfMap();
#endif
		std::cout << "Generated machine code in " << machineCodeTimer.getMilliseconds() << "ms" << std::endl;
		
		return true;
	}

	std::string getExternalFunctionName(uintptr_t functionIndex)
	{
		return "wasmFunc" + std::to_string(functionIndex);
	}

	bool getFunctionIndexFromExternalName(const char* externalName,uintptr_t& outFunctionIndex)
	{
		if(strncmp(externalName,"wasmFunc",8)) { return false; }
		else
		{
			char* numberEnd = nullptr;
			outFunctionIndex = std::strtoull(externalName + 8,&numberEnd,10);
			return *numberEnd == 0;
		}
	}

	void* getFunctionPointer(const AST::Module* module,uintptr functionIndex)
	{
		for(auto jitModule : jitModules)
		{
			if(jitModule->astModule == module)
			{
				return (void*)jitModule->compileLayer->findSymbolIn(jitModule->handle,getExternalFunctionName(functionIndex),false).getAddress();
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
						auto astFunction = jitModule->astModule->functions[functionIndex];
						if(astFunction->name) { outDescription = astFunction->name; return true; }
					}

					outDescription = function.name;
					return true;
				}
			}
		}
		return false;
	}
}
