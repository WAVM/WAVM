#include "WAVM.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Host.h"
#include "llvm/Transforms/Scalar.h"
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace JIT
{
	using namespace WAVM;

	//===----------------------------------------------------------------------===//
	// MCJIT helper class
	//===----------------------------------------------------------------------===//

	class Singleton
	{
	public:
		Singleton() : context(llvm::getGlobalContext())
		{
			llvm::InitializeNativeTarget();
			llvm::InitializeNativeTargetAsmPrinter();
			llvm::InitializeNativeTargetAsmParser();
		}
		~Singleton()
		{
			for(auto engineIt = engines.begin();engineIt != engines.end();++engineIt)
				delete *engineIt;
		}

		void compileModule(const Module& module);
		void* getPointerToFunction(llvm::Function* function);

	private:

		llvm::LLVMContext& context;
		std::vector<llvm::ExecutionEngine*> engines;
	};
	
	void Singleton::compileModule(const Module& module)
	{
		// Work around a problem with LLVM generating a COFF file that MCJIT can't parse. Adding -elf to the target triple forces it to use ELF instead of COFF.
		#if WIN32
			module.llvmModule->setTargetTriple(llvm::sys::getProcessTriple() + "-elf");
		#endif

		// Setup MCJIT for this module.
		std::string errStr;
		llvm::ExecutionEngine* newEngine =
			llvm::EngineBuilder(std::unique_ptr<llvm::Module>(module.llvmModule))
			.setErrorStr(&errStr)
			.create();
		if(!newEngine)
		{
			fprintf(stderr,"Could not create ExecutionEngine: %s\n",errStr.c_str());
			abort();
		}
		engines.push_back(newEngine);

		// Run some optimization on the module's functions.		
		auto fpm = new llvm::legacy::FunctionPassManager(module.llvmModule);
		module.llvmModule->setDataLayout(newEngine->getDataLayout());
		fpm->add(llvm::createPromoteMemoryToRegisterPass());
		fpm->add(llvm::createBasicAliasAnalysisPass());
		fpm->add(llvm::createInstructionCombiningPass());
		fpm->add(llvm::createReassociatePass());
		//fpm->add(llvm::createGVNPass());
		fpm->add(llvm::createCFGSimplificationPass());
		fpm->doInitialization();
		for(auto functionIt = module.llvmModule->begin();functionIt != module.llvmModule->end();++functionIt)
		{
			fpm->run(*functionIt);
			//functionIt->dump();
		}
		delete fpm;

		// Generate native machine code.
		newEngine->finalizeObject();
	}

	void* Singleton::getPointerToFunction(llvm::Function* function)
	{
		// See if an existing instance of MCJIT has this function.
		for(auto engineIt = engines.begin();engineIt != engines.end();++engineIt)
		{
			void *P = (*engineIt)->getPointerToFunction(function);
			if(P)
				return P;
		}

		return nullptr;
	}

	static Singleton singleton;
}

namespace WAVM
{
	void jitCompileModule(const Module& module)
	{
		JIT::singleton.compileModule(module);
	}

	void* getJITFunctionPointer(llvm::Function* function)
	{
		return JIT::singleton.getPointerToFunction(function);
	}
}