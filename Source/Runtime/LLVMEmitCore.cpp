#include "IR/Operators.h"
#include "Inline/Assert.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMEmitModuleContext.h"
#include "LLVMJIT.h"

using namespace LLVMJIT;
using namespace IR;

void EmitFunctionContext::block(ControlStructureImm imm)
{
	FunctionType blockType = resolveBlockType(module, imm.type);

	// Create an end block+phi for the block result.
	auto endBlock = llvm::BasicBlock::Create(*llvmContext, "blockEnd", llvmFunction);
	auto endPHIs  = createPHIs(endBlock, blockType.results());

	// Pop the block arguments.
	llvm::Value** blockArgs
		= (llvm::Value**)alloca(sizeof(llvm::Value*) * blockType.params().size());
	popMultiple(blockArgs, blockType.params().size());

	// Push a control context that ends at the end block/phi.
	pushControlStack(ControlContext::Type::block, blockType.results(), endBlock, endPHIs);

	// Push a branch target for the end block/phi.
	pushBranchTarget(blockType.results(), endBlock, endPHIs);

	// Repush the block arguments.
	pushMultiple(blockArgs, blockType.params().size());
}
void EmitFunctionContext::loop(ControlStructureImm imm)
{
	FunctionType blockType           = resolveBlockType(module, imm.type);
	llvm::BasicBlock* loopEntryBlock = irBuilder.GetInsertBlock();

	// Create a loop block, and an end block for the loop result.
	auto loopBodyBlock = llvm::BasicBlock::Create(*llvmContext, "loopBody", llvmFunction);
	auto endBlock      = llvm::BasicBlock::Create(*llvmContext, "loopEnd", llvmFunction);

	// Create PHIs for the loop's parameters, and PHIs for the loop result.
	auto parameterPHIs = createPHIs(loopBodyBlock, blockType.params());
	auto endPHIs       = createPHIs(endBlock, blockType.results());

	// Branch to the loop body and switch the IR builder to emit there.
	irBuilder.CreateBr(loopBodyBlock);
	irBuilder.SetInsertPoint(loopBodyBlock);

	// Pop the initial values of the loop's parameters from the stack.
	for(Iptr elementIndex = Iptr(blockType.params().size()) - 1; elementIndex >= 0; --elementIndex)
	{ parameterPHIs[elementIndex]->addIncoming(pop(), loopEntryBlock); }

	// Push a control context that ends at the end block/phi.
	pushControlStack(ControlContext::Type::loop, blockType.results(), endBlock, endPHIs);

	// Push a branch target for the loop body start.
	pushBranchTarget(blockType.params(), loopBodyBlock, parameterPHIs);

	// Push the loop argument PHIs on the stack.
	pushMultiple((llvm::Value**)parameterPHIs.data(), parameterPHIs.size());
}
void EmitFunctionContext::if_(ControlStructureImm imm)
{
	FunctionType blockType = resolveBlockType(module, imm.type);

	// Create a then block and else block for the if, and an end block+phi for the if result.
	auto thenBlock = llvm::BasicBlock::Create(*llvmContext, "ifThen", llvmFunction);
	auto elseBlock = llvm::BasicBlock::Create(*llvmContext, "ifElse", llvmFunction);
	auto endBlock  = llvm::BasicBlock::Create(*llvmContext, "ifElseEnd", llvmFunction);
	auto endPHIs   = createPHIs(endBlock, blockType.results());

	// Pop the if condition from the operand stack.
	auto condition = pop();
	irBuilder.CreateCondBr(coerceI32ToBool(condition), thenBlock, elseBlock);

	// Pop the arguments from the operand stack.
	ValueVector args;
	wavmAssert(stack.size() >= blockType.params().size());
	args.resize(blockType.params().size());
	popMultiple(args.data(), args.size());

	// Switch the IR builder to emit the then block.
	irBuilder.SetInsertPoint(thenBlock);

	// Push an ifThen control context that ultimately ends at the end block/phi, but may be
	// terminated by an else operator that changes the control context to the else block.
	pushControlStack(
		ControlContext::Type::ifThen, blockType.results(), endBlock, endPHIs, elseBlock, args);

	// Push a branch target for the if end.
	pushBranchTarget(blockType.results(), endBlock, endPHIs);

	// Repush the if arguments on the stack.
	pushMultiple(args.data(), args.size());
}
void EmitFunctionContext::else_(NoImm imm)
{
	wavmAssert(controlStack.size());
	ControlContext& currentContext = controlStack.back();

	branchToEndOfControlContext();

	// Switch the IR emitter to the else block.
	wavmAssert(currentContext.elseBlock);
	wavmAssert(currentContext.type == ControlContext::Type::ifThen);
	currentContext.elseBlock->moveAfter(irBuilder.GetInsertBlock());
	irBuilder.SetInsertPoint(currentContext.elseBlock);

	// Push the if arguments back on the operand stack.
	for(llvm::Value* argument : currentContext.elseArgs) { push(argument); }

	// Change the top of the control stack to an else clause.
	currentContext.type        = ControlContext::Type::ifElse;
	currentContext.isReachable = true;
	currentContext.elseBlock   = nullptr;
}
void EmitFunctionContext::end(NoImm)
{
	wavmAssert(controlStack.size());
	ControlContext& currentContext = controlStack.back();

	branchToEndOfControlContext();

	if(currentContext.elseBlock)
	{
		// If this is the end of an if without an else clause, create a dummy else clause.
		currentContext.elseBlock->moveAfter(irBuilder.GetInsertBlock());
		irBuilder.SetInsertPoint(currentContext.elseBlock);
		irBuilder.CreateBr(currentContext.endBlock);

		// Add the if arguments to the end PHIs as if they just passed through the absent else
		// block.
		wavmAssert(currentContext.elseArgs.size() == currentContext.endPHIs.size());
		for(Uptr argIndex = 0; argIndex < currentContext.elseArgs.size(); ++argIndex)
		{
			currentContext.endPHIs[argIndex]->addIncoming(currentContext.elseArgs[argIndex],
														  currentContext.elseBlock);
		}
	}

	if(currentContext.type == ControlContext::Type::try_) { endTry(); }
	else if(currentContext.type == ControlContext::Type::catch_)
	{
		endCatch();
	}

	// Switch the IR emitter to the end block.
	currentContext.endBlock->moveAfter(irBuilder.GetInsertBlock());
	irBuilder.SetInsertPoint(currentContext.endBlock);

	if(currentContext.endPHIs.size())
	{
		// If the control context yields results, take the PHIs that merges all the control flow to
		// the end and push their values onto the operand stack.
		wavmAssert(currentContext.endPHIs.size() == currentContext.resultTypes.size());
		for(Uptr elementIndex = 0; elementIndex < currentContext.endPHIs.size(); ++elementIndex)
		{
			if(currentContext.endPHIs[elementIndex]->getNumIncomingValues())
			{ push(currentContext.endPHIs[elementIndex]); }
			else
			{
				// If there weren't any incoming values for the end PHI, remove it and push
				// a dummy value.
				currentContext.endPHIs[elementIndex]->eraseFromParent();
				push(typedZeroConstants[(Uptr)currentContext.resultTypes[elementIndex]]);
			}
		}
	}

	// Pop and branch targets introduced by this control context.
	wavmAssert(currentContext.outerBranchTargetStackSize <= branchTargetStack.size());
	branchTargetStack.resize(currentContext.outerBranchTargetStackSize);

	// Pop this control context.
	controlStack.pop_back();
}

void EmitFunctionContext::br_if(BranchImm imm)
{
	// Pop the condition from operand stack.
	auto condition = pop();

	BranchTarget& target = getBranchTargetByDepth(imm.targetDepth);
	wavmAssert(target.params.size() == target.phis.size());
	for(Uptr argIndex = 0; argIndex < target.params.size(); ++argIndex)
	{
		// Take the branch target operands from the stack (without popping them) and add them to the
		// target's incoming value PHIs.
		llvm::Value* argument = getValueFromTop(target.params.size() - argIndex - 1);
		target.phis[argIndex]->addIncoming(coerceToCanonicalType(argument),
										   irBuilder.GetInsertBlock());
	}

	// Create a new basic block for the case where the branch is not taken.
	auto falseBlock = llvm::BasicBlock::Create(*llvmContext, "br_ifElse", llvmFunction);

	// Emit a conditional branch to either the falseBlock or the target block.
	irBuilder.CreateCondBr(coerceI32ToBool(condition), target.block, falseBlock);

	// Resume emitting instructions in the falseBlock.
	irBuilder.SetInsertPoint(falseBlock);
}

void EmitFunctionContext::br(BranchImm imm)
{
	BranchTarget& target = getBranchTargetByDepth(imm.targetDepth);
	wavmAssert(target.params.size() == target.phis.size());

	// Pop the branch target operands from the stack and add them to the target's incoming value
	// PHIs.
	for(Iptr argIndex = Iptr(target.params.size()) - 1; argIndex >= 0; --argIndex)
	{
		llvm::Value* argument = pop();
		target.phis[argIndex]->addIncoming(coerceToCanonicalType(argument),
										   irBuilder.GetInsertBlock());
	}

	// Branch to the target block.
	irBuilder.CreateBr(target.block);

	enterUnreachable();
}
void EmitFunctionContext::br_table(BranchTableImm imm)
{
	// Pop the table index from the operand stack.
	auto index = pop();

	// Look up the default branch target, and assume its argument type applies to all targets. (this
	// is guaranteed by the validator)
	BranchTarget& defaultTarget = getBranchTargetByDepth(imm.defaultTargetDepth);

	// Pop the branch arguments from the stack.
	const Uptr numArgs = defaultTarget.params.size();
	llvm::Value** args = (llvm::Value**)alloca(sizeof(llvm::Value*) * numArgs);
	popMultiple(args, numArgs);

	// Add the branch arguments to the default target's parameter PHI nodes.
	for(Uptr argIndex = 0; argIndex < numArgs; ++argIndex)
	{
		defaultTarget.phis[argIndex]->addIncoming(coerceToCanonicalType(args[argIndex]),
												  irBuilder.GetInsertBlock());
	}

	// Create a LLVM switch instruction.
	wavmAssert(imm.branchTableIndex < functionDef.branchTables.size());
	const std::vector<U32>& targetDepths = functionDef.branchTables[imm.branchTableIndex];
	auto llvmSwitch
		= irBuilder.CreateSwitch(index, defaultTarget.block, (unsigned int)targetDepths.size());

	for(Uptr targetIndex = 0; targetIndex < targetDepths.size(); ++targetIndex)
	{
		BranchTarget& target = getBranchTargetByDepth(targetDepths[targetIndex]);

		// Add this target to the switch instruction.
		llvmSwitch->addCase(emitLiteral((U32)targetIndex), target.block);

		// Add the branch arguments to the PHI nodes for the branch target's parameters.
		wavmAssert(target.phis.size() == numArgs);
		for(Uptr argIndex = 0; argIndex < numArgs; ++argIndex)
		{
			target.phis[argIndex]->addIncoming(coerceToCanonicalType(args[argIndex]),
											   irBuilder.GetInsertBlock());
		}
	}

	enterUnreachable();
}
void EmitFunctionContext::return_(NoImm)
{
	// Pop the branch target operands from the stack and add them to the target's incoming value
	// PHIs.
	for(Iptr argIndex = functionType.results().size() - 1; argIndex >= 0; --argIndex)
	{
		llvm::Value* argument = pop();
		controlStack[0].endPHIs[argIndex]->addIncoming(coerceToCanonicalType(argument),
													   irBuilder.GetInsertBlock());
	}

	// Branch to the return block.
	irBuilder.CreateBr(controlStack[0].endBlock);

	enterUnreachable();
}

void EmitFunctionContext::unreachable(NoImm)
{
	// Call an intrinsic that causes a trap, and insert the LLVM unreachable terminator.
	emitRuntimeIntrinsic("unreachableTrap", FunctionType(), {});
	irBuilder.CreateUnreachable();

	enterUnreachable();
}

//
// Call operators
//

void EmitFunctionContext::call(CallImm imm)
{
	// Map the callee function index to either an imported function pointer or a function in this
	// module.
	llvm::Value* callee;
	FunctionType calleeType;
	CallingConvention callingConvention;
	if(imm.functionIndex < module.functions.imports.size())
	{
		wavmAssert(imm.functionIndex < moduleContext.moduleInstance->functions.size());
		FunctionInstance* importedCallee
			= moduleContext.moduleInstance->functions[imm.functionIndex];
		calleeType = importedCallee->type;
		callee     = emitLiteralPointer(
            importedCallee->nativeFunction,
            asLLVMType(importedCallee->type, importedCallee->callingConvention)->getPointerTo());
		callingConvention = importedCallee->callingConvention;
	}
	else
	{
		const Uptr functionDefIndex = imm.functionIndex - module.functions.imports.size();
		wavmAssert(functionDefIndex < moduleContext.functionDefs.size());
		callee            = moduleContext.functionDefs[functionDefIndex];
		calleeType        = module.types[module.functions.defs[functionDefIndex].type.index];
		callingConvention = CallingConvention::wasm;
	}

	// Pop the call arguments from the operand stack.
	const Uptr numArguments = calleeType.params().size();
	auto llvmArgs           = (llvm::Value**)alloca(sizeof(llvm::Value*) * numArguments);
	popMultiple(llvmArgs, numArguments);

	// Call the function.
	ValueVector results = emitCallOrInvoke(callee,
										   llvm::ArrayRef<llvm::Value*>(llvmArgs, numArguments),
										   calleeType,
										   callingConvention,
										   getInnermostUnwindToBlock());

	// Push the results on the operand stack.
	for(llvm::Value* result : results) { push(result); }
}
void EmitFunctionContext::call_indirect(CallIndirectImm imm)
{
	wavmAssert(imm.type.index < module.types.size());

	const FunctionType calleeType = module.types[imm.type.index];

	// Compile the function index.
	auto tableElementIndex = pop();

	// Pop the call arguments from the operand stack.
	const Uptr numArguments = calleeType.params().size();
	auto llvmArgs           = (llvm::Value**)alloca(sizeof(llvm::Value*) * numArguments);
	popMultiple(llvmArgs, numArguments);

	// Zero extend the function index to the pointer size.
	auto functionIndexZExt = zext(tableElementIndex, sizeof(Uptr) == 4 ? llvmI32Type : llvmI64Type);

	auto tableElementType = llvm::StructType::get(*llvmContext, {llvmI8PtrType, llvmI8PtrType});
	auto typedTableBasePointer = irBuilder.CreatePointerCast(
		irBuilder.CreateLoad(tableBasePointerVariable), tableElementType->getPointerTo());

	// Load the type for this table entry.
	auto functionTypePointerPointer = irBuilder.CreateInBoundsGEP(
		typedTableBasePointer, {functionIndexZExt, emitLiteral((U32)0)});
	auto functionTypePointer = irBuilder.CreateLoad(functionTypePointerPointer);
	auto llvmCalleeType
		= emitLiteralPointer(reinterpret_cast<void*>(calleeType.getEncoding().impl), llvmI8PtrType);

	// If the function type doesn't match, trap.
	emitConditionalTrapIntrinsic(
		irBuilder.CreateICmpNE(llvmCalleeType, functionTypePointer),
		"indirectCallSignatureMismatch",
		FunctionType(TypeTuple(), TypeTuple({ValueType::i32, ValueType::i64})),
		{tableElementIndex, irBuilder.CreatePtrToInt(llvmCalleeType, llvmI64Type)});

	// Call the function loaded from the table.
	auto functionPointerPointer = irBuilder.CreateInBoundsGEP(
		typedTableBasePointer, {functionIndexZExt, emitLiteral((U32)1)});
	auto functionPointer = loadFromUntypedPointer(
		functionPointerPointer, asLLVMType(calleeType, CallingConvention::wasm)->getPointerTo());
	ValueVector results = emitCallOrInvoke(functionPointer,
										   llvm::ArrayRef<llvm::Value*>(llvmArgs, numArguments),
										   calleeType,
										   CallingConvention::wasm,
										   getInnermostUnwindToBlock());

	// Push the results on the operand stack.
	for(llvm::Value* result : results) { push(result); }
}

void EmitFunctionContext::nop(IR::NoImm) {}
void EmitFunctionContext::drop(IR::NoImm) { stack.pop_back(); }
void EmitFunctionContext::select(IR::NoImm)
{
	auto condition  = pop();
	auto falseValue = pop();
	auto trueValue  = pop();
	push(irBuilder.CreateSelect(coerceI32ToBool(condition), trueValue, falseValue));
}
