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

		llvm::Module* llvmModule;
		std::vector<llvm::Function*> functionDefs;

		llvm::DIBuilder diBuilder;
		llvm::DICompileUnit* diCompileUnit;
		llvm::DIFile* diModuleScope;

		llvm::DIType* diValueTypes[(Uptr)ValueType::num];

		llvm::MDNode* likelyFalseBranchWeights;
		llvm::MDNode* likelyTrueBranchWeights;

		llvm::Value* fpRoundingModeMetadata;
		llvm::Value* fpExceptionMetadata;

		llvm::Function* tryPrologueDummyFunction;
		llvm::Function* cxaBeginCatchFunction;

		EmitModuleContext(const Module& inModule,
						  ModuleInstance* inModuleInstance,
						  llvm::Module* inLLVMModule);

		inline llvm::Function* getLLVMIntrinsic(llvm::ArrayRef<llvm::Type*> typeArguments,
												llvm::Intrinsic::ID id)
		{
			return llvm::Intrinsic::getDeclaration(llvmModule, id, typeArguments);
		}
	};
}
