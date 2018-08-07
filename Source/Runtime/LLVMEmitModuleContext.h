#pragma once

#include "LLVMJIT.h"

#include "LLVMPreInclude.h"

#include "llvm/IR/DIBuilder.h"

#include "LLVMPostInclude.h"

namespace LLVMJIT
{
	struct EmitModuleContext
	{
		const IR::Module& module;
		Runtime::ModuleInstance* moduleInstance;

		std::shared_ptr<llvm::Module> llvmModuleSharedPtr;
		llvm::Module* llvmModule;
		std::vector<llvm::Function*> functionDefs;

		std::unique_ptr<llvm::DIBuilder> diBuilder;
		llvm::DICompileUnit* diCompileUnit;
		llvm::DIFile* diModuleScope;

		llvm::DIType* diValueTypes[(Uptr)ValueType::num];

		llvm::MDNode* likelyFalseBranchWeights;
		llvm::MDNode* likelyTrueBranchWeights;

		llvm::Value* fpRoundingModeMetadata;
		llvm::Value* fpExceptionMetadata;

#ifdef _WIN32
		llvm::Function* tryPrologueDummyFunction;
#else
		llvm::Function* cxaBeginCatchFunction;
#endif

		EmitModuleContext(const Module& inModule, ModuleInstance* inModuleInstance);

		std::shared_ptr<llvm::Module> emit();

		inline llvm::Function* getLLVMIntrinsic(
			llvm::ArrayRef<llvm::Type*> typeArguments,
			llvm::Intrinsic::ID id)
		{
			return llvm::Intrinsic::getDeclaration(llvmModule, id, typeArguments);
		}
	};

}