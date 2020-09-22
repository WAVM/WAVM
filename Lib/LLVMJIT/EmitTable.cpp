#include "EmitContext.h"
#include "EmitFunctionContext.h"
#include "EmitModuleContext.h"
#include "LLVMJITPrivate.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

void EmitFunctionContext::ref_null(ReferenceTypeImm imm)
{
	push(llvm::Constant::getNullValue(llvmContext.externrefType));
}

void EmitFunctionContext::ref_is_null(NoImm)
{
	llvm::Value* reference = pop();
	llvm::Value* null = llvm::Constant::getNullValue(llvmContext.externrefType);
	llvm::Value* isNull = irBuilder.CreateICmpEQ(reference, null);
	push(coerceBoolToI32(isNull));
}

void EmitFunctionContext::ref_func(FunctionRefImm imm)
{
	llvm::Value* referencedFunction = moduleContext.functions[imm.functionIndex];
	llvm::Value* codeAddress = irBuilder.CreatePtrToInt(referencedFunction, llvmContext.iptrType);
	llvm::Value* functionAddress = irBuilder.CreateSub(
		codeAddress, emitLiteral(llvmContext, Uptr(offsetof(Runtime::Function, code))));
	llvm::Value* externref = irBuilder.CreateIntToPtr(functionAddress, llvmContext.externrefType);
	push(externref);
}

void EmitFunctionContext::table_get(TableImm imm)
{
	llvm::Value* index = pop();
	llvm::Value* result = emitRuntimeIntrinsic(
		"table.get",
		FunctionType({ValueType::externref},
					 TypeTuple({ValueType::i32, inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
		{index, getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.tableIndex])})[0];
	push(result);
}

void EmitFunctionContext::table_set(TableImm imm)
{
	llvm::Value* value = pop();
	llvm::Value* index = pop();
	emitRuntimeIntrinsic(
		"table.set",
		FunctionType({},
					 TypeTuple({ValueType::i32, ValueType::externref, inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
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
								inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
		{destOffset,
		 sourceOffset,
		 numElements,
		 moduleContext.instanceId,
		 getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.tableIndex]),
		 emitLiteral(llvmContext, imm.elemSegmentIndex)});
}

void EmitFunctionContext::elem_drop(ElemSegmentImm imm)
{
	emitRuntimeIntrinsic(
		"elem.drop",
		FunctionType({},
					 TypeTuple({inferValueType<Uptr>(), inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
		{moduleContext.instanceId, emitLiteral(llvmContext, imm.elemSegmentIndex)});
}

void EmitFunctionContext::table_copy(TableCopyImm imm)
{
	auto numElements = pop();
	auto sourceOffset = pop();
	auto destOffset = pop();

	emitRuntimeIntrinsic(
		"table.copy",
		FunctionType({},
					 TypeTuple({ValueType::i32,
								ValueType::i32,
								ValueType::i32,
								inferValueType<Uptr>(),
								inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
		{destOffset,
		 sourceOffset,
		 numElements,
		 getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.destTableIndex]),
		 getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.sourceTableIndex])});
}

void EmitFunctionContext::table_fill(TableImm imm)
{
	auto numElements = pop();
	auto value = pop();
	auto destOffset = pop();

	emitRuntimeIntrinsic(
		"table.fill",
		FunctionType(
			{},
			TypeTuple(
				{ValueType::i32, ValueType::externref, ValueType::i32, inferValueType<Uptr>()}),
			IR::CallingConvention::intrinsic),
		{destOffset,
		 value,
		 numElements,
		 getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.tableIndex])});
}

void EmitFunctionContext::table_grow(TableImm imm)
{
	llvm::Value* deltaNumElements = pop();
	llvm::Value* value = pop();
	ValueVector previousNumElements = emitRuntimeIntrinsic(
		"table.grow",
		FunctionType(TypeTuple(ValueType::i32),
					 TypeTuple({ValueType::externref, ValueType::i32, inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
		{value,
		 deltaNumElements,
		 getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.tableIndex])});
	WAVM_ASSERT(previousNumElements.size() == 1);
	push(previousNumElements[0]);
}
void EmitFunctionContext::table_size(TableImm imm)
{
	ValueVector currentNumElements = emitRuntimeIntrinsic(
		"table.size",
		FunctionType(TypeTuple(ValueType::i32),
					 TypeTuple(inferValueType<Uptr>()),
					 IR::CallingConvention::intrinsic),
		{getTableIdFromOffset(llvmContext, moduleContext.tableOffsets[imm.tableIndex])});
	WAVM_ASSERT(currentNumElements.size() == 1);
	push(currentNumElements[0]);
}
