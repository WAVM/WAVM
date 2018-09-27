#include <stddef.h>
#include <memory>
#include <vector>

#include "EmitFunctionContext.h"
#include "EmitModuleContext.h"
#include "LLVMJITPrivate.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Exception.h"
#include "WAVM/Runtime/RuntimeData.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;
using namespace WAVM::Runtime;

void EmitFunctionContext::endTry()
{
	wavmAssert(tryStack.size());
	tryStack.pop_back();
	catchStack.pop_back();
}

void EmitFunctionContext::endCatch()
{
	wavmAssert(catchStack.size());
	if(!USE_WINDOWS_SEH)
	{
		CatchContext& catchContext = catchStack.back();

		irBuilder.SetInsertPoint(catchContext.nextHandlerBlock);

		emitRuntimeIntrinsic(
			"rethrowException",
			FunctionType(TypeTuple{}, TypeTuple{inferValueType<Iptr>()}),
			{irBuilder.CreatePtrToInt(catchContext.exceptionPointer, llvmContext.iptrType)});

		irBuilder.CreateUnreachable();
	}

	catchStack.pop_back();
}

llvm::BasicBlock* EmitFunctionContext::getInnermostUnwindToBlock()
{
	if(tryStack.size()) { return tryStack.back().unwindToBlock; }
	else
	{
		return nullptr;
	}
}

void EmitFunctionContext::try_(ControlStructureImm imm)
{
	if(USE_WINDOWS_SEH)
	{
		FunctionType blockType = resolveBlockType(irModule, imm.type);

		auto catchSwitchBlock = llvm::BasicBlock::Create(llvmContext, "catchSwitch", function);
		auto originalInsertBlock = irBuilder.GetInsertBlock();
		irBuilder.SetInsertPoint(catchSwitchBlock);
		auto catchSwitchInst
			= irBuilder.CreateCatchSwitch(llvm::ConstantTokenNone::get(llvmContext), nullptr, 1);
		irBuilder.SetInsertPoint(originalInsertBlock);
		tryStack.push_back(TryContext{catchSwitchBlock});
		catchStack.push_back(CatchContext{catchSwitchInst, nullptr, nullptr, nullptr, nullptr});

		// Create an end try+phi for the try result.
		auto endBlock = llvm::BasicBlock::Create(llvmContext, "tryEnd", function);
		auto endPHIs = createPHIs(endBlock, blockType.results());

		// Pop the try arguments.
		llvm::Value** tryArgs
			= (llvm::Value**)alloca(sizeof(llvm::Value*) * blockType.params().size());
		popMultiple(tryArgs, blockType.params().size());

		// Push a control context that ends at the end block/phi.
		pushControlStack(ControlContext::Type::try_, blockType.results(), endBlock, endPHIs);

		// Push a branch target for the end block/phi.
		pushBranchTarget(blockType.results(), endBlock, endPHIs);

		// Repush the try arguments.
		pushMultiple(tryArgs, blockType.params().size());

		// Call a dummy function to workaround a LLVM bug on Windows with the recoverfp intrinsic if
		// the try block doesn't contain any calls.
		if(!moduleContext.tryPrologueDummyFunction)
		{
			moduleContext.tryPrologueDummyFunction = llvm::Function::Create(
				llvm::FunctionType::get(llvm::Type::getVoidTy(llvmContext), false),
				llvm::GlobalValue::LinkageTypes::PrivateLinkage,
				"__try_prologue",
				moduleContext.llvmModule);
			auto entryBasicBlock = llvm::BasicBlock::Create(
				llvmContext, "entry", moduleContext.tryPrologueDummyFunction);
			llvm::IRBuilder<> dummyIRBuilder(llvmContext);
			dummyIRBuilder.SetInsertPoint(entryBasicBlock);
			dummyIRBuilder.CreateRetVoid();
		}
		emitCallOrInvoke(moduleContext.tryPrologueDummyFunction,
						 {},
						 FunctionType(),
						 CallingConvention::c,
						 getInnermostUnwindToBlock());
	}
	else
	{
		FunctionType blockType = resolveBlockType(irModule, imm.type);

		auto landingPadBlock = llvm::BasicBlock::Create(llvmContext, "landingPad", function);
		auto originalInsertBlock = irBuilder.GetInsertBlock();
		irBuilder.SetInsertPoint(landingPadBlock);
		auto landingPadInst = irBuilder.CreateLandingPad(
			llvm::StructType::get(llvmContext, {llvmContext.i8PtrType, llvmContext.i32Type}), 1);
		auto exceptionPointer = loadFromUntypedPointer(
			irBuilder.CreateCall(moduleContext.cxaBeginCatchFunction,
								 {irBuilder.CreateExtractValue(landingPadInst, {0})}),
			llvmContext.i8Type->getPointerTo());
		auto exceptionTypeId = loadFromUntypedPointer(
			irBuilder.CreateInBoundsGEP(
				exceptionPointer,
				{emitLiteral(llvmContext, Uptr(offsetof(ExceptionData, typeId)))}),
			llvmContext.iptrType);

		irBuilder.SetInsertPoint(originalInsertBlock);
		tryStack.push_back(TryContext{landingPadBlock});
		catchStack.push_back(CatchContext{
			nullptr, landingPadInst, landingPadBlock, exceptionTypeId, exceptionPointer});

		// Add the platform exception type to the landing pad's type filter.
		landingPadInst->addClause(moduleContext.userExceptionTypeInfo);

		// Create an end try+phi for the try result.
		auto endBlock = llvm::BasicBlock::Create(llvmContext, "tryEnd", function);
		auto endPHIs = createPHIs(endBlock, blockType.results());

		// Pop the try arguments.
		llvm::Value** tryArgs
			= (llvm::Value**)alloca(sizeof(llvm::Value*) * blockType.params().size());
		popMultiple(tryArgs, blockType.params().size());

		// Push a control context that ends at the end block/phi.
		pushControlStack(ControlContext::Type::try_, blockType.results(), endBlock, endPHIs);

		// Push a branch target for the end block/phi.
		pushBranchTarget(blockType.results(), endBlock, endPHIs);

		// Repush the try arguments.
		pushMultiple(tryArgs, blockType.params().size());
	}
}

static llvm::Function* createSEHFilterFunction(LLVMContext& llvmContext,
											   EmitFunctionContext& functionContext,
											   llvm::Constant* catchTypeId,
											   llvm::Value*& outExceptionDataAlloca)
{
	// Insert an alloca for the exception point at the beginning of the function, and add it as a
	// localescape.
	llvm::BasicBlock* savedInsertBlock = functionContext.irBuilder.GetInsertBlock();
	if(!functionContext.localEscapeBlock)
	{
		functionContext.localEscapeBlock
			= llvm::BasicBlock::Create(llvmContext, "alloca", functionContext.function);
	}
	functionContext.irBuilder.SetInsertPoint(functionContext.localEscapeBlock);
	const Uptr exceptionDataLocalEscapeIndex = functionContext.pendingLocalEscapes.size();
	outExceptionDataAlloca = functionContext.irBuilder.CreateAlloca(llvmContext.i8PtrType);
	functionContext.pendingLocalEscapes.push_back(outExceptionDataAlloca);
	functionContext.irBuilder.SetInsertPoint(savedInsertBlock);

	// Create a SEH filter function that decides whether to handle an exception.
	llvm::FunctionType* filterFunctionType = llvm::FunctionType::get(
		llvmContext.i32Type, {llvmContext.i8PtrType, llvmContext.i8PtrType}, false);
	auto filterFunction = llvm::Function::Create(filterFunctionType,
												 llvm::GlobalValue::LinkageTypes::PrivateLinkage,
												 "sehFilter",
												 functionContext.moduleContext.llvmModule);
	auto filterEntryBasicBlock = llvm::BasicBlock::Create(llvmContext, "entry", filterFunction);
	auto argIt = filterFunction->arg_begin();
	llvm::IRBuilder<> filterIRBuilder(filterEntryBasicBlock);

	// Get the pointer to the Windows EXCEPTION_RECORD struct.
	auto exceptionPointersArg = filterIRBuilder.CreatePointerCast(
		(llvm::Argument*)&(*argIt++), llvmContext.exceptionPointersStructType->getPointerTo());
	auto exceptionRecordPointer = filterIRBuilder.CreateLoad(filterIRBuilder.CreateInBoundsGEP(
		exceptionPointersArg,
		{emitLiteral(llvmContext, U32(0)), emitLiteral(llvmContext, U32(0))}));

	// Recover the frame pointer of the catching frame, and the escaped local to write the exception
	// pointer to.
	auto framePointer = filterIRBuilder.CreateCall(
		functionContext.moduleContext.getLLVMIntrinsic({}, llvm::Intrinsic::x86_seh_recoverfp),
		{filterIRBuilder.CreatePointerCast(functionContext.function, llvmContext.i8PtrType),
		 (llvm::Value*)&(*argIt++)});
	auto exceptionDataAlloca = filterIRBuilder.CreateCall(
		functionContext.moduleContext.getLLVMIntrinsic({}, llvm::Intrinsic::localrecover),
		{filterIRBuilder.CreatePointerCast(functionContext.function, llvmContext.i8PtrType),
		 framePointer,
		 emitLiteral(llvmContext, I32(exceptionDataLocalEscapeIndex))});

	// Check if the exception code is SEH_WAVM_EXCEPTION
	// If it does not match, return 0 from the filter function.
	auto nonWebAssemblyExceptionBlock
		= llvm::BasicBlock::Create(llvmContext, "nonWebAssemblyException", filterFunction);
	auto exceptionTypeCheckBlock
		= llvm::BasicBlock::Create(llvmContext, "exceptionTypeCheck", filterFunction);
	auto exceptionCode = filterIRBuilder.CreateLoad(filterIRBuilder.CreateInBoundsGEP(
		exceptionRecordPointer,
		{emitLiteral(llvmContext, U32(0)), emitLiteral(llvmContext, U32(0))}));
	auto isWebAssemblyException = filterIRBuilder.CreateICmpEQ(
		exceptionCode, emitLiteral(llvmContext, I32(Platform::SEH_WAVM_EXCEPTION)));
	filterIRBuilder.CreateCondBr(
		isWebAssemblyException, exceptionTypeCheckBlock, nonWebAssemblyExceptionBlock);
	filterIRBuilder.SetInsertPoint(nonWebAssemblyExceptionBlock);
	filterIRBuilder.CreateRet(emitLiteral(llvmContext, I32(0)));
	filterIRBuilder.SetInsertPoint(exceptionTypeCheckBlock);

	// Copy the pointer to the exception data to the alloca in the catch frame.
	auto exceptionDataGEP = filterIRBuilder.CreateInBoundsGEP(exceptionRecordPointer,
															  {emitLiteral(llvmContext, U32(0)),
															   emitLiteral(llvmContext, U32(5)),
															   emitLiteral(llvmContext, U32(0))});
	auto exceptionData = filterIRBuilder.CreateLoad(exceptionDataGEP);
	filterIRBuilder.CreateStore(exceptionData,
								filterIRBuilder.CreatePointerCast(
									exceptionDataAlloca, llvmContext.i64Type->getPointerTo()));

	if(!catchTypeId)
	{
		// If the exception code is SEH_WAVM_EXCEPTION, and the exception is a user exception,
		// return 1 from the filter function.
		auto isUserExceptionI8 = filterIRBuilder.CreateLoad(filterIRBuilder.CreateInBoundsGEP(
			filterIRBuilder.CreateIntToPtr(exceptionData, llvmContext.i8Type->getPointerTo()),
			{emitLiteral(llvmContext, Uptr(offsetof(ExceptionData, isUserException)))}));
		filterIRBuilder.CreateRet(
			filterIRBuilder.CreateZExt(isUserExceptionI8, llvmContext.i32Type));
	}
	else
	{
		// If the exception code is SEH_WAVM_EXCEPTION, and the thrown exception matches the catch
		// exception type, return 1 from the filter function.
		auto exceptionTypeId = filterIRBuilder.CreateLoad(
			filterIRBuilder.CreateIntToPtr(exceptionData, llvmContext.iptrType->getPointerTo()));
		auto isExpectedTypeId = filterIRBuilder.CreateICmpEQ(exceptionTypeId, catchTypeId);
		filterIRBuilder.CreateRet(
			filterIRBuilder.CreateZExt(isExpectedTypeId, llvmContext.i32Type));
	}

	return filterFunction;
}

void EmitFunctionContext::catch_(ExceptionTypeImm imm)
{
	wavmAssert(controlStack.size());
	wavmAssert(catchStack.size());
	ControlContext& controlContext = controlStack.back();
	CatchContext& catchContext = catchStack.back();
	wavmAssert(controlContext.type == ControlContext::Type::try_
			   || controlContext.type == ControlContext::Type::catch_);
	if(controlContext.type == ControlContext::Type::try_)
	{
		wavmAssert(tryStack.size());
		tryStack.pop_back();
	}

	branchToEndOfControlContext();

	// Look up the exception type instance to be caught
	wavmAssert(imm.exceptionTypeIndex < moduleContext.exceptionTypeIds.size());
	const IR::ExceptionType catchType = irModule.exceptionTypes.getType(imm.exceptionTypeIndex);
	llvm::Constant* catchTypeId = moduleContext.exceptionTypeIds[imm.exceptionTypeIndex];

	if(USE_WINDOWS_SEH)
	{
		// Create a filter function that returns 1 for the specific exception type this instruction
		// catches.
		llvm::Value* exceptionDataAlloca = nullptr;

		auto filterFunction
			= createSEHFilterFunction(llvmContext, *this, catchTypeId, exceptionDataAlloca);

		// Create a block+catchpad that the catchswitch will transfer control to if the filter
		// function returns 1.
		auto catchPadBlock = llvm::BasicBlock::Create(llvmContext, "catchPad", function);
		catchContext.catchSwitchInst->addHandler(catchPadBlock);
		irBuilder.SetInsertPoint(catchPadBlock);
		auto catchPadInst
			= irBuilder.CreateCatchPad(catchContext.catchSwitchInst, {filterFunction});

		// Create a catchret that immediately returns from the catch "funclet" to a new non-funclet
		// basic block.
		auto catchBlock = llvm::BasicBlock::Create(llvmContext, "catch", function);
		irBuilder.CreateCatchRet(catchPadInst, catchBlock);
		irBuilder.SetInsertPoint(catchBlock);

		catchContext.exceptionPointer = irBuilder.CreateLoad(exceptionDataAlloca);
		for(Uptr argumentIndex = 0; argumentIndex < catchType.params.size(); ++argumentIndex)
		{
			const ValueType parameters = catchType.params[argumentIndex];
			auto argument = loadFromUntypedPointer(
				irBuilder.CreateInBoundsGEP(
					catchContext.exceptionPointer,
					{emitLiteral(llvmContext,
								 offsetof(ExceptionData, arguments)
									 + (catchType.params.size() - argumentIndex - 1)
										   * sizeof(ExceptionData::arguments[0]))}),
				asLLVMType(llvmContext, parameters),
				sizeof(ExceptionData::arguments[0]));
			push(argument);
		}
	}
	else
	{
		irBuilder.SetInsertPoint(catchContext.nextHandlerBlock);
		auto isExceptionType = irBuilder.CreateICmpEQ(catchContext.exceptionTypeId, catchTypeId);

		auto catchBlock = llvm::BasicBlock::Create(llvmContext, "catch", function);
		auto unhandledBlock = llvm::BasicBlock::Create(llvmContext, "unhandled", function);
		irBuilder.CreateCondBr(isExceptionType, catchBlock, unhandledBlock);
		catchContext.nextHandlerBlock = unhandledBlock;
		irBuilder.SetInsertPoint(catchBlock);

		for(Iptr argumentIndex = catchType.params.size() - 1; argumentIndex >= 0; --argumentIndex)
		{
			const ValueType parameters = catchType.params[argumentIndex];
			const Uptr argumentOffset = offsetof(ExceptionData, arguments)
										+ sizeof(ExceptionData::arguments[0]) * argumentIndex;
			auto argument = loadFromUntypedPointer(
				irBuilder.CreateInBoundsGEP(catchContext.exceptionPointer,
											{emitLiteral(llvmContext, argumentOffset)}),
				asLLVMType(llvmContext, parameters),
				sizeof(ExceptionData::arguments[0]));
			push(argument);
		}
	}

	// Change the top of the control stack to a catch clause.
	controlContext.type = ControlContext::Type::catch_;
	controlContext.isReachable = true;
}
void EmitFunctionContext::catch_all(NoImm)
{
	wavmAssert(controlStack.size());
	wavmAssert(catchStack.size());
	ControlContext& controlContext = controlStack.back();
	CatchContext& catchContext = catchStack.back();
	wavmAssert(controlContext.type == ControlContext::Type::try_
			   || controlContext.type == ControlContext::Type::catch_);
	if(controlContext.type == ControlContext::Type::try_)
	{
		wavmAssert(tryStack.size());
		tryStack.pop_back();
	}

	branchToEndOfControlContext();

	if(USE_WINDOWS_SEH)
	{
		// Create a filter function that returns 1 for any WebAssembly exception.
		llvm::Value* exceptionDataAlloca = nullptr;
		llvm::Function* filterFunction
			= createSEHFilterFunction(llvmContext, *this, nullptr, exceptionDataAlloca);

		// Create a block+catchpad that the catchswitch will transfer control to if the filter
		// function returns 1.
		auto catchPadBlock = llvm::BasicBlock::Create(llvmContext, "catchPad", function);
		catchContext.catchSwitchInst->addHandler(catchPadBlock);
		irBuilder.SetInsertPoint(catchPadBlock);
		auto catchPadInst
			= irBuilder.CreateCatchPad(catchContext.catchSwitchInst, {filterFunction});

		// Create a catchret that immediately returns from the catch "funclet" to a new non-funclet
		// basic block.
		auto catchBlock = llvm::BasicBlock::Create(llvmContext, "catch", function);
		irBuilder.CreateCatchRet(catchPadInst, catchBlock);
		irBuilder.SetInsertPoint(catchBlock);

		catchContext.exceptionPointer = irBuilder.CreateLoad(exceptionDataAlloca);
	}
	else
	{
		irBuilder.SetInsertPoint(catchContext.nextHandlerBlock);
		auto isUserExceptionType = irBuilder.CreateICmpNE(
			loadFromUntypedPointer(
				irBuilder.CreateInBoundsGEP(
					catchContext.exceptionPointer,
					{emitLiteral(llvmContext, Uptr(offsetof(ExceptionData, isUserException)))}),
				llvmContext.i8Type),
			llvm::ConstantInt::get(llvmContext.i8Type, llvm::APInt(8, 0, false)));

		auto catchBlock = llvm::BasicBlock::Create(llvmContext, "catch", function);
		auto unhandledBlock = llvm::BasicBlock::Create(llvmContext, "unhandled", function);
		irBuilder.CreateCondBr(isUserExceptionType, catchBlock, unhandledBlock);
		catchContext.nextHandlerBlock = unhandledBlock;
		irBuilder.SetInsertPoint(catchBlock);
	}

	// Change the top of the control stack to a catch clause.
	controlContext.type = ControlContext::Type::catch_;
	controlContext.isReachable = true;
}

void EmitFunctionContext::throw_(ExceptionTypeImm imm)
{
	const IR::ExceptionType& exceptionType
		= irModule.exceptionTypes.getType(imm.exceptionTypeIndex);

	const Uptr numArgs = exceptionType.params.size();
	const Uptr numArgBytes = numArgs * sizeof(UntaggedValue);
	auto argBaseAddress
		= irBuilder.CreateAlloca(llvmContext.i8Type, emitLiteral(llvmContext, numArgBytes));
	argBaseAddress->setAlignment(sizeof(UntaggedValue));

	for(Uptr argIndex = 0; argIndex < exceptionType.params.size(); ++argIndex)
	{
		auto elementValue = pop();
		storeToUntypedPointer(
			elementValue,
			irBuilder.CreatePointerCast(
				irBuilder.CreateInBoundsGEP(
					argBaseAddress,
					{emitLiteral(llvmContext, (numArgs - argIndex - 1) * sizeof(UntaggedValue))}),
				elementValue->getType()->getPointerTo()),
			sizeof(UntaggedValue));
	}

	llvm::Value* exceptionTypeId = moduleContext.exceptionTypeIds[imm.exceptionTypeIndex];
	llvm::Value* argsPointerAsInt = irBuilder.CreatePtrToInt(argBaseAddress, llvmContext.iptrType);

	emitRuntimeIntrinsic(
		"throwException",
		FunctionType(TypeTuple{},
					 TypeTuple{inferValueType<Iptr>(), inferValueType<Iptr>(), ValueType::i32}),
		{exceptionTypeId, argsPointerAsInt, emitLiteral(llvmContext, I32(1))});

	irBuilder.CreateUnreachable();
	enterUnreachable();
}
void EmitFunctionContext::rethrow(RethrowImm imm)
{
	wavmAssert(imm.catchDepth < catchStack.size());
	CatchContext& catchContext = catchStack[catchStack.size() - imm.catchDepth - 1];
	emitRuntimeIntrinsic(
		"rethrowException",
		FunctionType(TypeTuple{}, TypeTuple{inferValueType<Iptr>()}),
		{irBuilder.CreatePtrToInt(catchContext.exceptionPointer, llvmContext.iptrType)});

	irBuilder.CreateUnreachable();
	enterUnreachable();
}
