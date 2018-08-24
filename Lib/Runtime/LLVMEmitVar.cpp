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
	wavmAssert(imm.variableIndex < moduleContext.moduleInstance->globals.size());
	GlobalInstance* global    = moduleContext.moduleInstance->globals[imm.variableIndex];
	llvm::Type* llvmValueType = asLLVMType(global->type.valueType);

	if(global->type.isMutable)
	{
		llvm::Value* globalPointer = irBuilder.CreatePointerCast(
			irBuilder.CreateInBoundsGEP(irBuilder.CreateLoad(contextPointerVariable),
										{emitLiteral(Uptr(offsetof(ContextRuntimeData, globalData))
													 + global->mutableDataOffset)}),
			llvmValueType->getPointerTo());
		push(irBuilder.CreateLoad(globalPointer));
	}
	else
	{
		if(getTypeByteWidth(global->type.valueType) > sizeof(void*))
		{
			llvm::Value* globalPointer
				= emitLiteralPointer(&global->initialValue, llvmValueType->getPointerTo());
			push(irBuilder.CreateLoad(globalPointer));
		}
		else
		{
			llvm::Constant* immutableValue;
			switch(global->type.valueType)
			{
			case ValueType::i32: immutableValue = emitLiteral(global->initialValue.i32); break;
			case ValueType::i64: immutableValue = emitLiteral(global->initialValue.i64); break;
			case ValueType::f32: immutableValue = emitLiteral(global->initialValue.f32); break;
			case ValueType::f64: immutableValue = emitLiteral(global->initialValue.f64); break;
			default: Errors::unreachable();
			};
			push(immutableValue);
		}
	}
}
void EmitFunctionContext::set_global(GetOrSetVariableImm<true> imm)
{
	wavmAssert(imm.variableIndex < moduleContext.moduleInstance->globals.size());
	GlobalInstance* global = moduleContext.moduleInstance->globals[imm.variableIndex];
	wavmAssert(global->type.isMutable);
	llvm::Type* llvmValueType  = asLLVMType(global->type.valueType);
	llvm::Value* globalPointer = irBuilder.CreatePointerCast(
		irBuilder.CreateInBoundsGEP(irBuilder.CreateLoad(contextPointerVariable),
									{emitLiteral(Uptr(offsetof(ContextRuntimeData, globalData)
													  + global->mutableDataOffset))}),
		llvmValueType->getPointerTo());
	auto value = irBuilder.CreateBitCast(pop(), llvmValueType);
	irBuilder.CreateStore(value, globalPointer);
}
