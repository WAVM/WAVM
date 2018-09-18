#include <vector>

#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Types.h"
#include "Inline/Assert.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMEmitModuleContext.h"
#include "LLVMJITPrivate.h"

#include "LLVMPreInclude.h"

#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

#include "LLVMPostInclude.h"

using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

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
	GlobalType globalType = irModule.globals.getType(imm.variableIndex);
	llvm::Type* llvmValueType = asLLVMType(llvmContext, globalType.valueType);

	llvm::Value* value = nullptr;
	if(globalType.isMutable)
	{
		// If the global is mutable, the symbol will be bound to an offset into the
		// ContextRuntimeData::globalData that its value is stored at.
		llvm::Value* globalDataOffset = irBuilder.CreatePtrToInt(
			moduleContext.globals[imm.variableIndex], llvmContext.iptrType);
		llvm::Value* globalPointer = irBuilder.CreateInBoundsGEP(
			irBuilder.CreateLoad(contextPointerVariable), {globalDataOffset});
		value = loadFromUntypedPointer(globalPointer, llvmValueType);
	}
	else
	{
		// If the value is an immutable global definition with a literal value, emit the literal.
		if(irModule.globals.isDef(imm.variableIndex))
		{
			const IR::GlobalDef& globalDef = irModule.globals.getDef(imm.variableIndex);

			switch(globalDef.initializer.type)
			{
			case InitializerExpression::Type::i32_const:
				value = emitLiteral(llvmContext, globalDef.initializer.i32);
				break;
			case InitializerExpression::Type::i64_const:
				value = emitLiteral(llvmContext, globalDef.initializer.i64);
				break;
			case InitializerExpression::Type::f32_const:
				value = emitLiteral(llvmContext, globalDef.initializer.f32);
				break;
			case InitializerExpression::Type::f64_const:
				value = emitLiteral(llvmContext, globalDef.initializer.f64);
				break;
			case InitializerExpression::Type::v128_const:
				value = emitLiteral(llvmContext, globalDef.initializer.v128);
				break;
			case InitializerExpression::Type::ref_null:
				value = emitLiteralPointer(nullptr, llvmContext.anyrefType);
				break;
			default: break;
			};
		}

		if(!value)
		{
			// Otherwise, the symbol's value will point to the global's immutable value.
			value = loadFromUntypedPointer(moduleContext.globals[imm.variableIndex], llvmValueType);
		}
	}

	push(value);
}
void EmitFunctionContext::set_global(GetOrSetVariableImm<true> imm)
{
	wavmAssert(imm.variableIndex < irModule.globals.size());
	GlobalType globalType = irModule.globals.getType(imm.variableIndex);
	wavmAssert(globalType.isMutable);
	llvm::Type* llvmValueType = asLLVMType(llvmContext, globalType.valueType);

	llvm::Value* value = irBuilder.CreateBitCast(pop(), llvmValueType);

	// If the global is mutable, the symbol will be bound to an offset into the
	// ContextRuntimeData::globalData that its value is stored at.
	llvm::Value* globalDataOffset
		= irBuilder.CreatePtrToInt(moduleContext.globals[imm.variableIndex], llvmContext.iptrType);
	llvm::Value* globalPointer = irBuilder.CreateInBoundsGEP(
		irBuilder.CreateLoad(contextPointerVariable), {globalDataOffset});
	storeToUntypedPointer(value, globalPointer);
}
