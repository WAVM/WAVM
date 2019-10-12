#include "EmitContext.h"
#include "EmitFunctionContext.h"
#include "EmitModuleContext.h"
#include "LLVMJITPrivate.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/AtomicOrdering.h>
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

// Bounds checks a sandboxed memory address + offset, and returns an offset relative to the memory
// base address that is guaranteed to be within the virtual address space allocated for the linear
// memory object.
static llvm::Value* getOffsetAndBoundedAddress(EmitContext& emitContext,
											   llvm::Value* address,
											   U32 offset)
{
	// zext the 32-bit address to 64-bits.
	// This is crucial for security, as LLVM will otherwise implicitly sign extend it to 64-bits in
	// the GEP below, interpreting it as a signed offset and allowing access to memory outside the
	// sandboxed memory range. There are no 'far addresses' in a 32 bit runtime.
	address = emitContext.irBuilder.CreateZExt(address, emitContext.llvmContext.i64Type);

	// Add the offset to the byte index.
	if(offset)
	{
		address = emitContext.irBuilder.CreateAdd(
			address,
			emitContext.irBuilder.CreateZExt(emitLiteral(emitContext.llvmContext, offset),
											 emitContext.llvmContext.i64Type));
	}

	// If HAS_64BIT_ADDRESS_SPACE, the memory has enough virtual address space allocated to ensure
	// that any 32-bit byte index + 32-bit offset will fall within the virtual address sandbox, so
	// no explicit bounds check is necessary.

	return address;
}

llvm::Value* EmitFunctionContext::coerceAddressToPointer(llvm::Value* boundedAddress,
														 llvm::Type* memoryType,
														 Uptr memoryIndex)
{
	llvm::Value* memoryBasePointer = irBuilder.CreateLoad(memoryBasePointerVariables[memoryIndex]);
	llvm::Value* bytePointer = irBuilder.CreateInBoundsGEP(memoryBasePointer, boundedAddress);

	// Cast the pointer to the appropriate type.
	return irBuilder.CreatePointerCast(bytePointer, memoryType->getPointerTo());
}

//
// Memory size operators
// These just call out to wavmIntrinsics.growMemory/currentMemory, passing a pointer to the default
// memory for the module.
//

void EmitFunctionContext::memory_grow(MemoryImm imm)
{
	llvm::Value* deltaNumPages = pop();
	ValueVector previousNumPages = emitRuntimeIntrinsic(
		"memory.grow",
		FunctionType(TypeTuple(ValueType::i32),
					 TypeTuple({ValueType::i32, inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
		{deltaNumPages,
		 getMemoryIdFromOffset(llvmContext, moduleContext.memoryOffsets[imm.memoryIndex])});
	WAVM_ASSERT(previousNumPages.size() == 1);
	push(previousNumPages[0]);
}
void EmitFunctionContext::memory_size(MemoryImm imm)
{
	ValueVector currentNumPages = emitRuntimeIntrinsic(
		"memory.size",
		FunctionType(TypeTuple(ValueType::i32),
					 TypeTuple(inferValueType<Uptr>()),
					 IR::CallingConvention::intrinsic),
		{getMemoryIdFromOffset(llvmContext, moduleContext.memoryOffsets[imm.memoryIndex])});
	WAVM_ASSERT(currentNumPages.size() == 1);
	push(currentNumPages[0]);
}

//
// Memory bulk operators.
//

void EmitFunctionContext::memory_init(DataSegmentAndMemImm imm)
{
	auto numBytes = pop();
	auto sourceOffset = pop();
	auto destAddress = pop();
	emitRuntimeIntrinsic(
		"memory.init",
		FunctionType({},
					 TypeTuple({ValueType::i32,
								ValueType::i32,
								ValueType::i32,
								inferValueType<Uptr>(),
								inferValueType<Uptr>(),
								inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
		{destAddress,
		 sourceOffset,
		 numBytes,
		 moduleContext.moduleInstanceId,
		 getMemoryIdFromOffset(llvmContext, moduleContext.memoryOffsets[imm.memoryIndex]),
		 emitLiteral(llvmContext, imm.dataSegmentIndex)});
}

void EmitFunctionContext::data_drop(DataSegmentImm imm)
{
	emitRuntimeIntrinsic(
		"data.drop",
		FunctionType({},
					 TypeTuple({inferValueType<Uptr>(), inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
		{moduleContext.moduleInstanceId, emitLiteral(llvmContext, imm.dataSegmentIndex)});
}

void EmitFunctionContext::memory_copy(MemoryCopyImm imm)
{
	auto numBytes = pop();
	auto sourceAddress = pop();
	auto destAddress = pop();

	emitRuntimeIntrinsic(
		"memory.copy",
		FunctionType({},
					 TypeTuple({ValueType::i32,
								ValueType::i32,
								ValueType::i32,
								inferValueType<Uptr>(),
								inferValueType<Uptr>()}),
					 IR::CallingConvention::intrinsic),
		{destAddress,
		 sourceAddress,
		 numBytes,
		 getMemoryIdFromOffset(llvmContext, moduleContext.memoryOffsets[imm.destMemoryIndex]),
		 getMemoryIdFromOffset(llvmContext, moduleContext.memoryOffsets[imm.sourceMemoryIndex])});
}

void EmitFunctionContext::memory_fill(MemoryImm imm)
{
	auto numBytes = pop();
	auto value = pop();
	auto destAddress = pop();

	emitRuntimeIntrinsic(
		"memory.fill",
		FunctionType(
			{},
			TypeTuple({ValueType::i32, ValueType::i32, ValueType::i32, inferValueType<Uptr>()}),
			IR::CallingConvention::intrinsic),
		{destAddress,
		 value,
		 numBytes,
		 getMemoryIdFromOffset(llvmContext, moduleContext.memoryOffsets[imm.memoryIndex])});
}

//
// Load/store operators
//

#define EMIT_LOAD_OP(destType, name, llvmMemoryType, naturalAlignmentLog2, conversionOp)           \
	void EmitFunctionContext::name(LoadOrStoreImm<naturalAlignmentLog2> imm)                       \
	{                                                                                              \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(*this, address, imm.offset);              \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto load = irBuilder.CreateLoad(pointer);                                                 \
		/* Don't trust the alignment hint provided by the WebAssembly code, since the load can't   \
		 * trap if it's wrong. */                                                                  \
		load->setAlignment(1);                                                                     \
		load->setVolatile(true);                                                                   \
		push(conversionOp(load, destType));                                                        \
	}
#define EMIT_STORE_OP(valueTypeId, name, llvmMemoryType, naturalAlignmentLog2, conversionOp)       \
	void EmitFunctionContext::name(LoadOrStoreImm<naturalAlignmentLog2> imm)                       \
	{                                                                                              \
		auto value = pop();                                                                        \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(*this, address, imm.offset);              \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto memoryValue = conversionOp(value, llvmMemoryType);                                    \
		auto store = irBuilder.CreateStore(memoryValue, pointer);                                  \
		store->setVolatile(true);                                                                  \
		/* Don't trust the alignment hint provided by the WebAssembly code, since the store can't  \
		 * trap if it's wrong. */                                                                  \
		store->setAlignment(1);                                                                    \
	}

EMIT_LOAD_OP(llvmContext.i32Type, i32_load8_s, llvmContext.i8Type, 0, sext)
EMIT_LOAD_OP(llvmContext.i32Type, i32_load8_u, llvmContext.i8Type, 0, zext)
EMIT_LOAD_OP(llvmContext.i32Type, i32_load16_s, llvmContext.i16Type, 1, sext)
EMIT_LOAD_OP(llvmContext.i32Type, i32_load16_u, llvmContext.i16Type, 1, zext)
EMIT_LOAD_OP(llvmContext.i64Type, i64_load8_s, llvmContext.i8Type, 0, sext)
EMIT_LOAD_OP(llvmContext.i64Type, i64_load8_u, llvmContext.i8Type, 0, zext)
EMIT_LOAD_OP(llvmContext.i64Type, i64_load16_s, llvmContext.i16Type, 1, sext)
EMIT_LOAD_OP(llvmContext.i64Type, i64_load16_u, llvmContext.i16Type, 1, zext)
EMIT_LOAD_OP(llvmContext.i64Type, i64_load32_s, llvmContext.i32Type, 2, sext)
EMIT_LOAD_OP(llvmContext.i64Type, i64_load32_u, llvmContext.i32Type, 2, zext)

EMIT_LOAD_OP(llvmContext.i32Type, i32_load, llvmContext.i32Type, 2, identity)
EMIT_LOAD_OP(llvmContext.i64Type, i64_load, llvmContext.i64Type, 3, identity)
EMIT_LOAD_OP(llvmContext.f32Type, f32_load, llvmContext.f32Type, 2, identity)
EMIT_LOAD_OP(llvmContext.f64Type, f64_load, llvmContext.f64Type, 3, identity)

EMIT_STORE_OP(i32, i32_store8, llvmContext.i8Type, 0, trunc)
EMIT_STORE_OP(i64, i64_store8, llvmContext.i8Type, 0, trunc)
EMIT_STORE_OP(i32, i32_store16, llvmContext.i16Type, 1, trunc)
EMIT_STORE_OP(i64, i64_store16, llvmContext.i16Type, 1, trunc)
EMIT_STORE_OP(i32, i32_store, llvmContext.i32Type, 2, trunc)
EMIT_STORE_OP(i64, i64_store32, llvmContext.i32Type, 2, trunc)
EMIT_STORE_OP(i64, i64_store, llvmContext.i64Type, 3, identity)
EMIT_STORE_OP(f32, f32_store, llvmContext.f32Type, 2, identity)
EMIT_STORE_OP(f64, f64_store, llvmContext.f64Type, 3, identity)

EMIT_STORE_OP(v128, v128_store, value->getType(), 4, identity)
EMIT_LOAD_OP(llvmContext.i64x2Type, v128_load, llvmContext.i64x2Type, 4, identity)

EMIT_LOAD_OP(llvmContext.i8x16Type, v8x16_load_splat, llvmContext.i8Type, 0, splat<16>)
EMIT_LOAD_OP(llvmContext.i16x8Type, v16x8_load_splat, llvmContext.i16Type, 1, splat<8>)
EMIT_LOAD_OP(llvmContext.i32x4Type, v32x4_load_splat, llvmContext.i32Type, 2, splat<4>)
EMIT_LOAD_OP(llvmContext.i64x2Type, v64x2_load_splat, llvmContext.i64Type, 3, splat<2>)

EMIT_LOAD_OP(llvmContext.i16x8Type, i16x8_load8x8_s, llvmContext.i8x8Type, 3, sext)
EMIT_LOAD_OP(llvmContext.i16x8Type, i16x8_load8x8_u, llvmContext.i8x8Type, 3, zext)
EMIT_LOAD_OP(llvmContext.i32x4Type, i32x4_load16x4_s, llvmContext.i16x4Type, 3, sext)
EMIT_LOAD_OP(llvmContext.i32x4Type, i32x4_load16x4_u, llvmContext.i16x4Type, 3, zext)
EMIT_LOAD_OP(llvmContext.i64x2Type, i64x2_load32x2_s, llvmContext.i32x2Type, 3, sext)
EMIT_LOAD_OP(llvmContext.i64x2Type, i64x2_load32x2_u, llvmContext.i32x2Type, 3, zext)

void EmitFunctionContext::trapIfMisalignedAtomic(llvm::Value* address, U32 alignmentLog2)
{
	if(alignmentLog2 > 0)
	{
		emitConditionalTrapIntrinsic(
			irBuilder.CreateICmpNE(
				llvmContext.typedZeroConstants[(Uptr)ValueType::i64],
				irBuilder.CreateAnd(address,
									emitLiteral(llvmContext, (U64(1) << alignmentLog2) - 1))),
			"misalignedAtomicTrap",
			FunctionType(TypeTuple{}, TypeTuple{ValueType::i64}, IR::CallingConvention::intrinsic),
			{address});
	}
}

void EmitFunctionContext::atomic_notify(AtomicLoadOrStoreImm<2> imm)
{
	llvm::Value* numWaiters = pop();
	llvm::Value* address = pop();
	llvm::Value* boundedAddress = getOffsetAndBoundedAddress(*this, address, imm.offset);
	trapIfMisalignedAtomic(boundedAddress, imm.alignmentLog2);
	push(emitRuntimeIntrinsic(
		"atomic_notify",
		FunctionType(TypeTuple{ValueType::i32},
					 TypeTuple{ValueType::i32, ValueType::i32, ValueType::i64},
					 IR::CallingConvention::intrinsic),
		{address,
		 numWaiters,
		 getMemoryIdFromOffset(llvmContext, moduleContext.memoryOffsets[imm.memoryIndex])})[0]);
}
void EmitFunctionContext::i32_atomic_wait(AtomicLoadOrStoreImm<2> imm)
{
	llvm::Value* timeout = pop();
	llvm::Value* expectedValue = pop();
	llvm::Value* address = pop();
	llvm::Value* boundedAddress = getOffsetAndBoundedAddress(*this, address, imm.offset);
	trapIfMisalignedAtomic(boundedAddress, imm.alignmentLog2);
	push(emitRuntimeIntrinsic(
		"atomic_wait_i32",
		FunctionType(
			TypeTuple{ValueType::i32},
			TypeTuple{ValueType::i32, ValueType::i32, ValueType::i64, inferValueType<Uptr>()},
			IR::CallingConvention::intrinsic),
		{address,
		 expectedValue,
		 timeout,
		 getMemoryIdFromOffset(llvmContext, moduleContext.memoryOffsets[imm.memoryIndex])})[0]);
}
void EmitFunctionContext::i64_atomic_wait(AtomicLoadOrStoreImm<3> imm)
{
	llvm::Value* timeout = pop();
	llvm::Value* expectedValue = pop();
	llvm::Value* address = pop();
	llvm::Value* boundedAddress = getOffsetAndBoundedAddress(*this, address, imm.offset);
	trapIfMisalignedAtomic(boundedAddress, imm.alignmentLog2);
	push(emitRuntimeIntrinsic(
		"atomic_wait_i64",
		FunctionType(
			TypeTuple{ValueType::i32},
			TypeTuple{ValueType::i32, ValueType::i64, ValueType::i64, inferValueType<Uptr>()},
			IR::CallingConvention::intrinsic),
		{address,
		 expectedValue,
		 timeout,
		 getMemoryIdFromOffset(llvmContext, moduleContext.memoryOffsets[imm.memoryIndex])})[0]);
}

void EmitFunctionContext::atomic_fence(AtomicFenceImm imm)
{
	switch(imm.order)
	{
	case MemoryOrder::sequentiallyConsistent:
		irBuilder.CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
		break;
	default: WAVM_UNREACHABLE();
	};
}

#define EMIT_ATOMIC_LOAD_OP(valueTypeId, name, llvmMemoryType, naturalAlignmentLog2, memToValue)   \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm) \
	{                                                                                              \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(*this, address, imm.offset);              \
		trapIfMisalignedAtomic(boundedAddress, naturalAlignmentLog2);                              \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto load = irBuilder.CreateLoad(pointer);                                                 \
		load->setAlignment(1 << imm.alignmentLog2);                                                \
		load->setVolatile(true);                                                                   \
		load->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);                             \
		push(memToValue(load, asLLVMType(llvmContext, ValueType::valueTypeId)));                   \
	}
#define EMIT_ATOMIC_STORE_OP(valueTypeId, name, llvmMemoryType, naturalAlignmentLog2, valueToMem)  \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm) \
	{                                                                                              \
		auto value = pop();                                                                        \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(*this, address, imm.offset);              \
		trapIfMisalignedAtomic(boundedAddress, naturalAlignmentLog2);                              \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto memoryValue = valueToMem(value, llvmMemoryType);                                      \
		auto store = irBuilder.CreateStore(memoryValue, pointer);                                  \
		store->setVolatile(true);                                                                  \
		store->setAlignment(1 << imm.alignmentLog2);                                               \
		store->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);                            \
	}
EMIT_ATOMIC_LOAD_OP(i32, atomic_load, llvmContext.i32Type, 2, identity)
EMIT_ATOMIC_LOAD_OP(i64, atomic_load, llvmContext.i64Type, 3, identity)

EMIT_ATOMIC_LOAD_OP(i32, atomic_load8_u, llvmContext.i8Type, 0, zext)
EMIT_ATOMIC_LOAD_OP(i32, atomic_load16_u, llvmContext.i16Type, 1, zext)
EMIT_ATOMIC_LOAD_OP(i64, atomic_load8_u, llvmContext.i8Type, 0, zext)
EMIT_ATOMIC_LOAD_OP(i64, atomic_load16_u, llvmContext.i16Type, 1, zext)
EMIT_ATOMIC_LOAD_OP(i64, atomic_load32_u, llvmContext.i32Type, 2, zext)

EMIT_ATOMIC_STORE_OP(i32, atomic_store, llvmContext.i32Type, 2, identity)
EMIT_ATOMIC_STORE_OP(i64, atomic_store, llvmContext.i64Type, 3, identity)

EMIT_ATOMIC_STORE_OP(i32, atomic_store8, llvmContext.i8Type, 0, trunc)
EMIT_ATOMIC_STORE_OP(i32, atomic_store16, llvmContext.i16Type, 1, trunc)
EMIT_ATOMIC_STORE_OP(i64, atomic_store8, llvmContext.i8Type, 0, trunc)
EMIT_ATOMIC_STORE_OP(i64, atomic_store16, llvmContext.i16Type, 1, trunc)
EMIT_ATOMIC_STORE_OP(i64, atomic_store32, llvmContext.i32Type, 2, trunc)

#define EMIT_ATOMIC_CMPXCHG(                                                                       \
	valueTypeId, name, llvmMemoryType, alignmentLog2, memToValue, valueToMem)                      \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<alignmentLog2> imm)        \
	{                                                                                              \
		auto replacementValue = valueToMem(pop(), llvmMemoryType);                                 \
		auto expectedValue = valueToMem(pop(), llvmMemoryType);                                    \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(*this, address, imm.offset);              \
		trapIfMisalignedAtomic(boundedAddress, alignmentLog2);                                     \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto atomicCmpXchg                                                                         \
			= irBuilder.CreateAtomicCmpXchg(pointer,                                               \
											expectedValue,                                         \
											replacementValue,                                      \
											llvm::AtomicOrdering::SequentiallyConsistent,          \
											llvm::AtomicOrdering::SequentiallyConsistent);         \
		atomicCmpXchg->setVolatile(true);                                                          \
		auto previousValue = irBuilder.CreateExtractValue(atomicCmpXchg, {0});                     \
		push(memToValue(previousValue, asLLVMType(llvmContext, ValueType::valueTypeId)));          \
	}

EMIT_ATOMIC_CMPXCHG(i32, atomic_rmw8_cmpxchg_u, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i32, atomic_rmw16_cmpxchg_u, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i32, atomic_rmw_cmpxchg, llvmContext.i32Type, 2, identity, identity)

EMIT_ATOMIC_CMPXCHG(i64, atomic_rmw8_cmpxchg_u, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i64, atomic_rmw16_cmpxchg_u, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i64, atomic_rmw32_cmpxchg_u, llvmContext.i32Type, 2, zext, trunc)
EMIT_ATOMIC_CMPXCHG(i64, atomic_rmw_cmpxchg, llvmContext.i64Type, 3, identity, identity)

#define EMIT_ATOMIC_RMW(                                                                           \
	valueTypeId, name, rmwOpId, llvmMemoryType, alignmentLog2, memToValue, valueToMem)             \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<alignmentLog2> imm)        \
	{                                                                                              \
		auto value = valueToMem(pop(), llvmMemoryType);                                            \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(*this, address, imm.offset);              \
		trapIfMisalignedAtomic(boundedAddress, alignmentLog2);                                     \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto atomicRMW = irBuilder.CreateAtomicRMW(llvm::AtomicRMWInst::BinOp::rmwOpId,            \
												   pointer,                                        \
												   value,                                          \
												   llvm::AtomicOrdering::SequentiallyConsistent);  \
		atomicRMW->setVolatile(true);                                                              \
		push(memToValue(atomicRMW, asLLVMType(llvmContext, ValueType::valueTypeId)));              \
	}

EMIT_ATOMIC_RMW(i32, atomic_rmw8_xchg_u, Xchg, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_xchg_u, Xchg, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_xchg, Xchg, llvmContext.i32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_xchg_u, Xchg, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_xchg_u, Xchg, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_xchg_u, Xchg, llvmContext.i32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_xchg, Xchg, llvmContext.i64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_add_u, Add, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_add_u, Add, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_add, Add, llvmContext.i32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_add_u, Add, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_add_u, Add, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_add_u, Add, llvmContext.i32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_add, Add, llvmContext.i64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_sub_u, Sub, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_sub_u, Sub, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_sub, Sub, llvmContext.i32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_sub_u, Sub, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_sub_u, Sub, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_sub_u, Sub, llvmContext.i32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_sub, Sub, llvmContext.i64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_and_u, And, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_and_u, And, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_and, And, llvmContext.i32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_and_u, And, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_and_u, And, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_and_u, And, llvmContext.i32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_and, And, llvmContext.i64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_or_u, Or, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_or_u, Or, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_or, Or, llvmContext.i32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_or_u, Or, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_or_u, Or, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_or_u, Or, llvmContext.i32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_or, Or, llvmContext.i64Type, 3, identity, identity)

EMIT_ATOMIC_RMW(i32, atomic_rmw8_xor_u, Xor, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw16_xor_u, Xor, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i32, atomic_rmw_xor, Xor, llvmContext.i32Type, 2, identity, identity)

EMIT_ATOMIC_RMW(i64, atomic_rmw8_xor_u, Xor, llvmContext.i8Type, 0, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw16_xor_u, Xor, llvmContext.i16Type, 1, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw32_xor_u, Xor, llvmContext.i32Type, 2, zext, trunc)
EMIT_ATOMIC_RMW(i64, atomic_rmw_xor, Xor, llvmContext.i64Type, 3, identity, identity)
