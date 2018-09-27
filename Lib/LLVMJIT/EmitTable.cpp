#include "EmitContext.h"
#include "EmitFunctionContext.h"
#include "EmitModuleContext.h"
#include "LLVMJITPrivate.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Runtime/RuntimeData.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

void EmitFunctionContext::ref_null(NoImm)
{
	push(llvm::Constant::getNullValue(llvmContext.anyrefType));
}

void EmitFunctionContext::ref_isnull(NoImm)
{
	llvm::Value* reference = pop();
	llvm::Value* null = llvm::Constant::getNullValue(llvmContext.anyrefType);
	llvm::Value* isNull = irBuilder.CreateICmpEQ(reference, null);
	push(coerceBoolToI32(isNull));
}

void EmitFunctionContext::ref_func(FunctionImm imm)
{
	llvm::Value* referencedFunction = moduleContext.functions[imm.functionIndex];
	llvm::Value* codeAddress = irBuilder.CreatePtrToInt(referencedFunction, llvmContext.iptrType);
	llvm::Value* functionAddress = irBuilder.CreateSub(
		codeAddress, emitLiteral(llvmContext, Uptr(offsetof(Runtime::Function, code))));
	llvm::Value* anyref = irBuilder.CreateIntToPtr(functionAddress, llvmContext.anyrefType);
	push(anyref);
}

void EmitFunctionContext::table_get(TableImm imm)
{
	llvm::Value* index = pop();
	llvm::Value* result = emitRuntimeIntrinsic(
		"table.get",
		FunctionType({ValueType::anyref}, TypeTuple({ValueType::i32, inferValueType<Uptr>()})),
		{index, getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.tableIndex])})[0];
	push(result);
}

void EmitFunctionContext::table_set(TableImm imm)
{
	llvm::Value* value = pop();
	llvm::Value* index = pop();
	emitRuntimeIntrinsic(
		"table.set",
		FunctionType({}, TypeTuple({ValueType::i32, ValueType::anyref, inferValueType<Uptr>()})),
		{index,
		 value,
		 getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.tableIndex])});
}

void EmitFunctionContext::table_init(ElemSegmentAndTableImm imm)
{
	auto numElements = pop();
	auto sourceOffset = pop();
	auto destOffset = pop();
	emitRuntimeIntrinsic(
		"table.init",
		FunctionType({},
					 TypeTuple({ValueType::i32,
								ValueType::i32,
								ValueType::i32,
								inferValueType<Uptr>(),
								inferValueType<Uptr>(),
								inferValueType<Uptr>()})),
		{destOffset,
		 sourceOffset,
		 numElements,
		 moduleContext.moduleInstanceId,
		 getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.tableIndex]),
		 emitLiteral(llvmContext, imm.elemSegmentIndex)});
}

void EmitFunctionContext::table_drop(ElemSegmentImm imm)
{
	emitRuntimeIntrinsic(
		"table.drop",
		FunctionType({}, TypeTuple({inferValueType<Uptr>(), inferValueType<Uptr>()})),
		{moduleContext.moduleInstanceId, emitLiteral(llvmContext, imm.elemSegmentIndex)});
}

void EmitFunctionContext::table_copy(TableImm imm)
{
	auto numElements = pop();
	auto sourceOffset = pop();
	auto destOffset = pop();

	emitRuntimeIntrinsic(
		"table.copy",
		FunctionType(
			{},
			TypeTuple({ValueType::i32, ValueType::i32, ValueType::i32, inferValueType<Uptr>()})),
		{destOffset,
		 sourceOffset,
		 numElements,
		 getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.tableIndex])});
}
