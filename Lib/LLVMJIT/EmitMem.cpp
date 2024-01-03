#include "EmitContext.h"
#include "EmitFunctionContext.h"
#include "EmitModuleContext.h"
#include "LLVMJITPrivate.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/AtomicOrdering.h>

#if LLVM_VERSION_MAJOR >= 10
#include <llvm/IR/IntrinsicsAArch64.h>
#endif
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

enum class BoundsCheckOp
{
	clampToGuardRegion,
	trapOnOutOfBounds
};

static llvm::Value* getMemoryNumPages(EmitFunctionContext& functionContext, Uptr memoryIndex)
{
	llvm::Constant* memoryOffset = functionContext.moduleContext.memoryOffsets[memoryIndex];

	// Load the number of memory pages from the compartment runtime data.
	llvm::LoadInst* memoryNumPagesLoad = functionContext.loadFromUntypedPointer(
		::WAVM::LLVMJIT::wavmCreateInBoundsGEP(
			functionContext.irBuilder,
			functionContext.irBuilder.getInt8Ty(),
			functionContext.getCompartmentAddress(),
			{llvm::ConstantExpr::getAdd(
				memoryOffset,
				emitLiteralIptr(offsetof(Runtime::MemoryRuntimeData, numPages),
								functionContext.moduleContext.iptrType))}),
		functionContext.moduleContext.iptrType,
		functionContext.moduleContext.iptrAlignment);
	memoryNumPagesLoad->setAtomic(llvm::AtomicOrdering::Acquire);

	return memoryNumPagesLoad;
}

static llvm::Value* getMemoryNumBytes(EmitFunctionContext& functionContext, Uptr memoryIndex)
{
	return functionContext.irBuilder.CreateMul(
		getMemoryNumPages(functionContext, memoryIndex),
		emitLiteralIptr(IR::numBytesPerPage, functionContext.moduleContext.iptrType));
}

// Bounds checks a sandboxed memory address + offset, and returns an offset relative to the memory
// base address that is guaranteed to be within the virtual address space allocated for the linear
// memory object.
static llvm::Value* getOffsetAndBoundedAddress(EmitFunctionContext& functionContext,
											   Uptr memoryIndex,
											   llvm::Value* address,
											   llvm::Value* numBytes,
											   U64 offset,
											   BoundsCheckOp boundsCheckOp,
											   bool istagging = false)
{
	const MemoryType& memoryType
		= functionContext.moduleContext.irModule.memories.getType(memoryIndex);
	const bool is32bitMemoryOn64bitHost
		= memoryType.indexType == IndexType::i32
		  && functionContext.moduleContext.iptrValueType == ValueType::i64;

	llvm::IRBuilder<>& irBuilder = functionContext.irBuilder;
	auto& meminfo{functionContext.memoryInfos[memoryIndex]};

	auto memtagBasePointerVariable = meminfo.memtagBasePointerVariable;
	::llvm::Value* taggedval{};
	if(memtagBasePointerVariable) // memtag needs to ignore upper 8 bits
	{
		if(memoryType.indexType == IndexType::i64)
		{
			auto pointertype = address->getType();
			address = irBuilder.CreatePtrToInt(address, functionContext.llvmContext.i64Type);
			taggedval = irBuilder.CreateTrunc(irBuilder.CreateLShr(address, 56),
											  functionContext.llvmContext.i8Type);
			address = irBuilder.CreateAnd(address, irBuilder.getInt64(0x00FFFFFFFFFFFFFF));
			address = irBuilder.CreateIntToPtr(address, pointertype);
		}
		else
		{
			auto pointertype = address->getType();
			address = irBuilder.CreatePtrToInt(address, functionContext.llvmContext.i32Type);
			taggedval = irBuilder.CreateTrunc(irBuilder.CreateLShr(address, 30),
											  functionContext.llvmContext.i8Type);
			address = irBuilder.CreateAnd(address, irBuilder.getInt32(0x3FFFFFFF));
			address = irBuilder.CreateIntToPtr(address, pointertype);
		}
	}
	numBytes = irBuilder.CreateZExt(numBytes, address->getType());
	WAVM_ASSERT(numBytes->getType() == address->getType());

	if(memoryType.indexType == IndexType::i32)
	{
		// zext a 32-bit address and number of bytes to the target machine pointer width.
		// This is crucial for security, as LLVM will otherwise implicitly sign extend it to the
		// target machine pointer width in the GEP below, interpreting it as a signed offset and
		// allowing access to memory outside the sandboxed memory range.
		address = irBuilder.CreateZExt(address, functionContext.moduleContext.iptrType);
		numBytes = irBuilder.CreateZExt(numBytes, functionContext.moduleContext.iptrType);
	}

	// If the offset is greater than the size of the guard region, add it before bounds checking,
	// and check for overflow.
	if(offset && offset >= Runtime::memoryNumGuardBytes)
	{
		llvm::Constant* offsetConstant
			= emitLiteralIptr(offset, functionContext.moduleContext.iptrType);

		if(is32bitMemoryOn64bitHost)
		{
			// This is a 64-bit add of two numbers zero-extended from 32-bit, so it can't overflow.
			address = irBuilder.CreateAdd(address, offsetConstant);
		}
		else
		{
			llvm::Value* addressPlusOffsetAndOverflow
				= functionContext.callLLVMIntrinsic({functionContext.moduleContext.iptrType},
													llvm::Intrinsic::uadd_with_overflow,
													{address, offsetConstant});

			llvm::Value* addressPlusOffset
				= irBuilder.CreateExtractValue(addressPlusOffsetAndOverflow, {0});
			llvm::Value* addressPlusOffsetOverflowed
				= irBuilder.CreateExtractValue(addressPlusOffsetAndOverflow, {1});

			address
				= irBuilder.CreateOr(addressPlusOffset,
									 irBuilder.CreateSExt(addressPlusOffsetOverflowed,
														  functionContext.moduleContext.iptrType));
		}
	}

	if(boundsCheckOp == BoundsCheckOp::trapOnOutOfBounds)
	{
		// If the caller requires a trap, test whether the addressed bytes are within the bounds of
		// the memory, and if not call a trap intrinsic.
		llvm::Value* memoryNumBytes = getMemoryNumBytes(functionContext, memoryIndex);
		llvm::Value* memoryNumBytesMinusNumBytes = irBuilder.CreateSub(memoryNumBytes, numBytes);
		llvm::Value* numBytesWasGreaterThanMemoryNumBytes
			= irBuilder.CreateICmpUGT(memoryNumBytesMinusNumBytes, memoryNumBytes);
		functionContext.emitConditionalTrapIntrinsic(
			irBuilder.CreateOr(numBytesWasGreaterThanMemoryNumBytes,
							   irBuilder.CreateICmpUGT(address, memoryNumBytesMinusNumBytes)),
#if 0
			"memoryOutOfBoundsTrap",
			FunctionType(TypeTuple{},
						 TypeTuple{functionContext.moduleContext.iptrValueType,
								   functionContext.moduleContext.iptrValueType,
								   functionContext.moduleContext.iptrValueType,
								   functionContext.moduleContext.iptrValueType},
						 IR::CallingConvention::intrinsic),
			{address,
			 numBytes,
			 memoryNumBytes,
			 emitLiteralIptr(memoryIndex, functionContext.moduleContext.iptrType)}
#else
			"memoryOutOfBoundsTrapSimple",
			FunctionType({}, {}, IR::CallingConvention::intrinsic),
			{}
#endif
		);
	}
	else if(is32bitMemoryOn64bitHost)
	{
		// For 32-bit addresses on 64-bit targets, the runtime will reserve the full range of
		// addresses that can be generated by this function, so accessing those addresses has
		// well-defined behavior.
	}
	else
	{
		// For all other cases (e.g. 64-bit addresses on 64-bit targets), it's not possible for the
		// runtime to reserve the full range of addresses, so this function must clamp addresses to
		// the guard region.

		llvm::Value* endAddress = ::WAVM::LLVMJIT::wavmCreateLoad(
			irBuilder,
			functionContext.moduleContext.iptrType,
			functionContext.memoryInfos[memoryIndex].endAddressVariable);
		address = irBuilder.CreateSelect(
			irBuilder.CreateICmpULT(address, endAddress), address, endAddress);
	}

	// If the offset is less than the size of the guard region, then add it after bounds checking.
	// This avoids the need to check the addition for overflow, and allows it to be used as the
	// displacement in x86 addresses. Additionally, it allows the LLVM optimizer to reuse the bounds
	// checking code for consecutive loads/stores to the same address.
	auto addressoriginal = address;
	if(offset && offset < Runtime::memoryNumGuardBytes)
	{
		llvm::Constant* offsetConstant
			= emitLiteralIptr(offset, functionContext.moduleContext.iptrType);

		address = irBuilder.CreateAdd(address, offsetConstant);
	}

	if(!istagging && memtagBasePointerVariable)
	{
		::llvm::Value* addressrshift{irBuilder.CreateLShr(addressoriginal, 4)};
		::llvm::Value* tagbaseptrval = ::WAVM::LLVMJIT::wavmCreateLoad(
			irBuilder, functionContext.llvmContext.i8PtrType, memtagBasePointerVariable);
		::llvm::Value* tagbytePointer = ::WAVM::LLVMJIT::wavmCreateInBoundsGEP(
			irBuilder, functionContext.llvmContext.i8Type, tagbaseptrval, addressrshift);
		::llvm::Value* taginmem = ::WAVM::LLVMJIT::wavmCreateLoad(
			irBuilder, functionContext.llvmContext.i8Type, tagbytePointer);
		functionContext.emitConditionalTrapIntrinsic(
			irBuilder.CreateICmpNE(taggedval, taginmem),
			"memoryTagFails",
#if 0
			FunctionType(TypeTuple{},
						 TypeTuple{functionContext.moduleContext.iptrValueType,
						 	functionContext.moduleContext.iptrValueType,
								   IR::ValueType::i32,
								   IR::ValueType::i32},
						 IR::CallingConvention::intrinsic),
			{irBuilder.CreatePtrToInt(tagbaseptrval,irBuilder.getInt64Ty()),addressrshift,irBuilder.CreateZExt(taggedval,irBuilder.getInt32Ty()),irBuilder.CreateZExt(taginmem,irBuilder.getInt32Ty())}
#else
			FunctionType(TypeTuple{}, TypeTuple{}, IR::CallingConvention::intrinsic),
			{}
#endif
		);
	}
	return address;
}

llvm::Value* EmitFunctionContext::coerceAddressToPointer(llvm::Value* boundedAddress,
														 llvm::Type* memoryType,
														 Uptr memoryIndex)
{
	llvm::Value* memoryBasePointer = ::WAVM::LLVMJIT::wavmCreateLoad(
		irBuilder, llvmContext.i8PtrType, memoryInfos[memoryIndex].basePointerVariable);
	llvm::Value* bytePointer = ::WAVM::LLVMJIT::wavmCreateInBoundsGEP(
		irBuilder, llvmContext.i8Type, memoryBasePointer, boundedAddress);

	// Cast the pointer to the appropriate type.
#if LLVM_VERSION_MAJOR > 14
	return bytePointer;
#else
	return irBuilder.CreatePointerCast(bytePointer, memoryType->getPointerTo());
#endif
}

//
// Memory size operators
// These just call out to wavmIntrinsics.growMemory/currentMemory, passing a pointer to the default
// memory for the module.
//

void EmitFunctionContext::memory_grow(MemoryImm imm)
{
	llvm::Value* deltaNumPages = pop();
	ValueVector resultTuple = emitRuntimeIntrinsic(
		"memory.grow",
		FunctionType(TypeTuple(moduleContext.iptrValueType),
					 TypeTuple({moduleContext.iptrValueType, moduleContext.iptrValueType}),
					 IR::CallingConvention::intrinsic),
		{zext(deltaNumPages, moduleContext.iptrType),
		 getMemoryIdFromOffset(irBuilder, moduleContext.memoryOffsets[imm.memoryIndex])});
	WAVM_ASSERT(resultTuple.size() == 1);
	const MemoryType& memoryType = moduleContext.irModule.memories.getType(imm.memoryIndex);
	push(coerceIptrToIndex(memoryType.indexType, resultTuple[0]));
}
void EmitFunctionContext::memory_size(MemoryImm imm)
{
	const MemoryType& memoryType = moduleContext.irModule.memories.getType(imm.memoryIndex);
	push(coerceIptrToIndex(memoryType.indexType, getMemoryNumPages(*this, imm.memoryIndex)));
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
					 TypeTuple({moduleContext.iptrValueType,
								moduleContext.iptrValueType,
								moduleContext.iptrValueType,
								moduleContext.iptrValueType,
								moduleContext.iptrValueType,
								moduleContext.iptrValueType}),
					 IR::CallingConvention::intrinsic),
		{zext(destAddress, moduleContext.iptrType),
		 zext(sourceOffset, moduleContext.iptrType),
		 zext(numBytes, moduleContext.iptrType),
		 moduleContext.instanceId,
		 getMemoryIdFromOffset(irBuilder, moduleContext.memoryOffsets[imm.memoryIndex]),
		 emitLiteral(llvmContext, imm.dataSegmentIndex)});
}

void EmitFunctionContext::data_drop(DataSegmentImm imm)
{
	emitRuntimeIntrinsic(
		"data.drop",
		FunctionType({},
					 TypeTuple({moduleContext.iptrValueType, moduleContext.iptrValueType}),
					 IR::CallingConvention::intrinsic),
		{moduleContext.instanceId, emitLiteralIptr(imm.dataSegmentIndex, moduleContext.iptrType)});
}

void EmitFunctionContext::memory_copy(MemoryCopyImm imm)
{
	llvm::Value* numBytes = pop();
	llvm::Value* sourceAddress = pop();
	llvm::Value* destAddress = pop();

	llvm::Value* sourceBoundedAddress = getOffsetAndBoundedAddress(
		*this, imm.sourceMemoryIndex, sourceAddress, numBytes, 0, BoundsCheckOp::trapOnOutOfBounds);
	llvm::Value* destBoundedAddress = getOffsetAndBoundedAddress(
		*this, imm.destMemoryIndex, destAddress, numBytes, 0, BoundsCheckOp::trapOnOutOfBounds);

	llvm::Value* sourcePointer
		= coerceAddressToPointer(sourceBoundedAddress, llvmContext.i8Type, imm.sourceMemoryIndex);
	llvm::Value* destPointer
		= coerceAddressToPointer(destBoundedAddress, llvmContext.i8Type, imm.destMemoryIndex);

	llvm::Value* numBytesUptr = irBuilder.CreateZExt(numBytes, moduleContext.iptrType);

	// Use the LLVM memmove instruction to do the copy.
#if LLVM_VERSION_MAJOR < 7
	irBuilder.CreateMemMove(destPointer, sourcePointer, numBytesUptr, 1, true);
#else
	irBuilder.CreateMemMove(
		destPointer, LLVM_ALIGNMENT(1), sourcePointer, LLVM_ALIGNMENT(1), numBytesUptr, true);
#endif
}

void EmitFunctionContext::memory_fill(MemoryImm imm)
{
	llvm::Value* numBytes = pop();
	llvm::Value* value = pop();
	llvm::Value* destAddress = pop();

	llvm::Value* destBoundedAddress = getOffsetAndBoundedAddress(
		*this, imm.memoryIndex, destAddress, numBytes, 0, BoundsCheckOp::trapOnOutOfBounds);
	llvm::Value* destPointer
		= coerceAddressToPointer(destBoundedAddress, llvmContext.i8Type, imm.memoryIndex);

	llvm::Value* numBytesUptr = irBuilder.CreateZExt(numBytes, moduleContext.iptrType);

	// Use the LLVM memset instruction to do the fill.
	irBuilder.CreateMemSet(destPointer,
						   irBuilder.CreateTrunc(value, llvmContext.i8Type),
						   numBytesUptr,
						   LLVM_ALIGNMENT(1),
						   true);
}

static inline bool isMemTaggedEnabled(EmitFunctionContext& functionContext, Uptr memoryIndex)
{
	return functionContext.memoryInfos[memoryIndex].memtagBasePointerVariable != nullptr;
}

static inline ::llvm::Value* generateMemRandomTagByte(EmitFunctionContext& functionContext,
													  Uptr memoryIndex)
{
	auto& irBuilder = functionContext.irBuilder;
	auto& meminfo = functionContext.memoryInfos[memoryIndex];
	::llvm::Value* memtagrandombufferptr = ::WAVM::LLVMJIT::wavmCreateLoad(
		irBuilder, functionContext.llvmContext.i8PtrType, meminfo.memtagRandomBufferVariable);
	auto* function = functionContext.function;
	::llvm::BasicBlock* rdtagentryBlock
		= ::llvm::BasicBlock::Create(functionContext.moduleContext.llvmContext, "", function);
	irBuilder.CreateBr(rdtagentryBlock);
	irBuilder.SetInsertPoint(rdtagentryBlock);
	::llvm::Value* arg0 = memtagrandombufferptr;

	::llvm::Value* currptraddr
		= irBuilder.CreateGEP(functionContext.llvmContext.i8PtrType, arg0, {irBuilder.getInt32(1)});
	::llvm::Value* currptr = ::WAVM::LLVMJIT::wavmCreateLoad(
		irBuilder, functionContext.llvmContext.i8PtrType, currptraddr);
	::llvm::Value* endptraddr
		= irBuilder.CreateGEP(functionContext.llvmContext.i8PtrType, arg0, {irBuilder.getInt32(2)});
	::llvm::Value* endptr = ::WAVM::LLVMJIT::wavmCreateLoad(
		irBuilder, functionContext.llvmContext.i8PtrType, endptraddr);
	::llvm::Value* cmpres = irBuilder.CreateICmpEQ(currptr, endptr);
	::llvm::BasicBlock* trueBlock
		= ::llvm::BasicBlock::Create(functionContext.moduleContext.llvmContext, "", function);
	::llvm::BasicBlock* mergeBlock
		= ::llvm::BasicBlock::Create(functionContext.moduleContext.llvmContext, "", function);
	irBuilder.CreateCondBr(cmpres, trueBlock, mergeBlock);
	irBuilder.SetInsertPoint(trueBlock);
	::llvm::Value* begptr
		= ::WAVM::LLVMJIT::wavmCreateLoad(irBuilder, functionContext.llvmContext.i8PtrType, arg0);

	IR::ValueType iptrnativeValueType;
	::llvm::Type* ptrtype;
	if constexpr(sizeof(::std::uint_least64_t) == sizeof(::std::size_t))
	{
		iptrnativeValueType = IR::ValueType::i64;
		ptrtype = irBuilder.getInt64Ty();
	}
	else
	{
		iptrnativeValueType = IR::ValueType::i32;
		ptrtype = irBuilder.getInt32Ty();
	}

	functionContext.emitRuntimeIntrinsic(
		"memoryTagRandomTagRefillFunction",
		FunctionType({}, {iptrnativeValueType}, IR::CallingConvention::intrinsic),
		{irBuilder.CreatePtrToInt(begptr, ptrtype)});

	irBuilder.CreateBr(mergeBlock);
	irBuilder.SetInsertPoint(mergeBlock);
	auto currphiNode = irBuilder.CreatePHI(functionContext.llvmContext.i8PtrType, 2);
	currphiNode->addIncoming(begptr, trueBlock);
	currphiNode->addIncoming(currptr, rdtagentryBlock);
	::llvm::Value* rettag = ::WAVM::LLVMJIT::wavmCreateLoad(
		irBuilder, functionContext.llvmContext.i8Type, currphiNode);
	currptr = irBuilder.CreateGEP(
		functionContext.llvmContext.i8PtrType, currphiNode, {irBuilder.getInt32(1)});
	irBuilder.CreateStore(currptraddr, currptr);
	return rettag;
}

static inline ::llvm::Value* TagMemPointer(EmitFunctionContext& functionContext,
										   Uptr memoryIndex,
										   ::llvm::Value* address,
										   ::llvm::Value* color,
										   bool addressuntagged)
{
	const MemoryType& memoryType
		= functionContext.moduleContext.irModule.memories.getType(memoryIndex);

	llvm::IRBuilder<>& irBuilder = functionContext.irBuilder;

	if(memoryType.indexType == IndexType::i64)
	{
		auto pointertype = address->getType();
		address = irBuilder.CreatePtrToInt(address, functionContext.llvmContext.i64Type);
		color = irBuilder.CreateZExt(color, functionContext.llvmContext.i64Type);
		color = irBuilder.CreateShl(color, 56);
		if(!addressuntagged)
		{
			address = irBuilder.CreateAnd(address, irBuilder.getInt64(0x00FFFFFFFFFFFFFF));
		}
		address = irBuilder.CreateOr(address, color);
		address = irBuilder.CreateIntToPtr(address, pointertype);
	}
	else
	{
		auto pointertype = address->getType();
		address = irBuilder.CreatePtrToInt(address, functionContext.llvmContext.i32Type);
		color = irBuilder.CreateZExt(color, functionContext.llvmContext.i32Type);
		color = irBuilder.CreateShl(color, 30);
		if(!addressuntagged)
		{
			address = irBuilder.CreateAnd(address, irBuilder.getInt32(0x3FFFFFFF));
		}
		address = irBuilder.CreateOr(address, color);
		address = irBuilder.CreateIntToPtr(address, pointertype);
	}
	return address;
}

static inline ::llvm::Value* ComputeMemTagIndex(EmitFunctionContext& functionContext,
												Uptr memoryIndex,
												::llvm::Value*& address)
{
	MemoryType const& memoryType
		= functionContext.moduleContext.irModule.memories.getType(memoryIndex);
	llvm::IRBuilder<>& irBuilder = functionContext.irBuilder;
	if(memoryType.indexType == IndexType::i64)
	{
		address = irBuilder.CreatePtrToInt(address, functionContext.llvmContext.i64Type);
		address = irBuilder.CreateAnd(address, irBuilder.getInt64(0x00FFFFFFFFFFFFFF));
	}
	else
	{
		address = irBuilder.CreatePtrToInt(address, functionContext.llvmContext.i32Type);
		address = irBuilder.CreateAnd(address, irBuilder.getInt32(0x3FFFFFFF));
	}
	auto* memtagBasePointerVariable
		= functionContext.memoryInfos[memoryIndex].memtagBasePointerVariable;
	auto* memtagbase = ::WAVM::LLVMJIT::wavmCreateLoad(
		irBuilder, functionContext.llvmContext.i8PtrType, memtagBasePointerVariable);
	auto* realtagaddress = irBuilder.CreateGEP(
		functionContext.llvmContext.i8Type, memtagbase, {irBuilder.CreateLShr(address, 4)});
	return realtagaddress;
}

static inline ::llvm::Value* StoreTagIntoMem(EmitFunctionContext& functionContext,
											 Uptr memoryIndex,
											 ::llvm::Value* address,
											 ::llvm::Value* taggedbytes,
											 ::llvm::Value* color)
{
	MemoryType const& memoryType
		= functionContext.moduleContext.irModule.memories.getType(memoryIndex);
	llvm::IRBuilder<>& irBuilder = functionContext.irBuilder;
	if(memoryType.indexType == IndexType::i64)
	{
		address = irBuilder.CreatePtrToInt(address, functionContext.llvmContext.i64Type);
		if(color == nullptr)
		{
			color = irBuilder.CreateTrunc(irBuilder.CreateShl(address, 56),
										  functionContext.llvmContext.i8Type);
		}
		address = irBuilder.CreateAnd(address, irBuilder.getInt64(0x00FFFFFFFFFFFFFF));
	}
	else
	{
		address = irBuilder.CreatePtrToInt(address, functionContext.llvmContext.i32Type);
		if(color == nullptr)
		{
			color = irBuilder.CreateTrunc(irBuilder.CreateShl(address, 30),
										  functionContext.llvmContext.i8Type);
		}
		address = irBuilder.CreateAnd(address, irBuilder.getInt32(0x3FFFFFFF));
	}
	auto* memtagBasePointerVariable
		= functionContext.memoryInfos[memoryIndex].memtagBasePointerVariable;
	auto* memtagbase = ::WAVM::LLVMJIT::wavmCreateLoad(
		irBuilder, functionContext.llvmContext.i8PtrType, memtagBasePointerVariable);
	auto* realtagaddress = irBuilder.CreateGEP(
		functionContext.llvmContext.i8Type, memtagbase, {irBuilder.CreateLShr(address, 4)});
	irBuilder.CreateMemSet(
		realtagaddress, color, irBuilder.CreateLShr(taggedbytes, 4), LLVM_ALIGNMENT(1), false);
	return address;
}

void EmitFunctionContext::memory_randomstoretag(NoImm)
{
	::llvm::Value* taggedbytes = pop();
	::llvm::Value* memaddress = pop();
	if(isMemTaggedEnabled(*this, 0))
	{
		auto color = generateMemRandomTagByte(*this, 0);
		memaddress = StoreTagIntoMem(*this, 0, memaddress, taggedbytes, color);
		memaddress = TagMemPointer(*this, 0, memaddress, color, true);
	}
	push(memaddress);
}

void EmitFunctionContext::memory_storetag(NoImm)
{
	::llvm::Value* taggedbytes = pop();
	::llvm::Value* memaddress = pop();
	if(isMemTaggedEnabled(*this, 0))
	{
		StoreTagIntoMem(*this, 0, memaddress, taggedbytes, nullptr);
	}
}

void EmitFunctionContext::memory_randomtag(NoImm)
{
	::llvm::Value* memaddress = pop();
	if(isMemTaggedEnabled(*this, 0))
	{
		auto color = generateMemRandomTagByte(*this, 0);
		memaddress = TagMemPointer(*this, 0, memaddress, color, false);
	}
	push(memaddress);
}

void EmitFunctionContext::memory_subtag(NoImm)
{
	::llvm::Value* ptra = pop();
	::llvm::Value* ptrb = pop();
	llvm::IRBuilder<>& irBuilder = this->irBuilder;

	if(isMemTaggedEnabled(*this, 0))
	{
		const MemoryType& memoryType = this->moduleContext.irModule.memories.getType(0);
		::llvm::Value* mask = nullptr;
		if(memoryType.indexType == IndexType::i64)
		{
			mask = irBuilder.getInt64(0x00FFFFFFFFFFFFFF);
		}
		else { mask = irBuilder.getInt32(0x3FFFFFFF); }
		ptra = irBuilder.CreateAnd(ptra, mask);
		ptrb = irBuilder.CreateAnd(ptrb, mask);
	}
	push(irBuilder.CreateSub(ptra, ptrb));
}
void EmitFunctionContext::memory_copytag(NoImm)
{
	::llvm::Value* memaddress1 = pop();
	::llvm::Value* memaddress2 = pop();
	if(isMemTaggedEnabled(*this, 0))
	{
		llvm::IRBuilder<>& irBuilder = this->irBuilder;
		const MemoryType& memoryType = this->moduleContext.irModule.memories.getType(0);
		if(memoryType.indexType == IndexType::i64)
		{
			memaddress1 = irBuilder.CreateOr(
				irBuilder.CreateAnd(memaddress2, irBuilder.getInt64(0xFF00000000000000)),
				irBuilder.CreateAnd(memaddress1, irBuilder.getInt64(0x00FFFFFFFFFFFFFF)));
		}
		else
		{
			memaddress1 = irBuilder.CreateOr(
				irBuilder.CreateAnd(memaddress2, irBuilder.getInt32(0xC0000000)),
				irBuilder.CreateAnd(memaddress1, irBuilder.getInt32(0x3FFFFFFF)));
		}
	}
	push(memaddress1);
}
void EmitFunctionContext::memory_loadtag(NoImm)
{
	::llvm::Value* memaddress = pop();
	if(isMemTaggedEnabled(*this, 0))
	{
		auto realaddr = ComputeMemTagIndex(*this, 0, memaddress);
		llvm::IRBuilder<>& irBuilder = this->irBuilder;

		const MemoryType& memoryType = this->moduleContext.irModule.memories.getType(0);
		::llvm::Value* color
			= ::WAVM::LLVMJIT::wavmCreateLoad(irBuilder, this->llvmContext.i8Type, realaddr);
		uint32_t shiftval{};
		if(memoryType.indexType == IndexType::i64)
		{
			color = irBuilder.CreateZExt(color, this->llvmContext.i64Type);
			shiftval = 56;
		}
		else
		{
			color = irBuilder.CreateZExt(color, this->llvmContext.i32Type);
			shiftval = 30;
		}
		color = irBuilder.CreateShl(color, irBuilder.getInt32(shiftval));
		memaddress = irBuilder.CreateOr(memaddress, color);
	}
	push(memaddress);
}

//
// Load/store operators
//

#define EMIT_LOAD_OP(destType, name, llvmMemoryType, numBytesLog2, conversionOp)                   \
	void EmitFunctionContext::name(LoadOrStoreImm<numBytesLog2> imm)                               \
	{                                                                                              \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(                                          \
			*this,                                                                                 \
			imm.memoryIndex,                                                                       \
			address,                                                                               \
			llvm::ConstantInt::get(address->getType(), U64(1 << numBytesLog2)),                    \
			imm.offset,                                                                            \
			BoundsCheckOp::clampToGuardRegion);                                                    \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto load = ::WAVM::LLVMJIT::wavmCreateLoad(irBuilder, llvmMemoryType, pointer);           \
		/* Don't trust the alignment hint provided by the WebAssembly code, since the load can't   \
		 * trap if it's wrong. */                                                                  \
		load->setAlignment(LLVM_ALIGNMENT(1));                                                     \
		load->setVolatile(true);                                                                   \
		push(conversionOp(load, destType));                                                        \
	}
#define EMIT_STORE_OP(name, llvmMemoryType, numBytesLog2, conversionOp)                            \
	void EmitFunctionContext::name(LoadOrStoreImm<numBytesLog2> imm)                               \
	{                                                                                              \
		auto value = pop();                                                                        \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(                                          \
			*this,                                                                                 \
			imm.memoryIndex,                                                                       \
			address,                                                                               \
			llvm::ConstantInt::get(address->getType(), U64(1 << numBytesLog2)),                    \
			imm.offset,                                                                            \
			BoundsCheckOp::clampToGuardRegion);                                                    \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto memoryValue = conversionOp(value, llvmMemoryType);                                    \
		auto store = irBuilder.CreateStore(memoryValue, pointer);                                  \
		store->setVolatile(true);                                                                  \
		/* Don't trust the alignment hint provided by the WebAssembly code, since the store can't  \
		 * trap if it's wrong. */                                                                  \
		store->setAlignment(LLVM_ALIGNMENT(1));                                                    \
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

EMIT_STORE_OP(i32_store8, llvmContext.i8Type, 0, trunc)
EMIT_STORE_OP(i64_store8, llvmContext.i8Type, 0, trunc)
EMIT_STORE_OP(i32_store16, llvmContext.i16Type, 1, trunc)
EMIT_STORE_OP(i64_store16, llvmContext.i16Type, 1, trunc)
EMIT_STORE_OP(i32_store, llvmContext.i32Type, 2, trunc)
EMIT_STORE_OP(i64_store32, llvmContext.i32Type, 2, trunc)
EMIT_STORE_OP(i64_store, llvmContext.i64Type, 3, identity)
EMIT_STORE_OP(f32_store, llvmContext.f32Type, 2, identity)
EMIT_STORE_OP(f64_store, llvmContext.f64Type, 3, identity)

EMIT_STORE_OP(v128_store, value->getType(), 4, identity)
EMIT_LOAD_OP(llvmContext.i64x2Type, v128_load, llvmContext.i64x2Type, 4, identity)

EMIT_LOAD_OP(llvmContext.i8x16Type, v128_load8_splat, llvmContext.i8Type, 0, splat<16>)
EMIT_LOAD_OP(llvmContext.i16x8Type, v128_load16_splat, llvmContext.i16Type, 1, splat<8>)
EMIT_LOAD_OP(llvmContext.i32x4Type, v128_load32_splat, llvmContext.i32Type, 2, splat<4>)
EMIT_LOAD_OP(llvmContext.i64x2Type, v128_load64_splat, llvmContext.i64Type, 3, splat<2>)

EMIT_LOAD_OP(llvmContext.i16x8Type, v128_load8x8_s, llvmContext.i8x8Type, 3, sext)
EMIT_LOAD_OP(llvmContext.i16x8Type, v128_load8x8_u, llvmContext.i8x8Type, 3, zext)
EMIT_LOAD_OP(llvmContext.i32x4Type, v128_load16x4_s, llvmContext.i16x4Type, 3, sext)
EMIT_LOAD_OP(llvmContext.i32x4Type, v128_load16x4_u, llvmContext.i16x4Type, 3, zext)
EMIT_LOAD_OP(llvmContext.i64x2Type, v128_load32x2_s, llvmContext.i32x2Type, 3, sext)
EMIT_LOAD_OP(llvmContext.i64x2Type, v128_load32x2_u, llvmContext.i32x2Type, 3, zext)

EMIT_LOAD_OP(llvmContext.i32x4Type,
			 v128_load32_zero,
			 llvmContext.i32Type,
			 2,
			 insertInZeroedVector<4>)
EMIT_LOAD_OP(llvmContext.i64x2Type,
			 v128_load64_zero,
			 llvmContext.i64Type,
			 3,
			 insertInZeroedVector<2>)

static void emitLoadLane(EmitFunctionContext& functionContext,
						 llvm::Type* llvmVectorType,
						 const BaseLoadOrStoreImm& loadOrStoreImm,
						 Uptr laneIndex,
						 Uptr numBytesLog2)
{
	llvm::Value* vector = functionContext.pop();
	vector = functionContext.irBuilder.CreateBitCast(vector, llvmVectorType);

	llvm::Value* address = functionContext.pop();
	llvm::Value* boundedAddress = getOffsetAndBoundedAddress(
		functionContext,
		loadOrStoreImm.memoryIndex,
		address,
		llvm::ConstantInt::get(address->getType(), U64(1) << numBytesLog2),
		loadOrStoreImm.offset,
		BoundsCheckOp::clampToGuardRegion);
	llvm::Value* pointer = functionContext.coerceAddressToPointer(
		boundedAddress, llvmVectorType->getScalarType(), loadOrStoreImm.memoryIndex);
	llvm::LoadInst* load = ::WAVM::LLVMJIT::wavmCreateLoad(
		functionContext.irBuilder, llvmVectorType->getScalarType(), pointer);
	// Don't trust the alignment hint provided by the WebAssembly code, since the load can't trap if
	// it's wrong.
	load->setAlignment(LLVM_ALIGNMENT(1));
	load->setVolatile(true);

	vector = functionContext.irBuilder.CreateInsertElement(vector, load, laneIndex);
	functionContext.push(vector);
}

static void emitStoreLane(EmitFunctionContext& functionContext,
						  llvm::Type* llvmVectorType,
						  const BaseLoadOrStoreImm& loadOrStoreImm,
						  Uptr laneIndex,
						  Uptr numBytesLog2)
{
	llvm::Value* vector = functionContext.pop();
	vector = functionContext.irBuilder.CreateBitCast(vector, llvmVectorType);

	auto lane = functionContext.irBuilder.CreateExtractElement(vector, laneIndex);

	llvm::Value* address = functionContext.pop();
	llvm::Value* boundedAddress = getOffsetAndBoundedAddress(
		functionContext,
		loadOrStoreImm.memoryIndex,
		address,
		llvm::ConstantInt::get(address->getType(), U64(1) << numBytesLog2),
		loadOrStoreImm.offset,
		BoundsCheckOp::clampToGuardRegion);
	llvm::Value* pointer = functionContext.coerceAddressToPointer(
		boundedAddress, lane->getType(), loadOrStoreImm.memoryIndex);
	llvm::StoreInst* store = functionContext.irBuilder.CreateStore(lane, pointer);
	// Don't trust the alignment hint provided by the WebAssembly code, since the load can't trap if
	// it's wrong.
	store->setAlignment(LLVM_ALIGNMENT(1));
	store->setVolatile(true);
}

#define EMIT_LOAD_LANE_OP(name, llvmVectorType, naturalAlignmentLog2, numLanes)                    \
	void EmitFunctionContext::name(LoadOrStoreLaneImm<naturalAlignmentLog2, numLanes> imm)         \
	{                                                                                              \
		emitLoadLane(*this, llvmVectorType, imm, imm.laneIndex, U64(1) << naturalAlignmentLog2);   \
	}
#define EMIT_STORE_LANE_OP(name, llvmVectorType, naturalAlignmentLog2, numLanes)                   \
	void EmitFunctionContext::name(LoadOrStoreLaneImm<naturalAlignmentLog2, numLanes> imm)         \
	{                                                                                              \
		emitStoreLane(*this, llvmVectorType, imm, imm.laneIndex, U64(1) << naturalAlignmentLog2);  \
	}
EMIT_LOAD_LANE_OP(v128_load8_lane, llvmContext.i8x16Type, 0, 16)
EMIT_LOAD_LANE_OP(v128_load16_lane, llvmContext.i16x8Type, 1, 8)
EMIT_LOAD_LANE_OP(v128_load32_lane, llvmContext.i32x4Type, 2, 4)
EMIT_LOAD_LANE_OP(v128_load64_lane, llvmContext.i64x2Type, 3, 2)
EMIT_STORE_LANE_OP(v128_store8_lane, llvmContext.i8x16Type, 0, 16)
EMIT_STORE_LANE_OP(v128_store16_lane, llvmContext.i16x8Type, 1, 8)
EMIT_STORE_LANE_OP(v128_store32_lane, llvmContext.i32x4Type, 2, 4)
EMIT_STORE_LANE_OP(v128_store64_lane, llvmContext.i64x2Type, 3, 2)

static void emitLoadInterleaved(EmitFunctionContext& functionContext,
								llvm::Type* llvmValueType,
								llvm::Intrinsic::ID aarch64IntrinsicID,
								const BaseLoadOrStoreImm& imm,
								U32 numVectors,
								U32 numLanes,
								U64 numBytes)
{
	static constexpr U32 maxVectors = 4;
	static constexpr U32 maxLanes = 16;
	WAVM_ASSERT(numVectors <= maxVectors);
	WAVM_ASSERT(numLanes <= maxLanes);

	auto address = functionContext.pop();
	auto boundedAddress
		= getOffsetAndBoundedAddress(functionContext,
									 imm.memoryIndex,
									 address,
									 llvm::ConstantInt::get(address->getType(), numBytes),
									 imm.offset,
									 BoundsCheckOp::clampToGuardRegion);
	auto pointer
		= functionContext.coerceAddressToPointer(boundedAddress, llvmValueType, imm.memoryIndex);
	if(functionContext.moduleContext.targetArch == llvm::Triple::aarch64)
	{
		auto results = functionContext.callLLVMIntrinsic(
			{llvmValueType, llvmValueType->getPointerTo()}, aarch64IntrinsicID, {pointer});
		for(U32 vectorIndex = 0; vectorIndex < numVectors; ++vectorIndex)
		{
			functionContext.push(
				functionContext.irBuilder.CreateExtractValue(results, vectorIndex));
		}
	}
	else
	{
		llvm::Value* loads[maxVectors];
		for(U32 vectorIndex = 0; vectorIndex < numVectors; ++vectorIndex)
		{
			auto load = ::WAVM::LLVMJIT::wavmCreateLoad(
				functionContext.irBuilder,
				llvmValueType,
				::WAVM::LLVMJIT::wavmCreateInBoundsGEP(
					functionContext.irBuilder,
					functionContext.llvmContext.i8Type,
					pointer,
					{emitLiteral(functionContext.llvmContext, U32(vectorIndex))}));
			/* Don't trust the alignment hint provided by the WebAssembly code, since the load
			 * can't trap if it's wrong. */
			load->setAlignment(LLVM_ALIGNMENT(1));
			load->setVolatile(true);
			loads[vectorIndex] = load;
		}
		for(U32 vectorIndex = 0; vectorIndex < numVectors; ++vectorIndex)
		{
			llvm::Value* deinterleavedVector = llvm::UndefValue::get(llvmValueType);
			for(U32 laneIndex = 0; laneIndex < numLanes; ++laneIndex)
			{
				const Uptr interleavedElementIndex = laneIndex * numVectors + vectorIndex;
				deinterleavedVector = functionContext.irBuilder.CreateInsertElement(
					deinterleavedVector,
					functionContext.irBuilder.CreateExtractElement(
						loads[interleavedElementIndex / numLanes],
						interleavedElementIndex % numLanes),
					laneIndex);
			}
			functionContext.push(deinterleavedVector);
		}
	}
}

static void emitStoreInterleaved(EmitFunctionContext& functionContext,
								 llvm::Type* llvmValueType,
								 llvm::Intrinsic::ID aarch64IntrinsicID,
								 const IR::BaseLoadOrStoreImm& imm,
								 U32 numVectors,
								 U32 numLanes,
								 U64 numBytes)
{
	static constexpr U32 maxVectors = 4;
	WAVM_ASSERT(numVectors <= 4);

	llvm::Value* values[maxVectors];
	for(U32 vectorIndex = 0; vectorIndex < numVectors; ++vectorIndex)
	{
		values[numVectors - vectorIndex - 1]
			= functionContext.irBuilder.CreateBitCast(functionContext.pop(), llvmValueType);
	}
	auto address = functionContext.pop();
	auto boundedAddress
		= getOffsetAndBoundedAddress(functionContext,
									 imm.memoryIndex,
									 address,
									 llvm::ConstantInt::get(address->getType(), numBytes),
									 imm.offset,
									 BoundsCheckOp::clampToGuardRegion);
	auto pointer
		= functionContext.coerceAddressToPointer(boundedAddress, llvmValueType, imm.memoryIndex);
	if(functionContext.moduleContext.targetArch == llvm::Triple::aarch64)
	{
		llvm::Value* args[maxVectors + 1];
		for(U32 vectorIndex = 0; vectorIndex < numVectors; ++vectorIndex)
		{
			args[vectorIndex] = values[vectorIndex];
			args[numVectors] = pointer;
		}
		functionContext.callLLVMIntrinsic({llvmValueType, llvmValueType->getPointerTo()},
										  aarch64IntrinsicID,
										  llvm::ArrayRef<llvm::Value*>(args, numVectors + 1));
	}
	else
	{
		for(U32 vectorIndex = 0; vectorIndex < numVectors; ++vectorIndex)
		{
			llvm::Value* interleavedVector = llvm::UndefValue::get(llvmValueType);
			for(U32 laneIndex = 0; laneIndex < numLanes; ++laneIndex)
			{
				const Uptr interleavedElementIndex = vectorIndex * numLanes + laneIndex;
				const Uptr deinterleavedVectorIndex = interleavedElementIndex % numVectors;
				const Uptr deinterleavedLaneIndex = interleavedElementIndex / numVectors;
				interleavedVector = functionContext.irBuilder.CreateInsertElement(
					interleavedVector,
					functionContext.irBuilder.CreateExtractElement(values[deinterleavedVectorIndex],
																   deinterleavedLaneIndex),
					laneIndex);
			}
			auto store = functionContext.irBuilder.CreateStore(
				interleavedVector,
				::WAVM::LLVMJIT::wavmCreateInBoundsGEP(
					functionContext.irBuilder,
					functionContext.llvmContext.i8Type,
					pointer,
					{emitLiteral(functionContext.llvmContext, U32(vectorIndex))}));
			store->setVolatile(true);
			store->setAlignment(LLVM_ALIGNMENT(1));
		}
	}
}

#define EMIT_LOAD_INTERLEAVED_OP(name, llvmValueType, naturalAlignmentLog2, numVectors, numLanes)  \
	void EmitFunctionContext::name(LoadOrStoreImm<naturalAlignmentLog2> imm)                       \
	{                                                                                              \
		emitLoadInterleaved(*this,                                                                 \
							llvmValueType,                                                         \
							llvm::Intrinsic::aarch64_neon_ld##numVectors,                          \
							imm,                                                                   \
							numVectors,                                                            \
							numLanes,                                                              \
							U64(1 << naturalAlignmentLog2) * numVectors);                          \
	}

#define EMIT_STORE_INTERLEAVED_OP(name, llvmValueType, naturalAlignmentLog2, numVectors, numLanes) \
	void EmitFunctionContext::name(LoadOrStoreImm<naturalAlignmentLog2> imm)                       \
	{                                                                                              \
		emitStoreInterleaved(*this,                                                                \
							 llvmValueType,                                                        \
							 llvm::Intrinsic::aarch64_neon_st##numVectors,                         \
							 imm,                                                                  \
							 numVectors,                                                           \
							 numLanes,                                                             \
							 U64(1 << naturalAlignmentLog2) * numVectors);                         \
	}

EMIT_LOAD_INTERLEAVED_OP(v8x16_load_interleaved_2, llvmContext.i8x16Type, 4, 2, 16)
EMIT_LOAD_INTERLEAVED_OP(v8x16_load_interleaved_3, llvmContext.i8x16Type, 4, 3, 16)
EMIT_LOAD_INTERLEAVED_OP(v8x16_load_interleaved_4, llvmContext.i8x16Type, 4, 4, 16)
EMIT_LOAD_INTERLEAVED_OP(v16x8_load_interleaved_2, llvmContext.i16x8Type, 4, 2, 8)
EMIT_LOAD_INTERLEAVED_OP(v16x8_load_interleaved_3, llvmContext.i16x8Type, 4, 3, 8)
EMIT_LOAD_INTERLEAVED_OP(v16x8_load_interleaved_4, llvmContext.i16x8Type, 4, 4, 8)
EMIT_LOAD_INTERLEAVED_OP(v32x4_load_interleaved_2, llvmContext.i32x4Type, 4, 2, 4)
EMIT_LOAD_INTERLEAVED_OP(v32x4_load_interleaved_3, llvmContext.i32x4Type, 4, 3, 4)
EMIT_LOAD_INTERLEAVED_OP(v32x4_load_interleaved_4, llvmContext.i32x4Type, 4, 4, 4)
EMIT_LOAD_INTERLEAVED_OP(v64x2_load_interleaved_2, llvmContext.i64x2Type, 4, 2, 2)
EMIT_LOAD_INTERLEAVED_OP(v64x2_load_interleaved_3, llvmContext.i64x2Type, 4, 3, 2)
EMIT_LOAD_INTERLEAVED_OP(v64x2_load_interleaved_4, llvmContext.i64x2Type, 4, 4, 2)

EMIT_STORE_INTERLEAVED_OP(v8x16_store_interleaved_2, llvmContext.i8x16Type, 4, 2, 16)
EMIT_STORE_INTERLEAVED_OP(v8x16_store_interleaved_3, llvmContext.i8x16Type, 4, 3, 16)
EMIT_STORE_INTERLEAVED_OP(v8x16_store_interleaved_4, llvmContext.i8x16Type, 4, 4, 16)
EMIT_STORE_INTERLEAVED_OP(v16x8_store_interleaved_2, llvmContext.i16x8Type, 4, 2, 8)
EMIT_STORE_INTERLEAVED_OP(v16x8_store_interleaved_3, llvmContext.i16x8Type, 4, 3, 8)
EMIT_STORE_INTERLEAVED_OP(v16x8_store_interleaved_4, llvmContext.i16x8Type, 4, 4, 8)
EMIT_STORE_INTERLEAVED_OP(v32x4_store_interleaved_2, llvmContext.i32x4Type, 4, 2, 4)
EMIT_STORE_INTERLEAVED_OP(v32x4_store_interleaved_3, llvmContext.i32x4Type, 4, 3, 4)
EMIT_STORE_INTERLEAVED_OP(v32x4_store_interleaved_4, llvmContext.i32x4Type, 4, 4, 4)
EMIT_STORE_INTERLEAVED_OP(v64x2_store_interleaved_2, llvmContext.i64x2Type, 4, 2, 2)
EMIT_STORE_INTERLEAVED_OP(v64x2_store_interleaved_3, llvmContext.i64x2Type, 4, 3, 2)
EMIT_STORE_INTERLEAVED_OP(v64x2_store_interleaved_4, llvmContext.i64x2Type, 4, 4, 2)

void EmitFunctionContext::trapIfMisalignedAtomic(llvm::Value* address, U32 alignmentLog2)
{
	if(alignmentLog2 > 0)
	{
		emitConditionalTrapIntrinsic(
			irBuilder.CreateICmpNE(
				llvmContext.typedZeroConstants[(Uptr)ValueType::i64],
				irBuilder.CreateAnd(
					address, emitLiteralIptr((U64(1) << alignmentLog2) - 1, address->getType()))),
			"misalignedAtomicTrap",
			FunctionType(TypeTuple{}, TypeTuple{ValueType::i64}, IR::CallingConvention::intrinsic),
			{address});
	}
}

void EmitFunctionContext::memory_atomic_notify(AtomicLoadOrStoreImm<2> imm)
{
	llvm::Value* numWaiters = pop();
	llvm::Value* address = pop();
	llvm::Value* boundedAddress
		= getOffsetAndBoundedAddress(*this,
									 imm.memoryIndex,
									 address,
									 llvm::ConstantInt::get(address->getType(), U64(4)),
									 imm.offset,
									 BoundsCheckOp::clampToGuardRegion);
	trapIfMisalignedAtomic(boundedAddress, imm.alignmentLog2);
	push(emitRuntimeIntrinsic(
		"memory.atomic.notify",
		FunctionType(
			TypeTuple{ValueType::i32},
			TypeTuple{moduleContext.iptrValueType, ValueType::i32, moduleContext.iptrValueType},
			IR::CallingConvention::intrinsic),
		{boundedAddress,
		 numWaiters,
		 getMemoryIdFromOffset(irBuilder, moduleContext.memoryOffsets[imm.memoryIndex])})[0]);
}
void EmitFunctionContext::memory_atomic_wait32(AtomicLoadOrStoreImm<2> imm)
{
	llvm::Value* timeout = pop();
	llvm::Value* expectedValue = pop();
	llvm::Value* address = pop();
	llvm::Value* boundedAddress
		= getOffsetAndBoundedAddress(*this,
									 imm.memoryIndex,
									 address,
									 llvm::ConstantInt::get(address->getType(), U64(4)),
									 imm.offset,
									 BoundsCheckOp::clampToGuardRegion);
	trapIfMisalignedAtomic(boundedAddress, imm.alignmentLog2);
	push(emitRuntimeIntrinsic(
		"memory.atomic.wait32",
		FunctionType(TypeTuple{ValueType::i32},
					 TypeTuple{moduleContext.iptrValueType,
							   ValueType::i32,
							   ValueType::i64,
							   moduleContext.iptrValueType},
					 IR::CallingConvention::intrinsic),
		{boundedAddress,
		 expectedValue,
		 timeout,
		 getMemoryIdFromOffset(irBuilder, moduleContext.memoryOffsets[imm.memoryIndex])})[0]);
}
void EmitFunctionContext::memory_atomic_wait64(AtomicLoadOrStoreImm<3> imm)
{
	llvm::Value* timeout = pop();
	llvm::Value* expectedValue = pop();
	llvm::Value* address = pop();
	llvm::Value* boundedAddress
		= getOffsetAndBoundedAddress(*this,
									 imm.memoryIndex,
									 address,
									 llvm::ConstantInt::get(address->getType(), U64(8)),
									 imm.offset,
									 BoundsCheckOp::clampToGuardRegion);
	trapIfMisalignedAtomic(boundedAddress, imm.alignmentLog2);
	push(emitRuntimeIntrinsic(
		"memory.atomic.wait64",
		FunctionType(TypeTuple{ValueType::i32},
					 TypeTuple{moduleContext.iptrValueType,
							   ValueType::i64,
							   ValueType::i64,
							   moduleContext.iptrValueType},
					 IR::CallingConvention::intrinsic),
		{boundedAddress,
		 expectedValue,
		 timeout,
		 getMemoryIdFromOffset(irBuilder, moduleContext.memoryOffsets[imm.memoryIndex])})[0]);
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

#define EMIT_ATOMIC_LOAD_OP(valueTypeId, name, llvmMemoryType, numBytesLog2, memToValue)           \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<numBytesLog2> imm)         \
	{                                                                                              \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(                                          \
			*this,                                                                                 \
			imm.memoryIndex,                                                                       \
			address,                                                                               \
			llvm::ConstantInt::get(address->getType(), U64(1 << numBytesLog2)),                    \
			imm.offset,                                                                            \
			BoundsCheckOp::clampToGuardRegion);                                                    \
		trapIfMisalignedAtomic(boundedAddress, numBytesLog2);                                      \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto load = ::WAVM::LLVMJIT::wavmCreateLoad(irBuilder, address->getType(), pointer);       \
		load->setAlignment(LLVM_ALIGNMENT(U64(1) << imm.alignmentLog2));                           \
		load->setVolatile(true);                                                                   \
		load->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);                             \
		push(memToValue(load, asLLVMType(llvmContext, ValueType::valueTypeId)));                   \
	}
#define EMIT_ATOMIC_STORE_OP(valueTypeId, name, llvmMemoryType, numBytesLog2, valueToMem)          \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<numBytesLog2> imm)         \
	{                                                                                              \
		auto value = pop();                                                                        \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(                                          \
			*this,                                                                                 \
			imm.memoryIndex,                                                                       \
			address,                                                                               \
			llvm::ConstantInt::get(address->getType(), U64(1 << numBytesLog2)),                    \
			imm.offset,                                                                            \
			BoundsCheckOp::clampToGuardRegion);                                                    \
		trapIfMisalignedAtomic(boundedAddress, numBytesLog2);                                      \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto memoryValue = valueToMem(value, llvmMemoryType);                                      \
		auto store = irBuilder.CreateStore(memoryValue, pointer);                                  \
		store->setVolatile(true);                                                                  \
		store->setAlignment(LLVM_ALIGNMENT(U64(1) << imm.alignmentLog2));                          \
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
	valueTypeId, name, llvmMemoryType, numBytesLog2, memToValue, valueToMem)                       \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<numBytesLog2> imm)         \
	{                                                                                              \
		auto replacementValue = valueToMem(pop(), llvmMemoryType);                                 \
		auto expectedValue = valueToMem(pop(), llvmMemoryType);                                    \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(                                          \
			*this,                                                                                 \
			imm.memoryIndex,                                                                       \
			address,                                                                               \
			llvm::ConstantInt::get(address->getType(), U64(1 << numBytesLog2)),                    \
			imm.offset,                                                                            \
			BoundsCheckOp::clampToGuardRegion);                                                    \
		trapIfMisalignedAtomic(boundedAddress, numBytesLog2);                                      \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto atomicCmpXchg = ::WAVM::LLVMJIT::wavmCreateAtomicCmpXchg(                             \
			irBuilder,                                                                             \
			pointer,                                                                               \
			expectedValue,                                                                         \
			replacementValue,                                                                      \
			llvm::AtomicOrdering::SequentiallyConsistent,                                          \
			llvm::AtomicOrdering::SequentiallyConsistent);                                         \
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
	valueTypeId, name, rmwOpId, llvmMemoryType, numBytesLog2, memToValue, valueToMem)              \
	void EmitFunctionContext::valueTypeId##_##name(AtomicLoadOrStoreImm<numBytesLog2> imm)         \
	{                                                                                              \
		auto value = valueToMem(pop(), llvmMemoryType);                                            \
		auto address = pop();                                                                      \
		auto boundedAddress = getOffsetAndBoundedAddress(                                          \
			*this,                                                                                 \
			imm.memoryIndex,                                                                       \
			address,                                                                               \
			llvm::ConstantInt::get(address->getType(), U64(1 << numBytesLog2)),                    \
			imm.offset,                                                                            \
			BoundsCheckOp::clampToGuardRegion);                                                    \
		trapIfMisalignedAtomic(boundedAddress, numBytesLog2);                                      \
		auto pointer = coerceAddressToPointer(boundedAddress, llvmMemoryType, imm.memoryIndex);    \
		auto atomicRMW                                                                             \
			= ::WAVM::LLVMJIT::wavmCreateAtomicRMW(irBuilder,                                      \
												   llvm::AtomicRMWInst::BinOp::rmwOpId,            \
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
