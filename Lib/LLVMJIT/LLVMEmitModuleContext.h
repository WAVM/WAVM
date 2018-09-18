#pragma once

#include "LLVMJITPrivate.h"
#include "WAVM/IR/Module.h"

#include "LLVMPreInclude.h"

#include "llvm/IR/DIBuilder.h"

#include "LLVMPostInclude.h"

namespace WAVM { namespace LLVMJIT {
	struct EmitModuleContext
	{
		const IR::Module& irModule;

		LLVMContext& llvmContext;
		llvm::Module* llvmModule;
		std::vector<llvm::Constant*> typeIds;
		std::vector<llvm::Constant*> functionDefInstances;
		std::vector<llvm::Function*> functions;
		std::vector<llvm::Constant*> tableOffsets;
		std::vector<llvm::Constant*> memoryOffsets;
		std::vector<llvm::Constant*> globals;
		std::vector<llvm::Constant*> exceptionTypeInstances;

		llvm::Constant* defaultMemoryOffset;
		llvm::Constant* defaultTableOffset;

		llvm::Constant* moduleInstancePointer;
		llvm::Constant* tableReferenceBias;

		llvm::DIBuilder diBuilder;
		llvm::DICompileUnit* diCompileUnit;
		llvm::DIFile* diModuleScope;

		llvm::DIType* diValueTypes[(Uptr)IR::ValueType::num];

		llvm::MDNode* likelyFalseBranchWeights;
		llvm::MDNode* likelyTrueBranchWeights;

		llvm::Value* fpRoundingModeMetadata;
		llvm::Value* fpExceptionMetadata;

		llvm::Function* tryPrologueDummyFunction;
		llvm::Function* cxaBeginCatchFunction;

		EmitModuleContext(const IR::Module& inModule,
						  LLVMContext& inLLVMContext,
						  llvm::Module* inLLVMModule);

		inline llvm::Function* getLLVMIntrinsic(llvm::ArrayRef<llvm::Type*> typeArguments,
												llvm::Intrinsic::ID id)
		{
			return llvm::Intrinsic::getDeclaration(llvmModule, id, typeArguments);
		}
	};
}}
