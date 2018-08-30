#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Lock.h"
#include "Inline/Timing.h"
#include "LLVMEmitContext.h"
#include "LLVMJIT.h"
#include "Logging/Logging.h"
#include "RuntimePrivate.h"

#include "LLVMPreInclude.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/ObjectMemoryBuffer.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolSize.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

#include "LLVMPostInclude.h"

using namespace IR;
using namespace LLVMJIT;
using namespace Runtime;

// A map from function types to JIT symbols for cached invoke thunks (C++ -> WASM)
static HashMap<FunctionType, struct JITSymbol*> invokeThunkTypeToSymbolMap;

struct IntrinsicThunkKey
{
	void* intrinsicFunction;
	Uptr defaultMemoryId;
	Uptr defaultTableId;
};

template<> struct Hash<IntrinsicThunkKey>
{
	Uptr operator()(const IntrinsicThunkKey& key, Uptr seed = 0) const
	{
		Uptr result = seed;
		result      = XXH64_fixed(reinterpret_cast<Uptr>(key.intrinsicFunction), result);
		result      = XXH64_fixed(key.defaultMemoryId, result);
		result      = XXH64_fixed(key.defaultTableId, result);
		return result;
	}
};

inline bool operator==(const IntrinsicThunkKey& a, const IntrinsicThunkKey& b)
{
	return a.intrinsicFunction == b.intrinsicFunction && a.defaultMemoryId == b.defaultMemoryId
		   && a.defaultTableId == b.defaultTableId;
}

// A map from function types to JIT symbols for cached native thunks (WASM -> C++)
static HashMap<IntrinsicThunkKey, struct JITSymbol*> intrinsicFunctionToThunkSymbolMap;

// The JIT compilation unit for a single invoke thunk.
struct JITThunkUnit : JITUnit
{
	FunctionType functionType;

	JITSymbol* symbol;

	JITThunkUnit(FunctionType inFunctionType) : functionType(inFunctionType), symbol(nullptr) {}

	virtual JITSymbol* notifySymbolLoaded(const char* name,
										  Uptr baseAddress,
										  Uptr numBytes,
										  std::map<U32, U32>&& offsetToOpIndexMap) override
	{
#if(defined(_WIN32) && !defined(_WIN64))
		wavmAssert(!strcmp(name, "_thunk"));
#else
		wavmAssert(!strcmp(name, "thunk"));
#endif
		wavmAssert(!symbol);
		symbol = new JITSymbol(functionType, baseAddress, numBytes, std::move(offsetToOpIndexMap));
		return symbol;
	}
};

InvokeFunctionPointer LLVMJIT::getInvokeThunk(FunctionType functionType,
											  CallingConvention callingConvention)
{
	Lock<Platform::Mutex> llvmLock(llvmMutex);

	initLLVM();

	// Reuse cached invoke thunks for the same function type.
	JITSymbol*& invokeThunkSymbol = invokeThunkTypeToSymbolMap.getOrAdd(functionType, nullptr);
	if(invokeThunkSymbol)
	{ return reinterpret_cast<InvokeFunctionPointer>(invokeThunkSymbol->baseAddress); }

	llvm::Module llvmModule("", *llvmContext);
	auto llvmFunctionType = llvm::FunctionType::get(
		llvmI8PtrType,
		{asLLVMType(functionType, callingConvention)->getPointerTo(), llvmI8PtrType},
		false);
	auto llvmFunction = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	llvm::Value* functionPointer = &*(llvmFunction->args().begin() + 0);
	llvm::Value* contextPointer  = &*(llvmFunction->args().begin() + 1);

	EmitContext emitContext(nullptr, nullptr);
	emitContext.irBuilder.SetInsertPoint(
		llvm::BasicBlock::Create(*llvmContext, "entry", llvmFunction));

	emitContext.initContextVariables(contextPointer);

	// Load the function's arguments from an array of 64-bit values at an address provided by the
	// caller.
	std::vector<llvm::Value*> arguments;
	Uptr argDataOffset = 0;
	for(ValueType parameterType : functionType.params())
	{
		if(parameterType == ValueType::v128)
		{
			// Use 16-byte alignment for V128 arguments.
			argDataOffset = (argDataOffset + 15) & ~15;
		}

		arguments.push_back(emitContext.loadFromUntypedPointer(
			emitContext.irBuilder.CreateInBoundsGEP(
				contextPointer,
				{emitLiteral(argDataOffset + offsetof(ContextRuntimeData, thunkArgAndReturnData))}),
			asLLVMType(parameterType)));

		argDataOffset += parameterType == ValueType::v128 ? 16 : 8;
	}

	// Call the function.
	ValueVector results
		= emitContext.emitCallOrInvoke(functionPointer, arguments, functionType, callingConvention);

	// If the function has a return value, write it to the context invoke return memory.
	wavmAssert(results.size() == functionType.results().size());
	auto newContextPointer = emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable);
	Uptr resultOffset      = 0;
	for(Uptr resultIndex = 0; resultIndex < results.size(); ++resultIndex)
	{
		const ValueType resultType = functionType.results()[resultIndex];
		const U8 resultNumBytes    = getTypeByteWidth(resultType);

		resultOffset = (resultOffset + resultNumBytes - 1) & -I8(resultNumBytes);
		wavmAssert(resultOffset < maxThunkArgAndReturnBytes);

		emitContext.irBuilder.CreateStore(results[resultIndex],
										  emitContext.irBuilder.CreatePointerCast(
											  emitContext.irBuilder.CreateInBoundsGEP(
												  newContextPointer, {emitLiteral(resultOffset)}),
											  asLLVMType(resultType)->getPointerTo()));

		resultOffset += resultNumBytes;
	}

	emitContext.irBuilder.CreateRet(
		emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable));

	// Compile the LLVM IR to object code.
	std::vector<U8> objectBytes = compileLLVMModule(std::move(llvmModule), false);

	// Load the object code.
	auto jitUnit = new JITThunkUnit(functionType);
	jitUnit->load(objectBytes, {}, false);

	wavmAssert(jitUnit->symbol);
	invokeThunkSymbol = jitUnit->symbol;

	return reinterpret_cast<InvokeFunctionPointer>(invokeThunkSymbol->baseAddress);
}

void* LLVMJIT::getIntrinsicThunk(void* nativeFunction,
								 FunctionType functionType,
								 CallingConvention callingConvention,
								 MemoryInstance* defaultMemory,
								 TableInstance* defaultTable)
{
	wavmAssert(callingConvention == CallingConvention::intrinsic
			   || callingConvention == CallingConvention::intrinsicWithContextSwitch
			   || callingConvention == CallingConvention::intrinsicWithMemAndTable);

	// Lock<Platform::Mutex> llvmLock(llvmMutex);

	initLLVM();

	// Reuse cached intrinsic thunks for the same function type.
	const IntrinsicThunkKey key{
		nativeFunction,
		callingConvention == CallingConvention::intrinsicWithMemAndTable && defaultMemory
			? defaultMemory->id
			: UINT32_MAX,
		callingConvention == CallingConvention::intrinsicWithMemAndTable && defaultTable
			? defaultTable->id
			: UINT32_MAX};
	JITSymbol*& intrinsicThunkSymbol = intrinsicFunctionToThunkSymbolMap.getOrAdd(key, nullptr);
	if(intrinsicThunkSymbol) { return reinterpret_cast<void*>(intrinsicThunkSymbol->baseAddress); }

	// Create a LLVM module containing a single function with the same signature as the native
	// function, but with the WASM calling convention.
	llvm::Module llvmModule("", *llvmContext);
	auto llvmFunctionType = asLLVMType(functionType, CallingConvention::wasm);
	auto llvmFunction     = llvm::Function::Create(
        llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	llvmFunction->setCallingConv(asLLVMCallingConv(callingConvention));

	llvm::Constant* defaultMemoryOffset = nullptr;
	if(defaultMemory)
	{
		defaultMemoryOffset = emitLiteral(offsetof(CompartmentRuntimeData, memoryBases)
										  + sizeof(void*) * defaultMemory->id);
	}

	llvm::Constant* defaultTableOffset = nullptr;
	if(defaultTable)
	{
		defaultTableOffset = emitLiteral(offsetof(CompartmentRuntimeData, tableBases)
										 + sizeof(void*) * defaultTable->id);
	}

	EmitContext emitContext(defaultMemoryOffset, defaultTableOffset);
	emitContext.irBuilder.SetInsertPoint(
		llvm::BasicBlock::Create(*llvmContext, "entry", llvmFunction));

	emitContext.initContextVariables(&*llvmFunction->args().begin());

	llvm::SmallVector<llvm::Value*, 8> args;
	for(auto argIt = llvmFunction->args().begin() + 1; argIt != llvmFunction->args().end(); ++argIt)
	{ args.push_back(&*argIt); }

	llvm::Type* llvmNativeFunctionType
		= asLLVMType(functionType, callingConvention)->getPointerTo();
	llvm::Value* llvmNativeFunction = emitLiteralPointer(nativeFunction, llvmNativeFunctionType);
	ValueVector results
		= emitContext.emitCallOrInvoke(llvmNativeFunction, args, functionType, callingConvention);

	// Emit the function return.
	emitContext.emitReturn(functionType.results(), results);

	// Compile the LLVM IR to object code.
	std::vector<U8> objectBytes = compileLLVMModule(std::move(llvmModule), false);

	// Load the object code.
	auto jitUnit = new JITThunkUnit(functionType);
	jitUnit->load(objectBytes, {}, false);

	wavmAssert(jitUnit->symbol);
	intrinsicThunkSymbol = jitUnit->symbol;

	return reinterpret_cast<void*>(intrinsicThunkSymbol->baseAddress);
}
