#include "IR/Operators.h"
#include "Inline/Assert.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMEmitModuleContext.h"
#include "LLVMJIT.h"

using namespace LLVMJIT;
using namespace IR;

void EmitFunctionContext::endTry()
{
	wavmAssert(tryStack.size());
	tryStack.pop_back();
	catchStack.pop_back();
}

void EmitFunctionContext::endCatch()
{
	wavmAssert(catchStack.size());
#ifndef _WIN64
	CatchContext& catchContext = catchStack.back();

	irBuilder.SetInsertPoint(catchContext.nextHandlerBlock);
	emitThrow(
		loadFromUntypedPointer(catchContext.exceptionPointer, llvmI64Type),
		irBuilder.CreatePtrToInt(
			irBuilder.CreateInBoundsGEP(
				catchContext.exceptionPointer,
				{emitLiteral(I32(offsetof(ExceptionData, arguments)))}),
			llvmI64Type),
		false);
	irBuilder.CreateUnreachable();
#endif

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

#ifdef _WIN32

void EmitFunctionContext::try_(ControlStructureImm imm)
{
	FunctionType blockType = resolveBlockType(module, imm.type);

	auto catchSwitchBlock    = llvm::BasicBlock::Create(*llvmContext, "catchSwitch", llvmFunction);
	auto originalInsertBlock = irBuilder.GetInsertBlock();
	irBuilder.SetInsertPoint(catchSwitchBlock);
	auto catchSwitchInst
		= irBuilder.CreateCatchSwitch(llvm::ConstantTokenNone::get(*llvmContext), nullptr, 1);
	irBuilder.SetInsertPoint(originalInsertBlock);
	tryStack.push_back(TryContext{catchSwitchBlock});
	catchStack.push_back(CatchContext{catchSwitchInst, nullptr});

	// Create an end try+phi for the try result.
	auto endBlock = llvm::BasicBlock::Create(*llvmContext, "tryEnd", llvmFunction);
	auto endPHIs  = createPHIs(endBlock, blockType.results());

	// Pop the try arguments.
	llvm::Value** tryArgs = (llvm::Value**)alloca(sizeof(llvm::Value*) * blockType.params().size());
	popMultiple(tryArgs, blockType.params().size());

	// Push a control context that ends at the end block/phi.
	pushControlStack(ControlContext::Type::try_, blockType.results(), endBlock, endPHIs);

	// Push a branch target for the end block/phi.
	pushBranchTarget(blockType.results(), endBlock, endPHIs);

	// Repush the try arguments.
	pushMultiple(tryArgs, blockType.params().size());

	// Call a dummy function to workaround a LLVM bug on Windows with the recoverfp intrinsic if the
	// try block doesn't contain any calls.
	if(!moduleContext.tryPrologueDummyFunction)
	{
		moduleContext.tryPrologueDummyFunction = llvm::Function::Create(
			llvm::FunctionType::get(llvmVoidType, false),
			llvm::GlobalValue::LinkageTypes::InternalLinkage,
			"__try_prologue",
			moduleContext.llvmModule);
		auto entryBasicBlock = llvm::BasicBlock::Create(
			*llvmContext, "entry", moduleContext.tryPrologueDummyFunction);
		llvm::IRBuilder<> dummyIRBuilder(*llvmContext);
		dummyIRBuilder.SetInsertPoint(entryBasicBlock);
		dummyIRBuilder.CreateRetVoid();
	}
	emitCallOrInvoke(
		moduleContext.tryPrologueDummyFunction,
		{},
		FunctionType(),
		CallingConvention::c,
		getInnermostUnwindToBlock());
}

static llvm::Function* createSEHFilterFunction(
	EmitFunctionContext& functionContext,
	const Runtime::ExceptionTypeInstance* catchTypeInstance,
	llvm::Value*& outExceptionDataAlloca)
{
	// Insert an alloca for the exception point at the beginning of the function, and add it as a
	// localescape.
	llvm::BasicBlock* savedInsertBlock = functionContext.irBuilder.GetInsertBlock();
	if(!functionContext.localEscapeBlock)
	{
		functionContext.localEscapeBlock
			= llvm::BasicBlock::Create(*llvmContext, "alloca", functionContext.llvmFunction);
	}
	functionContext.irBuilder.SetInsertPoint(functionContext.localEscapeBlock);
	const Uptr exceptionDataLocalEscapeIndex = functionContext.pendingLocalEscapes.size();
	outExceptionDataAlloca = functionContext.irBuilder.CreateAlloca(llvmI8PtrType);
	functionContext.pendingLocalEscapes.push_back(outExceptionDataAlloca);
	functionContext.irBuilder.SetInsertPoint(savedInsertBlock);

	// Create a SEH filter function that decides whether to handle an exception.
	llvm::FunctionType* filterFunctionType
		= llvm::FunctionType::get(llvmI32Type, {llvmI8PtrType, llvmI8PtrType}, false);
	auto filterFunction = llvm::Function::Create(
		filterFunctionType,
		llvm::GlobalValue::LinkageTypes::InternalLinkage,
		"sehFilter",
		functionContext.moduleContext.llvmModule);
	auto filterEntryBasicBlock = llvm::BasicBlock::Create(*llvmContext, "entry", filterFunction);
	auto argIt                 = filterFunction->arg_begin();
	llvm::IRBuilder<> filterIRBuilder(filterEntryBasicBlock);

	// Get the pointer to the Windows EXCEPTION_RECORD struct.
	auto exceptionPointersArg = filterIRBuilder.CreatePointerCast(
		(llvm::Argument*)&(*argIt++), llvmExceptionPointersStructType->getPointerTo());
	auto exceptionRecordPointer = filterIRBuilder.CreateLoad(filterIRBuilder.CreateInBoundsGEP(
		exceptionPointersArg, {emitLiteral(U32(0)), emitLiteral(U32(0))}));

	// Recover the frame pointer of the catching frame, and the escaped local to write the exception
	// pointer to.
	auto framePointer = filterIRBuilder.CreateCall(
		functionContext.moduleContext.getLLVMIntrinsic({}, llvm::Intrinsic::x86_seh_recoverfp),
		{filterIRBuilder.CreatePointerCast(functionContext.llvmFunction, llvmI8PtrType),
		 (llvm::Value*)&(*argIt++)});
	auto exceptionDataAlloca = filterIRBuilder.CreateCall(
		functionContext.moduleContext.getLLVMIntrinsic({}, llvm::Intrinsic::localrecover),
		{filterIRBuilder.CreatePointerCast(functionContext.llvmFunction, llvmI8PtrType),
		 framePointer,
		 emitLiteral(I32(exceptionDataLocalEscapeIndex))});

	// Check if the exception code is SEH_WAVM_EXCEPTION
	// If it does not match, return 0 from the filter function.
	auto nonWebAssemblyExceptionBlock
		= llvm::BasicBlock::Create(*llvmContext, "nonWebAssemblyException", filterFunction);
	auto exceptionTypeCheckBlock
		= llvm::BasicBlock::Create(*llvmContext, "exceptionTypeCheck", filterFunction);
	auto exceptionCode          = filterIRBuilder.CreateLoad(filterIRBuilder.CreateInBoundsGEP(
        exceptionRecordPointer, {emitLiteral(U32(0)), emitLiteral(U32(0))}));
	auto isWebAssemblyException = filterIRBuilder.CreateICmpEQ(
		exceptionCode, emitLiteral(I32(Platform::SEH_WAVM_EXCEPTION)));
	filterIRBuilder.CreateCondBr(
		isWebAssemblyException, exceptionTypeCheckBlock, nonWebAssemblyExceptionBlock);
	filterIRBuilder.SetInsertPoint(nonWebAssemblyExceptionBlock);
	filterIRBuilder.CreateRet(emitLiteral(I32(0)));
	filterIRBuilder.SetInsertPoint(exceptionTypeCheckBlock);

	// Copy the pointer to the exception data to the alloca in the catch frame.
	auto exceptionDataGEP = filterIRBuilder.CreateInBoundsGEP(
		exceptionRecordPointer, {emitLiteral(U32(0)), emitLiteral(U32(5)), emitLiteral(U32(0))});
	auto exceptionData = filterIRBuilder.CreateLoad(exceptionDataGEP);
	filterIRBuilder.CreateStore(
		exceptionData,
		filterIRBuilder.CreatePointerCast(exceptionDataAlloca, llvmI64Type->getPointerTo()));

	if(!catchTypeInstance)
	{
		// If the exception code is SEH_WAVM_EXCEPTION, and the exception is a user exception,
		// return 1 from the filter function.
		auto isUserExceptionI8 = filterIRBuilder.CreateLoad(filterIRBuilder.CreateInBoundsGEP(
			filterIRBuilder.CreateIntToPtr(exceptionData, llvmI8Type->getPointerTo()),
			{emitLiteral(offsetof(ExceptionData, isUserException))}));
		filterIRBuilder.CreateRet(filterIRBuilder.CreateZExt(isUserExceptionI8, llvmI32Type));
	}
	else
	{
		// If the exception code is SEH_WAVM_EXCEPTION, and the thrown exception matches the catch
		// exception type, return 1 from the filter function.
		auto exceptionTypeInstance = filterIRBuilder.CreateLoad(
			filterIRBuilder.CreateIntToPtr(exceptionData, llvmI64Type->getPointerTo()));
		auto isExpectedTypeInstance = filterIRBuilder.CreateICmpEQ(
			exceptionTypeInstance, emitLiteral(reinterpret_cast<I64>(catchTypeInstance)));
		filterIRBuilder.CreateRet(filterIRBuilder.CreateZExt(isExpectedTypeInstance, llvmI32Type));
	}

	return filterFunction;
}

void EmitFunctionContext::catch_(CatchImm imm)
{
	wavmAssert(controlStack.size());
	wavmAssert(catchStack.size());
	ControlContext& controlContext = controlStack.back();
	CatchContext& catchContext     = catchStack.back();
	wavmAssert(
		controlContext.type == ControlContext::Type::try_
		|| controlContext.type == ControlContext::Type::catch_);
	if(controlContext.type == ControlContext::Type::try_)
	{
		wavmAssert(tryStack.size());
		tryStack.pop_back();
	}

	branchToEndOfControlContext();

	// Look up the exception type instance to be caught
	wavmAssert(
		imm.exceptionTypeIndex < moduleContext.moduleInstance->exceptionTypeInstances.size());
	const Runtime::ExceptionTypeInstance* catchTypeInstance
		= moduleContext.moduleInstance->exceptionTypeInstances[imm.exceptionTypeIndex];

	// Create a filter function that returns 1 for the specific exception type this instruction
	// catches.
	llvm::Value* exceptionDataAlloca = nullptr;
	auto filterFunction = createSEHFilterFunction(*this, catchTypeInstance, exceptionDataAlloca);

	// Create a block+catchpad that the catchswitch will transfer control to if the filter function
	// returns 1.
	auto catchPadBlock = llvm::BasicBlock::Create(*llvmContext, "catchPad", llvmFunction);
	catchContext.catchSwitchInst->addHandler(catchPadBlock);
	irBuilder.SetInsertPoint(catchPadBlock);
	auto catchPadInst = irBuilder.CreateCatchPad(catchContext.catchSwitchInst, {filterFunction});

	// Create a catchret that immediately returns from the catch "funclet" to a new non-funclet
	// basic block.
	auto catchBlock = llvm::BasicBlock::Create(*llvmContext, "catch", llvmFunction);
	irBuilder.CreateCatchRet(catchPadInst, catchBlock);
	irBuilder.SetInsertPoint(catchBlock);

	catchContext.exceptionPointer = irBuilder.CreateLoad(exceptionDataAlloca);
	for(Uptr argumentIndex = 0; argumentIndex < catchTypeInstance->type.params.size();
		++argumentIndex)
	{
		const ValueType parameters = catchTypeInstance->type.params[argumentIndex];
		auto argument              = loadFromUntypedPointer(
            irBuilder.CreateInBoundsGEP(
                catchContext.exceptionPointer,
                {emitLiteral(offsetof(
                    ExceptionData,
                    arguments[catchTypeInstance->type.params.size() - argumentIndex - 1]))}),
            asLLVMType(parameters));
		push(argument);
	}

	// Change the top of the control stack to a catch clause.
	controlContext.type        = ControlContext::Type::catch_;
	controlContext.isReachable = true;
}
void EmitFunctionContext::catch_all(NoImm)
{
	wavmAssert(controlStack.size());
	wavmAssert(catchStack.size());
	ControlContext& controlContext = controlStack.back();
	CatchContext& catchContext     = catchStack.back();
	wavmAssert(
		controlContext.type == ControlContext::Type::try_
		|| controlContext.type == ControlContext::Type::catch_);
	if(controlContext.type == ControlContext::Type::try_)
	{
		wavmAssert(tryStack.size());
		tryStack.pop_back();
	}

	branchToEndOfControlContext();

	// Create a filter function that returns 1 for any WebAssembly exception.
	llvm::Value* exceptionDataAlloca = nullptr;
	llvm::Function* filterFunction   = createSEHFilterFunction(*this, nullptr, exceptionDataAlloca);

	// Create a block+catchpad that the catchswitch will transfer control to if the filter function
	// returns 1.
	auto catchPadBlock = llvm::BasicBlock::Create(*llvmContext, "catchPad", llvmFunction);
	catchContext.catchSwitchInst->addHandler(catchPadBlock);
	irBuilder.SetInsertPoint(catchPadBlock);
	auto catchPadInst = irBuilder.CreateCatchPad(catchContext.catchSwitchInst, {filterFunction});

	// Create a catchret that immediately returns from the catch "funclet" to a new non-funclet
	// basic block.
	auto catchBlock = llvm::BasicBlock::Create(*llvmContext, "catch", llvmFunction);
	irBuilder.CreateCatchRet(catchPadInst, catchBlock);
	irBuilder.SetInsertPoint(catchBlock);

	catchContext.exceptionPointer = irBuilder.CreateLoad(exceptionDataAlloca);

	// Change the top of the control stack to a catch clause.
	controlContext.type        = ControlContext::Type::catch_;
	controlContext.isReachable = true;
}
#else
void EmitFunctionContext::try_(ControlStructureImm imm)
{
	FunctionType blockType = resolveBlockType(module, imm.type);

	auto landingPadBlock     = llvm::BasicBlock::Create(*llvmContext, "landingPad", llvmFunction);
	auto originalInsertBlock = irBuilder.GetInsertBlock();
	irBuilder.SetInsertPoint(landingPadBlock);
	auto landingPadInst = irBuilder.CreateLandingPad(
		llvm::StructType::get(*llvmContext, {llvmI8PtrType, llvmI32Type}), 1);
	auto exceptionPointer = loadFromUntypedPointer(
		irBuilder.CreateCall(
			moduleContext.cxaBeginCatchFunction,
			{irBuilder.CreateExtractValue(landingPadInst, {0})}),
		llvmI8Type->getPointerTo());
	auto exceptionTypeInstance = loadFromUntypedPointer(
		irBuilder.CreateInBoundsGEP(
			exceptionPointer, {emitLiteral(Uptr(offsetof(ExceptionData, typeInstance)))}),
		llvmI64Type);

	irBuilder.SetInsertPoint(originalInsertBlock);
	tryStack.push_back(TryContext{landingPadBlock});
	catchStack.push_back(
		CatchContext{landingPadInst, landingPadBlock, exceptionTypeInstance, exceptionPointer});

	// Add the platform exception type to the landing pad's type filter.
	auto platformExceptionTypeInfo = (llvm::Constant*)irBuilder.CreateIntToPtr(
		emitLiteral(reinterpret_cast<Uptr>(Platform::getUserExceptionTypeInfo())), llvmI8PtrType);
	landingPadInst->addClause(platformExceptionTypeInfo);

	// Create an end try+phi for the try result.
	auto endBlock = llvm::BasicBlock::Create(*llvmContext, "tryEnd", llvmFunction);
	auto endPHIs  = createPHIs(endBlock, blockType.results());

	// Pop the try arguments.
	llvm::Value** tryArgs = (llvm::Value**)alloca(sizeof(llvm::Value*) * blockType.params().size());
	popMultiple(tryArgs, blockType.params().size());

	// Push a control context that ends at the end block/phi.
	pushControlStack(ControlContext::Type::try_, blockType.results(), endBlock, endPHIs);

	// Push a branch target for the end block/phi.
	pushBranchTarget(blockType.results(), endBlock, endPHIs);

	// Repush the try arguments.
	pushMultiple(tryArgs, blockType.params().size());
}

void EmitFunctionContext::catch_(CatchImm imm)
{
	wavmAssert(controlStack.size());
	wavmAssert(catchStack.size());
	ControlContext& controlContext = controlStack.back();
	CatchContext& catchContext     = catchStack.back();
	wavmAssert(
		controlContext.type == ControlContext::Type::try_
		|| controlContext.type == ControlContext::Type::catch_);
	if(controlContext.type == ControlContext::Type::try_)
	{
		wavmAssert(tryStack.size());
		tryStack.pop_back();
	}

	branchToEndOfControlContext();

	// Look up the exception type instance to be caught
	wavmAssert(
		imm.exceptionTypeIndex < moduleContext.moduleInstance->exceptionTypeInstances.size());
	const Runtime::ExceptionTypeInstance* catchTypeInstance
		= moduleContext.moduleInstance->exceptionTypeInstances[imm.exceptionTypeIndex];

	irBuilder.SetInsertPoint(catchContext.nextHandlerBlock);
	auto isExceptionType = irBuilder.CreateICmpEQ(
		catchContext.exceptionTypeInstance, emitLiteral(reinterpret_cast<Uptr>(catchTypeInstance)));

	auto catchBlock     = llvm::BasicBlock::Create(*llvmContext, "catch", llvmFunction);
	auto unhandledBlock = llvm::BasicBlock::Create(*llvmContext, "unhandled", llvmFunction);
	irBuilder.CreateCondBr(isExceptionType, catchBlock, unhandledBlock);
	catchContext.nextHandlerBlock = unhandledBlock;
	irBuilder.SetInsertPoint(catchBlock);

	for(Iptr argumentIndex = catchTypeInstance->type.params.size() - 1; argumentIndex >= 0;
		--argumentIndex)
	{
		const ValueType parameters = catchTypeInstance->type.params[argumentIndex];
		const Uptr argumentOffset  = offsetof(ExceptionData, arguments)
									+ sizeof(ExceptionData::arguments[0]) * argumentIndex;
		auto argument = loadFromUntypedPointer(
			irBuilder.CreateInBoundsGEP(
				catchContext.exceptionPointer, {emitLiteral(argumentOffset)}),
			asLLVMType(parameters));
		push(argument);
	}

	// Change the top of the control stack to a catch clause.
	controlContext.type        = ControlContext::Type::catch_;
	controlContext.isReachable = true;
}
void EmitFunctionContext::catch_all(NoImm)
{
	wavmAssert(controlStack.size());
	wavmAssert(catchStack.size());
	ControlContext& controlContext = controlStack.back();
	CatchContext& catchContext     = catchStack.back();
	wavmAssert(
		controlContext.type == ControlContext::Type::try_
		|| controlContext.type == ControlContext::Type::catch_);
	if(controlContext.type == ControlContext::Type::try_)
	{
		wavmAssert(tryStack.size());
		tryStack.pop_back();
	}

	branchToEndOfControlContext();

	irBuilder.SetInsertPoint(catchContext.nextHandlerBlock);
	auto isUserExceptionType = irBuilder.CreateICmpNE(
		loadFromUntypedPointer(
			irBuilder.CreateInBoundsGEP(
				catchContext.exceptionPointer,
				{emitLiteral(Uptr(offsetof(ExceptionData, isUserException)))}),
			llvmI8Type),
		llvm::ConstantInt::get(llvmI8Type, llvm::APInt(8, 0, false)));

	auto catchBlock     = llvm::BasicBlock::Create(*llvmContext, "catch", llvmFunction);
	auto unhandledBlock = llvm::BasicBlock::Create(*llvmContext, "unhandled", llvmFunction);
	irBuilder.CreateCondBr(isUserExceptionType, catchBlock, unhandledBlock);
	catchContext.nextHandlerBlock = unhandledBlock;
	irBuilder.SetInsertPoint(catchBlock);

	// Change the top of the control stack to a catch clause.
	controlContext.type        = ControlContext::Type::catch_;
	controlContext.isReachable = true;
}
#endif

void EmitFunctionContext::emitThrow(
	llvm::Value* exceptionTypeInstanceI64,
	llvm::Value* argumentsPointerI64,
	bool isUserException)
{
	emitRuntimeIntrinsic(
		"throwException",
		FunctionType(TypeTuple{}, TypeTuple{ValueType::i64, ValueType::i64, ValueType::i32}),
		{exceptionTypeInstanceI64, argumentsPointerI64, emitLiteral(I32(isUserException ? 1 : 0))});
}

void EmitFunctionContext::throw_(ThrowImm imm)
{
	const Runtime::ExceptionTypeInstance* exceptionTypeInstance
		= moduleContext.moduleInstance->exceptionTypeInstances[imm.exceptionTypeIndex];

	const Uptr numArgs     = exceptionTypeInstance->type.params.size();
	const Uptr numArgBytes = numArgs * sizeof(UntaggedValue);
	auto argBaseAddress    = irBuilder.CreateAlloca(llvmI8Type, emitLiteral(numArgBytes));

	for(Uptr argIndex = 0; argIndex < exceptionTypeInstance->type.params.size(); ++argIndex)
	{
		auto elementValue = pop();
		irBuilder.CreateStore(
			elementValue,
			irBuilder.CreatePointerCast(
				irBuilder.CreateInBoundsGEP(
					argBaseAddress,
					{emitLiteral((numArgs - argIndex - 1) * sizeof(UntaggedValue))}),
				elementValue->getType()->getPointerTo()));
	}

	emitThrow(
		emitLiteral((U64) reinterpret_cast<Uptr>(exceptionTypeInstance)),
		sizeof(Uptr) == 8
			? irBuilder.CreatePtrToInt(argBaseAddress, llvmI64Type)
			: zext(irBuilder.CreatePtrToInt(argBaseAddress, llvmI32Type), llvmI64Type),
		true);

	irBuilder.CreateUnreachable();
	enterUnreachable();
}
void EmitFunctionContext::rethrow(RethrowImm imm)
{
	wavmAssert(imm.catchDepth < catchStack.size());
	CatchContext& catchContext = catchStack[catchStack.size() - imm.catchDepth - 1];
	emitThrow(
		loadFromUntypedPointer(catchContext.exceptionPointer, llvmI64Type),
		irBuilder.CreatePtrToInt(
			irBuilder.CreateInBoundsGEP(
				catchContext.exceptionPointer,
				{emitLiteral(I32(offsetof(ExceptionData, arguments)))}),
			llvmI64Type),
		true);

	irBuilder.CreateUnreachable();
	enterUnreachable();
}
