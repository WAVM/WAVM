#include "IR/OperatorPrinter.h"
#include "IR/Operators.h"
#include "Inline/Assert.h"
#include "Inline/Timing.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMEmitModuleContext.h"
#include "LLVMJIT.h"
#include "Logging/Logging.h"

#include "LLVMPreInclude.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

#include "LLVMPostInclude.h"

#define ENABLE_LOGGING 0
#define ENABLE_FUNCTION_ENTER_EXIT_HOOKS 0

using namespace LLVMJIT;
using namespace IR;

// Creates a PHI node for the argument of branches to a basic block.
PHIVector EmitFunctionContext::createPHIs(llvm::BasicBlock* basicBlock, IR::TypeTuple type)
{
	auto originalBlock = irBuilder.GetInsertBlock();
	irBuilder.SetInsertPoint(basicBlock);

	PHIVector result;
	for(Uptr elementIndex = 0; elementIndex < type.size(); ++elementIndex)
	{ result.push_back(irBuilder.CreatePHI(asLLVMType(type[elementIndex]), 2)); }

	if(originalBlock) { irBuilder.SetInsertPoint(originalBlock); }

	return result;
}

// Bitcasts a LLVM value to a canonical type for the corresponding WebAssembly type.
// This is currently just used to map all the various vector types to a canonical type for the
// vector width.
llvm::Value* EmitFunctionContext::coerceToCanonicalType(llvm::Value* value)
{
	return value->getType()->isVectorTy() || value->getType()->isX86_MMXTy()
		? irBuilder.CreateBitCast(value, llvmI64x2Type)
		: value;
}

// Debug logging.
void EmitFunctionContext::logOperator(const std::string& operatorDescription)
{
	if(ENABLE_LOGGING)
	{
		std::string controlStackString;
		for(Uptr stackIndex = 0; stackIndex < controlStack.size(); ++stackIndex)
		{
			if(!controlStack[stackIndex].isReachable) { controlStackString += "("; }
			switch(controlStack[stackIndex].type)
			{
			case ControlContext::Type::function: controlStackString += "F"; break;
			case ControlContext::Type::block: controlStackString += "B"; break;
			case ControlContext::Type::ifThen: controlStackString += "I"; break;
			case ControlContext::Type::ifElse: controlStackString += "E"; break;
			case ControlContext::Type::loop: controlStackString += "L"; break;
			case ControlContext::Type::try_: controlStackString += "T"; break;
			case ControlContext::Type::catch_: controlStackString += "C"; break;
			default: Errors::unreachable();
			};
			if(!controlStack[stackIndex].isReachable) { controlStackString += ")"; }
		}

		std::string stackString;
		const Uptr stackBase = controlStack.size() == 0 ? 0 : controlStack.back().outerStackSize;
		for(Uptr stackIndex = 0; stackIndex < stack.size(); ++stackIndex)
		{
			if(stackIndex == stackBase) { stackString += "| "; }
			{
				llvm::raw_string_ostream stackTypeStream(stackString);
				stack[stackIndex]->getType()->print(stackTypeStream, true);
			}
			stackString += " ";
		}
		if(stack.size() == stackBase) { stackString += "|"; }

		Log::printf(
			Log::debug,
			"%-50s %-50s %-50s\n",
			controlStackString.c_str(),
			operatorDescription.c_str(),
			stackString.c_str());
	}
}

// Bounds checks and converts a memory operation I32 address operand to a LLVM pointer.
llvm::Value* EmitFunctionContext::coerceByteIndexToPointer(
	llvm::Value* byteIndex,
	U32 offset,
	llvm::Type* memoryType)
{
	// zext the 32-bit address to 64-bits.
	// This is crucial for security, as LLVM will otherwise implicitly sign extend it to 64-bits in
	// the GEP below, interpreting it as a signed offset and allowing access to memory outside the
	// sandboxed memory range. There are no 'far addresses' in a 32 bit runtime.
	byteIndex = zext(byteIndex, llvmI64Type);

	// Add the offset to the byte index.
	if(offset)
	{ byteIndex = irBuilder.CreateAdd(byteIndex, zext(emitLiteral(offset), llvmI64Type)); }

	// If HAS_64BIT_ADDRESS_SPACE, the memory has enough virtual address space allocated to ensure
	// that any 32-bit byte index + 32-bit offset will fall within the virtual address sandbox, so
	// no explicit bounds check is necessary.

	// Cast the pointer to the appropriate type.
	auto bytePointer
		= irBuilder.CreateInBoundsGEP(irBuilder.CreateLoad(memoryBasePointerVariable), byteIndex);
	return irBuilder.CreatePointerCast(bytePointer, memoryType->getPointerTo());
}

// Traps a divide-by-zero
void EmitFunctionContext::trapDivideByZero(ValueType type, llvm::Value* divisor)
{
	emitConditionalTrapIntrinsic(
		irBuilder.CreateICmpEQ(divisor, typedZeroConstants[(Uptr)type]),
		"divideByZeroOrIntegerOverflowTrap",
		FunctionType(),
		{});
}

// Traps on (x / 0) or (INT_MIN / -1).
void EmitFunctionContext::trapDivideByZeroOrIntegerOverflow(
	ValueType type,
	llvm::Value* left,
	llvm::Value* right)
{
	emitConditionalTrapIntrinsic(
		irBuilder.CreateOr(
			irBuilder.CreateAnd(
				irBuilder.CreateICmpEQ(
					left,
					type == ValueType::i32 ? emitLiteral((U32)INT32_MIN)
										   : emitLiteral((U64)INT64_MIN)),
				irBuilder.CreateICmpEQ(
					right, type == ValueType::i32 ? emitLiteral((U32)-1) : emitLiteral((U64)-1))),
			irBuilder.CreateICmpEQ(right, typedZeroConstants[(Uptr)type])),
		"divideByZeroOrIntegerOverflowTrap",
		FunctionType(),
		{});
}

// Emits a call to a WAVM intrinsic function.
ValueVector EmitFunctionContext::emitRuntimeIntrinsic(
	const char* intrinsicName,
	FunctionType intrinsicType,
	const std::initializer_list<llvm::Value*>& args)
{
	Object* intrinsicObject = Runtime::getInstanceExport(
		moduleContext.moduleInstance->compartment->wavmIntrinsics, intrinsicName);
	wavmAssert(intrinsicObject);
	wavmAssert(isA(intrinsicObject, intrinsicType));
	FunctionInstance* intrinsicFunction = asFunction(intrinsicObject);
	wavmAssert(intrinsicFunction->type == intrinsicType);
	auto intrinsicFunctionPointer = emitLiteralPointer(
		intrinsicFunction->nativeFunction,
		asLLVMType(intrinsicType, intrinsicFunction->callingConvention)->getPointerTo());

	return emitCallOrInvoke(
		intrinsicFunctionPointer,
		args,
		intrinsicType,
		intrinsicFunction->callingConvention,
		getInnermostUnwindToBlock());
}

// A helper function to emit a conditional call to a non-returning intrinsic function.
void EmitFunctionContext::emitConditionalTrapIntrinsic(
	llvm::Value* booleanCondition,
	const char* intrinsicName,
	FunctionType intrinsicType,
	const std::initializer_list<llvm::Value*>& args)
{
	auto trueBlock
		= llvm::BasicBlock::Create(*llvmContext, llvm::Twine(intrinsicName) + "Trap", llvmFunction);
	auto endBlock
		= llvm::BasicBlock::Create(*llvmContext, llvm::Twine(intrinsicName) + "Skip", llvmFunction);

	irBuilder.CreateCondBr(
		booleanCondition, trueBlock, endBlock, moduleContext.likelyFalseBranchWeights);

	irBuilder.SetInsertPoint(trueBlock);
	emitRuntimeIntrinsic(intrinsicName, intrinsicType, args);
	irBuilder.CreateUnreachable();

	irBuilder.SetInsertPoint(endBlock);
}

//
// Control structure operators
//

void EmitFunctionContext::pushControlStack(
	ControlContext::Type type,
	TypeTuple resultTypes,
	llvm::BasicBlock* endBlock,
	const PHIVector& endPHIs,
	llvm::BasicBlock* elseBlock,
	const ValueVector& elseArgs)
{
	// The unreachable operator filtering should filter out any opcodes that call pushControlStack.
	if(controlStack.size()) { errorUnless(controlStack.back().isReachable); }

	controlStack.push_back({type,
							endBlock,
							endPHIs,
							elseBlock,
							elseArgs,
							resultTypes,
							stack.size(),
							branchTargetStack.size(),
							true});
}

void EmitFunctionContext::pushBranchTarget(
	TypeTuple branchArgumentType,
	llvm::BasicBlock* branchTargetBlock,
	const PHIVector& branchTargetPHIs)
{
	branchTargetStack.push_back({branchArgumentType, branchTargetBlock, branchTargetPHIs});
}

void EmitFunctionContext::branchToEndOfControlContext()
{
	ControlContext& currentContext = controlStack.back();

	if(currentContext.isReachable)
	{
		// If the control context expects a result, take it from the operand stack and add it to the
		// control context's end PHI.
		for(Iptr resultIndex = Iptr(currentContext.resultTypes.size()) - 1; resultIndex >= 0;
			--resultIndex)
		{
			llvm::Value* result = pop();
			currentContext.endPHIs[resultIndex]->addIncoming(
				coerceToCanonicalType(result), irBuilder.GetInsertBlock());
		}

		// Branch to the control context's end.
		irBuilder.CreateBr(currentContext.endBlock);
	}
	wavmAssert(stack.size() == currentContext.outerStackSize);
}

void EmitFunctionContext::enterUnreachable()
{
	// Unwind the operand stack to the outer control context.
	wavmAssert(controlStack.back().outerStackSize <= stack.size());
	stack.resize(controlStack.back().outerStackSize);

	// Mark the current control context as unreachable: this will cause the outer loop to stop
	// dispatching operators to us until an else/end for the current control context is reached.
	controlStack.back().isReachable = false;
}

// A do-nothing visitor used to decode past unreachable operators (but supporting logging, and
// passing the end operator through).
struct UnreachableOpVisitor
{
	typedef void Result;

	UnreachableOpVisitor(EmitFunctionContext& inContext)
	: context(inContext)
	, unreachableControlDepth(0)
	{
	}
#define VISIT_OP(opcode, name, nameString, Imm, ...) \
	void name(Imm imm) {}
	ENUM_NONCONTROL_OPERATORS(VISIT_OP)
	VISIT_OP(_, unknown, "unknown", Opcode)
#undef VISIT_OP

	// Keep track of control structure nesting level in unreachable code, so we know when we reach
	// the end of the unreachable code.
	void block(ControlStructureImm) { ++unreachableControlDepth; }
	void loop(ControlStructureImm) { ++unreachableControlDepth; }
	void if_(ControlStructureImm) { ++unreachableControlDepth; }

	// If an else or end opcode would signal an end to the unreachable code, then pass it through to
	// the IR emitter.
	void else_(NoImm imm)
	{
		if(!unreachableControlDepth) { context.else_(imm); }
	}
	void end(NoImm imm)
	{
		if(!unreachableControlDepth) { context.end(imm); }
		else
		{
			--unreachableControlDepth;
		}
	}

	void try_(ControlStructureImm imm) { ++unreachableControlDepth; }
	void catch_(CatchImm imm)
	{
		if(!unreachableControlDepth) { context.catch_(imm); }
	}
	void catch_all(NoImm imm)
	{
		if(!unreachableControlDepth) { context.catch_all(imm); }
	}

private:
	EmitFunctionContext& context;
	Uptr unreachableControlDepth;
};

void EmitFunctionContext::emit()
{
	// Create debug info for the function.
	llvm::SmallVector<llvm::Metadata*, 10> diFunctionParameterTypes;
	for(auto parameterType : functionType.params())
	{ diFunctionParameterTypes.push_back(moduleContext.diValueTypes[(Uptr)parameterType]); }
	auto diParamArray   = moduleContext.diBuilder->getOrCreateTypeArray(diFunctionParameterTypes);
	auto diFunctionType = moduleContext.diBuilder->createSubroutineType(diParamArray);
	diFunction          = moduleContext.diBuilder->createFunction(
        moduleContext.diModuleScope,
        functionInstance->debugName,
        llvmFunction->getName(),
        moduleContext.diModuleScope,
        0,
        diFunctionType,
        false,
        true,
        0);
	llvmFunction->setSubprogram(diFunction);

	// Create the return basic block, and push the root control context for the function.
	auto returnBlock = llvm::BasicBlock::Create(*llvmContext, "return", llvmFunction);
	auto returnPHIs  = createPHIs(returnBlock, functionType.results());
	pushControlStack(
		ControlContext::Type::function, functionType.results(), returnBlock, returnPHIs);
	pushBranchTarget(functionType.results(), returnBlock, returnPHIs);

	// Create an initial basic block for the function.
	auto entryBasicBlock = llvm::BasicBlock::Create(*llvmContext, "entry", llvmFunction);
	irBuilder.SetInsertPoint(entryBasicBlock);

	// Create and initialize allocas for the memory and table base parameters.
	auto llvmArgIt            = llvmFunction->arg_begin();
	memoryBasePointerVariable = irBuilder.CreateAlloca(llvmI8PtrType, nullptr, "memoryBase");
	tableBasePointerVariable  = irBuilder.CreateAlloca(llvmI8PtrType, nullptr, "tableBase");
	contextPointerVariable    = irBuilder.CreateAlloca(llvmI8PtrType, nullptr, "context");
	irBuilder.CreateStore(&*llvmArgIt++, contextPointerVariable);
	reloadMemoryAndTableBase();

	// Create and initialize allocas for all the locals and parameters.
	for(Uptr localIndex = 0;
		localIndex < functionType.params().size() + functionDef.nonParameterLocalTypes.size();
		++localIndex)
	{
		auto localType = localIndex < functionType.params().size()
			? functionType.params()[localIndex]
			: functionDef.nonParameterLocalTypes[localIndex - functionType.params().size()];
		auto localPointer = irBuilder.CreateAlloca(asLLVMType(localType), nullptr, "");
		localPointers.push_back(localPointer);

		if(localIndex < functionType.params().size())
		{
			// Copy the parameter value into the local that stores it.
			irBuilder.CreateStore(&*llvmArgIt, localPointer);
			++llvmArgIt;
		}
		else
		{
			// Initialize non-parameter locals to zero.
			irBuilder.CreateStore(typedZeroConstants[(Uptr)localType], localPointer);
		}
	}

	// If enabled, emit a call to the WAVM function enter hook (for debugging).
	if(ENABLE_FUNCTION_ENTER_EXIT_HOOKS)
	{
		emitRuntimeIntrinsic(
			"debugEnterFunction",
			FunctionType(TypeTuple{}, TypeTuple{ValueType::i64}),
			{emitLiteral(reinterpret_cast<U64>(functionInstance))});
	}

	// Decode the WebAssembly opcodes and emit LLVM IR for them.
	OperatorDecoderStream decoder(functionDef.code);
	UnreachableOpVisitor unreachableOpVisitor(*this);
	OperatorPrinter operatorPrinter(module, functionDef);
	Uptr opIndex = 0;
	while(decoder && controlStack.size())
	{
		irBuilder.SetCurrentDebugLocation(
			llvm::DILocation::get(*llvmContext, (unsigned int)opIndex++, 0, diFunction));
		if(ENABLE_LOGGING) { logOperator(decoder.decodeOpWithoutConsume(operatorPrinter)); }

		if(controlStack.back().isReachable) { decoder.decodeOp(*this); }
		else
		{
			decoder.decodeOp(unreachableOpVisitor);
		}
	};
	wavmAssert(irBuilder.GetInsertBlock() == returnBlock);

	// If enabled, emit a call to the WAVM function enter hook (for debugging).
	if(ENABLE_FUNCTION_ENTER_EXIT_HOOKS)
	{
		emitRuntimeIntrinsic(
			"debugExitFunction",
			FunctionType(TypeTuple{}, TypeTuple{ValueType::i64}),
			{emitLiteral(reinterpret_cast<U64>(functionInstance))});
	}

	// Emit the function return.
	emitReturn(functionType.results(), stack);

	// If a local escape block was created, add a localescape intrinsic to it with the accumulated
	// local escape allocas, and insert it before the function's entry block.
	if(localEscapeBlock)
	{
		irBuilder.SetInsertPoint(localEscapeBlock);
		callLLVMIntrinsic({}, llvm::Intrinsic::localescape, pendingLocalEscapes);
		irBuilder.CreateBr(&llvmFunction->getEntryBlock());
		localEscapeBlock->moveBefore(&llvmFunction->getEntryBlock());
	}
}