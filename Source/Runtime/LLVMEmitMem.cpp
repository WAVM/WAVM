#include "Inline/Assert.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMEmitModuleContext.h"
#include "LLVMJIT.h"

using namespace LLVMJIT;
using namespace IR;

//
// Memory size operators
// These just call out to wavmIntrinsics.growMemory/currentMemory, passing a pointer to the default
// memory for the module.
//

void EmitFunctionContext::memory_grow(MemoryImm)
{
	llvm::Value* deltaNumPages   = pop();
	ValueVector previousNumPages = emitRuntimeIntrinsic(
		"growMemory",
		FunctionType(TypeTuple(ValueType::i32), TypeTuple({ValueType::i32, ValueType::i64})),
		{deltaNumPages, emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id))});
	wavmAssert(previousNumPages.size() == 1);
	push(previousNumPages[0]);
}
void EmitFunctionContext::memory_size(MemoryImm)
{
	ValueVector currentNumPages = emitRuntimeIntrinsic(
		"currentMemory",
		FunctionType(TypeTuple(ValueType::i32), TypeTuple(ValueType::i64)),
		{emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id))});
	wavmAssert(currentNumPages.size() == 1);
	push(currentNumPages[0]);
}

//
// Load/store operators
//

#define EMIT_LOAD_OP(valueTypeId, name, llvmMemoryType, naturalAlignmentLog2, conversionOp)  \
	void EmitFunctionContext::valueTypeId##_##name(LoadOrStoreImm<naturalAlignmentLog2> imm) \
	{                                                                                        \
		auto byteIndex = pop();                                                              \
		auto pointer   = coerceByteIndexToPointer(byteIndex, imm.offset, llvmMemoryType);    \
		auto load      = irBuilder.CreateLoad(pointer);                                      \
		load->setAlignment(1 << imm.alignmentLog2);                                          \
		load->setVolatile(true);                                                             \
		push(conversionOp(load, asLLVMType(ValueType::valueTypeId)));                        \
	}
#define EMIT_STORE_OP(valueTypeId, name, llvmMemoryType, naturalAlignmentLog2, conversionOp) \
	void EmitFunctionContext::valueTypeId##_##name(LoadOrStoreImm<naturalAlignmentLog2> imm) \
	{                                                                                        \
		auto value       = pop();                                                            \
		auto byteIndex   = pop();                                                            \
		auto pointer     = coerceByteIndexToPointer(byteIndex, imm.offset, llvmMemoryType);  \
		auto memoryValue = conversionOp(value, llvmMemoryType);                              \
		auto store       = irBuilder.CreateStore(memoryValue, pointer);                      \
		store->setVolatile(true);                                                            \
		store->setAlignment(1 << imm.alignmentLog2);                                         \
	}

EMIT_LOAD_OP(i32, load8_s, llvmI8Type, 0, sext)
EMIT_LOAD_OP(i32, load8_u, llvmI8Type, 0, zext)
EMIT_LOAD_OP(i32, load16_s, llvmI16Type, 1, sext)
EMIT_LOAD_OP(i32, load16_u, llvmI16Type, 1, zext)
EMIT_LOAD_OP(i64, load8_s, llvmI8Type, 0, sext)
EMIT_LOAD_OP(i64, load8_u, llvmI8Type, 0, zext)
EMIT_LOAD_OP(i64, load16_s, llvmI16Type, 1, sext)
EMIT_LOAD_OP(i64, load16_u, llvmI16Type, 1, zext)
EMIT_LOAD_OP(i64, load32_s, llvmI32Type, 2, sext)
EMIT_LOAD_OP(i64, load32_u, llvmI32Type, 2, zext)

EMIT_LOAD_OP(i32, load, llvmI32Type, 2, identity)
EMIT_LOAD_OP(i64, load, llvmI64Type, 3, identity)
EMIT_LOAD_OP(f32, load, llvmF32Type, 2, identity)
EMIT_LOAD_OP(f64, load, llvmF64Type, 3, identity)

EMIT_STORE_OP(i32, store8, llvmI8Type, 0, trunc)
EMIT_STORE_OP(i64, store8, llvmI8Type, 0, trunc)
EMIT_STORE_OP(i32, store16, llvmI16Type, 1, trunc)
EMIT_STORE_OP(i64, store16, llvmI16Type, 1, trunc)
EMIT_STORE_OP(i32, store, llvmI32Type, 2, trunc)
EMIT_STORE_OP(i64, store32, llvmI32Type, 2, trunc)
EMIT_STORE_OP(i64, store, llvmI64Type, 3, identity)
EMIT_STORE_OP(f32, store, llvmF32Type, 2, identity)
EMIT_STORE_OP(f64, store, llvmF64Type, 3, identity)

EMIT_STORE_OP(v128, store, value->getType(), 4, identity)
EMIT_LOAD_OP(v128, load, llvmI64x2Type, 4, identity)

void EmitFunctionContext::atomic_wake(AtomicLoadOrStoreImm<2>)
{
	auto numWaiters = pop();
	auto address    = pop();
	auto memoryId   = emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id));
	push(emitRuntimeIntrinsic(
		"atomic_wake",
		FunctionType(
			TypeTuple{ValueType::i32}, TypeTuple{ValueType::i32, ValueType::i32, ValueType::i64}),
		{address, numWaiters, memoryId})[0]);
}
void EmitFunctionContext::i32_atomic_wait(AtomicLoadOrStoreImm<2>)
{
	auto timeout       = pop();
	auto expectedValue = pop();
	auto address       = pop();
	auto memoryId      = emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id));
	push(emitRuntimeIntrinsic(
		"atomic_wait_i32",
		FunctionType(
			TypeTuple{ValueType::i32},
			TypeTuple{ValueType::i32, ValueType::i32, ValueType::f64, ValueType::i64}),
		{address, expectedValue, timeout, memoryId})[0]);
}
void EmitFunctionContext::i64_atomic_wait(AtomicLoadOrStoreImm<3>)
{
	auto timeout       = pop();
	auto expectedValue = pop();
	auto address       = pop();
	auto memoryId      = emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id));
	push(emitRuntimeIntrinsic(
		"atomic_wait_i64",
		FunctionType(
			TypeTuple{ValueType::i32},
			TypeTuple{ValueType::i32, ValueType::i64, ValueType::f64, ValueType::i64}),
		{address, expectedValue, timeout, memoryId})[0]);
}

void EmitFunctionContext::trapIfMisalignedAtomic(llvm::Value* address, U32 alignmentLog2)
{
	if(alignmentLog2 > 0)
	{
		emitConditionalTrapIntrinsic(
			irBuilder.CreateICmpNE(
				typedZeroConstants[(Uptr)ValueType::i32],
				irBuilder.CreateAnd(address, emitLiteral((U32(1) << alignmentLog2) - 1))),
			"misalignedAtomicTrap",
			FunctionType(TypeTuple{}, TypeTuple{ValueType::i32}),
			{address});
	}
}

#define EMIT_ATOMIC_LOAD_OP(valueTypeId, name, llvmMemoryType, naturalAlignmentLog2, memToValue)   \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm) \
	{                                                                                              \
		auto byteIndex = pop();                                                                    \
		trapIfMisalignedAtomic(byteIndex, naturalAlignmentLog2);                                   \
		auto pointer = coerceByteIndexToPointer(byteIndex, imm.offset, llvmMemoryType);            \
		auto load    = irBuilder.CreateLoad(pointer);                                              \
		load->setAlignment(1 << imm.alignmentLog2);                                                \
		load->setVolatile(true);                                                                   \
		load->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);                             \
		push(memToValue(load, asLLVMType(ValueType::valueTypeId)));                                \
	}
#define EMIT_ATOMIC_STORE_OP(valueTypeId, name, llvmMemoryType, naturalAlignmentLog2, valueToMem)  \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm) \
	{                                                                                              \
		auto value     = pop();                                                                    \
		auto byteIndex = pop();                                                                    \
		trapIfMisalignedAtomic(byteIndex, naturalAlignmentLog2);                                   \
		auto pointer     = coerceByteIndexToPointer(byteIndex, imm.offset, llvmMemoryType);        \
		auto memoryValue = valueToMem(value, llvmMemoryType);                                      \
		auto store       = irBuilder.CreateStore(memoryValue, pointer);                            \
		store->setVolatile(true);                                                                  \
		store->setAlignment(1 << imm.alignmentLog2);                                               \
		store->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);                            \
	}
EMIT_ATOMIC_LOAD_OP(i32, atomic_load, llvmI32Type, 2, identity)
EMIT_ATOMIC_LOAD_OP(i64, atomic_load, llvmI64Type, 3, identity)

EMIT_ATOMIC_LOAD_OP(i32, atomic_load8_u, llvmI8Type, 0, zext)
EMIT_ATOMIC_LOAD_OP(i32, atomic_load16_u, llvmI16Type, 1, zext)
EMIT_ATOMIC_LOAD_OP(i64, atomic_load8_u, llvmI8Type, 0, zext)
EMIT_ATOMIC_LOAD_OP(i64, atomic_load16_u, llvmI16Type, 1, zext)
EMIT_ATOMIC_LOAD_OP(i64, atomic_load32_u, llvmI32Type, 2, zext)

EMIT_ATOMIC_STORE_OP(i32, atomic_store, llvmI32Type, 2, identity)
EMIT_ATOMIC_STORE_OP(i64, atomic_store, llvmI64Type, 3, identity)

EMIT_ATOMIC_STORE_OP(i32, atomic_store8, llvmI8Type, 0, trunc)
EMIT_ATOMIC_STORE_OP(i32, atomic_store16, llvmI16Type, 1, trunc)
EMIT_ATOMIC_STORE_OP(i64, atomic_store8, llvmI8Type, 0, trunc)
EMIT_ATOMIC_STORE_OP(i64, atomic_store16, llvmI16Type, 1, trunc)
EMIT_ATOMIC_STORE_OP(i64, atomic_store32, llvmI32Type, 2, trunc)

#define EMIT_ATOMIC_CMPXCHG(                                                                  \
	valueTypeId, name, llvmMemoryType, alignmentLog2, memToValue, valueToMem)                 \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<alignmentLog2> imm)   \
	{                                                                                         \
		auto replacementValue = valueToMem(pop(), llvmMemoryType);                            \
		auto expectedValue    = valueToMem(pop(), llvmMemoryType);                            \
		auto byteIndex        = pop();                                                        \
		trapIfMisalignedAtomic(byteIndex, alignmentLog2);                                     \
		auto pointer       = coerceByteIndexToPointer(byteIndex, imm.offset, llvmMemoryType); \
		auto atomicCmpXchg = irBuilder.CreateAtomicCmpXchg(                                   \
			pointer,                                                                          \
			expectedValue,                                                                    \
			replacementValue,                                                                 \
			llvm::AtomicOrdering::SequentiallyConsistent,                                     \
			llvm::AtomicOrdering::SequentiallyConsistent);                                    \
		atomicCmpXchg->setVolatile(true);                                                     \
		auto previousValue = irBuilder.CreateExtractValue(atomicCmpXchg, {0});                \
		push(memToValue(previousValue, asLLVMType(ValueType::valueTypeId)));                  \
	}

EMIT_ATOMIC_CMPXCHG(i32, atomic_rmw8_u_cmpxchg, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i32, atomic_rmw16_u_cmpxchg, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i32, atomic_rmw_cmpxchg, llvmI32Type, 2, identity, identity)

EMIT_ATOMIC_CMPXCHG(i64, atomic_rmw8_u_cmpxchg, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i64, atomic_rmw16_u_cmpxchg, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i64, atomic_rmw32_u_cmpxchg, llvmI32Type, 2, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i64, atomic_rmw_cmpxchg, llvmI64Type, 3, identity, identity)

#define EMIT_ATOMIC_RMW(                                                                    \
	valueTypeId, name, rmwOpId, llvmMemoryType, alignmentLog2, memToValue, valueToMem)      \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<alignmentLog2> imm) \
	{                                                                                       \
		auto value     = valueToMem(pop(), llvmMemoryType);                                 \
		auto byteIndex = pop();                                                             \
		trapIfMisalignedAtomic(byteIndex, alignmentLog2);                                   \
		auto pointer   = coerceByteIndexToPointer(byteIndex, imm.offset, llvmMemoryType);   \
		auto atomicRMW = irBuilder.CreateAtomicRMW(                                         \
			llvm::AtomicRMWInst::BinOp::rmwOpId,                                            \
			pointer,                                                                        \
			value,                                                                          \
			llvm::AtomicOrdering::SequentiallyConsistent);                                  \
		atomicRMW->setVolatile(true);                                                       \
		push(memToValue(atomicRMW, asLLVMType(ValueType::valueTypeId)));                    \
	}

EMIT_ATOMIC_RMW(i32, atomic_rmw8_u_xchg, Xchg, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_u_xchg, Xchg, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_xchg, Xchg, llvmI32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_u_xchg, Xchg, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_u_xchg, Xchg, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_u_xchg, Xchg, llvmI32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_xchg, Xchg, llvmI64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_u_add, Add, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_u_add, Add, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_add, Add, llvmI32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_u_add, Add, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_u_add, Add, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_u_add, Add, llvmI32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_add, Add, llvmI64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_u_sub, Sub, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_u_sub, Sub, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_sub, Sub, llvmI32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_u_sub, Sub, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_u_sub, Sub, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_u_sub, Sub, llvmI32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_sub, Sub, llvmI64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_u_and, And, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_u_and, And, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_and, And, llvmI32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_u_and, And, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_u_and, And, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_u_and, And, llvmI32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_and, And, llvmI64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_u_or, Or, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_u_or, Or, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_or, Or, llvmI32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_u_or, Or, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_u_or, Or, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_u_or, Or, llvmI32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_or, Or, llvmI64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_u_xor, Xor, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_u_xor, Xor, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_xor, Xor, llvmI32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_u_xor, Xor, llvmI8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_u_xor, Xor, llvmI16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_u_xor, Xor, llvmI32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_xor, Xor, llvmI64Type, 3, identity, identity)
