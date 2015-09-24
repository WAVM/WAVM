#include "LLVMJIT.h"

namespace LLVMJIT
{	
	llvm::LLVMContext& context = llvm::getGlobalContext();
	bool isInitialized = false;
	
	// Maps a type ID to the corresponding LLVM type.
	llvm::Type* llvmTypesByTypeId[(size_t)TypeId::num];
	
	// A dummy constant to use as the unique value inhabiting the void type.
	llvm::Constant* voidDummy = nullptr;

	// Zero constants of each type.
	llvm::Constant* typedZeroConstants[(size_t)TypeId::num];

	// All the modules that have been JITted.
	std::vector<struct JITModule*> jitModules;

	// Converts an AST type to a LLVM type.
	llvm::Type* asLLVMType(TypeId type) { return llvmTypesByTypeId[(uintptr)type]; }
	
	// Converts an AST function type to a LLVM type.
	llvm::FunctionType* asLLVMType(const FunctionType& functionType)
	{
		auto llvmArgTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * functionType.parameters.size());
		for(uintptr argIndex = 0;argIndex < functionType.parameters.size();++argIndex)
		{
			llvmArgTypes[argIndex] = asLLVMType(functionType.parameters[argIndex]);
		}
		auto llvmReturnType = asLLVMType(functionType.returnType);
		return llvm::FunctionType::get(llvmReturnType,llvm::ArrayRef<llvm::Type*>(llvmArgTypes,functionType.parameters.size()),false);
	}

	// Used to resolve references to intrinsics in the LLVM IR.
	struct IntrinsicResolver : llvm::RuntimeDyld::SymbolResolver
	{
		static IntrinsicResolver singleton;

		void* getSymbolAddress(const std::string& name) const
		{
			const Intrinsics::Function* intrinsicFunction = Intrinsics::findFunction(name.c_str());
			if(intrinsicFunction) { return intrinsicFunction->value; }
			else
			{
				std::cerr << "getSymbolAddress: " << name << " not found" << std::endl;
				return nullptr;
			}
		}

		llvm::RuntimeDyld::SymbolInfo findSymbol(const std::string& name) override
		{
			return llvm::RuntimeDyld::SymbolInfo(reinterpret_cast<uint64>(getSymbolAddress(name)),llvm::JITSymbolFlags::None);
		}

		llvm::RuntimeDyld::SymbolInfo findSymbolInLogicalDylib(const std::string& name) override
		{
			return llvm::RuntimeDyld::SymbolInfo(reinterpret_cast<uint64>(getSymbolAddress(name)),llvm::JITSymbolFlags::None);
		}
	};
	IntrinsicResolver IntrinsicResolver::singleton;
	
	typedef llvm::orc::ObjectLinkingLayer<> ObjectLayer;
	typedef llvm::orc::IRCompileLayer<ObjectLayer> CompileLayer;
	std::unique_ptr<ObjectLayer> objectLayer;
	std::unique_ptr<CompileLayer> compileLayer;

	static void init()
	{
		isInitialized = true;

		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();

		llvmTypesByTypeId[(size_t)TypeId::None] = nullptr;
		llvmTypesByTypeId[(size_t)TypeId::I8] = llvm::Type::getInt8Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::I16] = llvm::Type::getInt16Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::I32] = llvm::Type::getInt32Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::I64] = llvm::Type::getInt64Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::F32] = llvm::Type::getFloatTy(context);
		llvmTypesByTypeId[(size_t)TypeId::F64] = llvm::Type::getDoubleTy(context);
		llvmTypesByTypeId[(size_t)TypeId::Bool] = llvm::Type::getInt1Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::Void] = llvm::Type::getVoidTy(context);
		
		// Create a null pointer constant to use as the void dummy value.
		voidDummy = llvm::Constant::getIntegerValue(llvm::Type::getInt8Ty(context),llvm::APInt(64,0));
		
		// Create zero constants of each type.
		typedZeroConstants[(size_t)TypeId::None] = nullptr;
		typedZeroConstants[(size_t)TypeId::I8] = compileLiteral((uint8)0);
		typedZeroConstants[(size_t)TypeId::I16] = compileLiteral((uint16)0);
		typedZeroConstants[(size_t)TypeId::I32] = compileLiteral((uint32)0);
		typedZeroConstants[(size_t)TypeId::I64] = compileLiteral((uint64)0);
		typedZeroConstants[(size_t)TypeId::F32] = compileLiteral((float32)0.0f);
		typedZeroConstants[(size_t)TypeId::F64] = compileLiteral((float64)0.0);
		typedZeroConstants[(size_t)TypeId::Bool] = compileLiteral(false);
		typedZeroConstants[(size_t)TypeId::Void] = voidDummy;

		auto targetMachine = llvm::EngineBuilder().selectTarget(llvm::Triple(llvm::sys::getProcessTriple() + "-elf"),"","",llvm::SmallVector<std::string,0>());
		objectLayer = std::make_unique<ObjectLayer>();
		compileLayer = std::make_unique<CompileLayer>(*objectLayer,llvm::orc::SimpleCompiler(*targetMachine));
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
	}
	
	bool compileModule(const Module* astModule)
	{
		if(!isInitialized)
		{
			init();
		}

		auto jitModule = emitModule(astModule);
		jitModules.push_back(jitModule);

		// Verify the module.
		#ifdef _DEBUG
			std::string verifyOutputString;
			llvm::raw_string_ostream verifyOutputStream(verifyOutputString);
			if(llvm::verifyModule(*jitModule->llvmModule,&verifyOutputStream))
			{
				std::error_code errorCode;
				llvm::raw_fd_ostream dumpFileStream(llvm::StringRef("llvmDump.ll"),errorCode,llvm::sys::fs::OpenFlags::F_Text);
				jitModule->llvmModule->print(dumpFileStream,nullptr);
				std::cerr << "LLVM verification errors:\n" << verifyOutputStream.str() << std::endl;
				return false;
			}
		#endif

		// Run some optimization on the module's functions.
		Core::Timer optimizationTimer;
		llvm::legacy::PassManager passManager;
		passManager.add(llvm::createFunctionInliningPass(2,0));
		passManager.run(*jitModule->llvmModule);

		auto fpm = new llvm::legacy::FunctionPassManager(jitModule->llvmModule);
		fpm->add(llvm::createPromoteMemoryToRegisterPass());
		fpm->add(llvm::createBasicAliasAnalysisPass());
		fpm->add(llvm::createInstructionCombiningPass());
		fpm->add(llvm::createReassociatePass());
		fpm->add(llvm::createGVNPass());
		fpm->add(llvm::createLICMPass());
		fpm->add(llvm::createLoopVectorizePass());
		fpm->add(llvm::createSLPVectorizerPass());
		fpm->add(llvm::createBBVectorizePass());
		fpm->add(llvm::createLoopUnrollPass());
		fpm->add(llvm::createCFGSimplificationPass());
		fpm->add(llvm::createConstantPropagationPass());
		fpm->add(llvm::createDeadInstEliminationPass());
		fpm->add(llvm::createDeadCodeEliminationPass());
		fpm->add(llvm::createDeadStoreEliminationPass());
		fpm->add(llvm::createAggressiveDCEPass());
		fpm->add(llvm::createBitTrackingDCEPass());
		fpm->add(llvm::createInductiveRangeCheckEliminationPass());
		fpm->add(llvm::createIndVarSimplifyPass());
		fpm->add(llvm::createLoopStrengthReducePass());
		fpm->add(llvm::createLoopRotatePass());
		fpm->add(llvm::createLoopIdiomPass());
		fpm->add(llvm::createJumpThreadingPass());
		fpm->add(llvm::createMemCpyOptPass());
		fpm->add(llvm::createConstantHoistingPass());
		fpm->doInitialization();
		for(auto functionIt = jitModule->llvmModule->begin();functionIt != jitModule->llvmModule->end();++functionIt)
		{ fpm->run(*functionIt); }
		delete fpm;

		std::cout << "Optimized LLVM code in " << optimizationTimer.getMilliseconds() << "ms" << std::endl;

		// Pass the module to the JIT compiler.
		Core::Timer machineCodeTimer;
		std::vector<llvm::Module*> moduleSet;
		moduleSet.push_back(jitModule->llvmModule);
		jitModule->handle = compileLayer->addModuleSet(moduleSet,llvm::make_unique<llvm::SectionMemoryManager>(),&IntrinsicResolver::singleton);
		std::cout << "Generated machine code in " << machineCodeTimer.getMilliseconds() << "ms" << std::endl;
		
		return true;
	}
}

namespace Runtime
{
	bool compileModule(const Module* astModule)
	{
		return LLVMJIT::compileModule(astModule);
	}

	void* getFunctionPointer(const Module* module,uintptr functionIndex)
	{
		for(auto jitModule : LLVMJIT::jitModules)
		{
			if(jitModule->astModule == module)
			{
				return (void*)LLVMJIT::compileLayer->findSymbolIn(jitModule->handle,jitModule->functions[functionIndex]->getName(),false).getAddress();
			}
		}
		return nullptr;
	}
}