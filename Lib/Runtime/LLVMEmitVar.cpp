#include "IR/Operators.h"
#include "Inline/Assert.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMEmitModuleContext.h"
#include "LLVMJIT.h"

using namespace LLVMJIT;
using namespace IR;

//
// Local variables
//

void EmitFunctionContext::get_local(GetOrSetVariableImm<false> imm)
{
	wavmAssert(imm.variableIndex < localPointers.size());
	push(irBuilder.CreateLoad(localPointers[imm.variableIndex]));
}
void EmitFunctionContext::set_local(GetOrSetVariableImm<false> imm)
{
	wavmAssert(imm.variableIndex < localPointers.size());
	auto value = irBuilder.CreateBitCast(
		pop(), localPointers[imm.variableIndex]->getType()->getPointerElementType());
	irBuilder.CreateStore(value, localPointers[imm.variableIndex]);
}
void EmitFunctionContext::tee_local(GetOrSetVariableImm<false> imm)
{
	wavmAssert(imm.variableIndex < localPointers.size());
	auto value = irBuilder.CreateBitCast(
		getValueFromTop(), localPointers[imm.variableIndex]->getType()->getPointerElementType());
	irBuilder.CreateStore(value, localPointers[imm.variableIndex]);
}

//
// Global variables
//

void EmitFunctionContext::get_global(GetOrSetVariableImm<true> imm)
{
	wavmAssert(imm.variableIndex < irModule.globals.size());
	GlobalType globalType     = irModule.globals.getType(imm.variableIndex);
	llvm::Type* llvmValueType = asLLVMType(globalType.valueType);

	llvm::Value* value;
	if(globalType.isMutable)
	{
		// If the global is mutable, the symbol will be bound to an offset into the
		// ContextRuntimeData::globalData that its value is stored at.
		llvm::Value* globalDataOffset
			= irBuilder.CreatePtrToInt(moduleContext.globals[imm.variableIndex], llvmIptrType);
		llvm::Value* globalPointer = irBuilder.CreateInBoundsGEP(
			irBuilder.CreateLoad(contextPointerVariable), {globalDataOffset});
		value = loadFromUntypedPointer(globalPointer, llvmValueType);
	}
	else
	{
		// Otherwise, the symbol's value will point to the global's immutable value.
		value = loadFromUntypedPointer(moduleContext.globals[imm.variableIndex], llvmValueType);
	}

	push(value);
}
void EmitFunctionContext::set_global(GetOrSetVariableImm<true> imm)
{
	wavmAssert(imm.variableIndex < irModule.globals.size());
	GlobalType globalType = irModule.globals.getType(imm.variableIndex);
	wavmAssert(globalType.isMutable);
	llvm::Type* llvmValueType = asLLVMType(globalType.valueType);

	llvm::Value* value = irBuilder.CreateBitCast(pop(), llvmValueType);

	// If the global is mutable, the symbol will be bound to an offset into the
	// ContextRuntimeData::globalData that its value is stored at.
	llvm::Value* globalDataOffset
		= irBuilder.CreatePtrToInt(moduleContext.globals[imm.variableIndex], llvmIptrType);
	llvm::Value* globalPointer = irBuilder.CreateInBoundsGEP(
		irBuilder.CreateLoad(contextPointerVariable), {globalDataOffset});
	storeToUntypedPointer(value, globalPointer);
}
