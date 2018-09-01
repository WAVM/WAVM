#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Lock.h"
#include "Inline/Timing.h"
#include "LLVMEmitContext.h"
#include "LLVMJIT/LLVMJIT.h"
#include "LLVMJITPrivate.h"
#include "Logging/Logging.h"
#include "Runtime/Runtime.h"

#include "LLVMPreInclude.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include "LLVMPostInclude.h"

using namespace IR;
using namespace LLVMJIT;
using namespace Runtime;

// A map from function types to JIT symbols for cached invoke thunks (C++ -> WASM)
static HashMap<FunctionType, struct JITFunction*> invokeThunkTypeToFunctionMap;

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
		result = XXH64_fixed(reinterpret_cast<Uptr>(key.intrinsicFunction), result);
		result = XXH64_fixed(key.defaultMemoryId, result);
		result = XXH64_fixed(key.defaultTableId, result);
		return result;
	}
};

inline bool operator==(const IntrinsicThunkKey& a, const IntrinsicThunkKey& b)
{
	return a.intrinsicFunction == b.intrinsicFunction && a.defaultMemoryId == b.defaultMemoryId
		   && a.defaultTableId == b.defaultTableId;
}

// A map from function types to JIT symbols for cached native thunks (WASM -> C++)
static HashMap<IntrinsicThunkKey, struct JITFunction*> intrinsicFunctionToThunkFunctionMap;

InvokeThunkPointer LLVMJIT::getInvokeThunk(FunctionType functionType,
										   CallingConvention callingConvention)
{
	LLVMContext llvmContext;

	// Reuse cached invoke thunks for the same function type.
	JITFunction*& invokeThunkFunction
		= invokeThunkTypeToFunctionMap.getOrAdd(functionType, nullptr);
	if(invokeThunkFunction)
	{ return reinterpret_cast<InvokeThunkPointer>(invokeThunkFunction->baseAddress); }

	llvm::Module llvmModule("", llvmContext);
	auto llvmFunctionType = llvm::FunctionType::get(
		llvmContext.i8PtrType,
		{asLLVMType(llvmContext, functionType, callingConvention)->getPointerTo(),
		 llvmContext.i8PtrType},
		false);
	auto function = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	llvm::Value* functionPointer = &*(function->args().begin() + 0);
	llvm::Value* contextPointer = &*(function->args().begin() + 1);

	EmitContext emitContext(llvmContext, nullptr, nullptr);
	emitContext.irBuilder.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "entry", function));

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
				{emitLiteral(llvmContext,
							 argDataOffset + offsetof(ContextRuntimeData, thunkArgAndReturnData))}),
			asLLVMType(llvmContext, parameterType)));

		argDataOffset += parameterType == ValueType::v128 ? 16 : 8;
	}

	// Call the function.
	ValueVector results
		= emitContext.emitCallOrInvoke(functionPointer, arguments, functionType, callingConvention);

	// If the function has a return value, write it to the context invoke return memory.
	wavmAssert(results.size() == functionType.results().size());
	auto newContextPointer = emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable);
	Uptr resultOffset = 0;
	for(Uptr resultIndex = 0; resultIndex < results.size(); ++resultIndex)
	{
		const ValueType resultType = functionType.results()[resultIndex];
		const U8 resultNumBytes = getTypeByteWidth(resultType);

		resultOffset = (resultOffset + resultNumBytes - 1) & -I8(resultNumBytes);
		wavmAssert(resultOffset < maxThunkArgAndReturnBytes);

		emitContext.irBuilder.CreateStore(
			results[resultIndex],
			emitContext.irBuilder.CreatePointerCast(
				emitContext.irBuilder.CreateInBoundsGEP(newContextPointer,
														{emitLiteral(llvmContext, resultOffset)}),
				asLLVMType(llvmContext, resultType)->getPointerTo()));

		resultOffset += resultNumBytes;
	}

	emitContext.irBuilder.CreateRet(
		emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable));

	// Compile the LLVM IR to object code.
	std::vector<U8> objectBytes = compileLLVMModule(llvmContext, std::move(llvmModule), false);

	// Load the object code.
	auto jitModule = new LoadedModule(objectBytes, {}, false);

#if(defined(_WIN32) && !defined(_WIN64))
	const char* thunkFunctionName = "_thunk";
#else
	const char* thunkFunctionName = "thunk";
#endif
	invokeThunkFunction = jitModule->nameToFunctionMap[thunkFunctionName];
	invokeThunkFunction->type = JITFunction::Type::invokeThunk;
	invokeThunkFunction->invokeThunkType = functionType;

	return reinterpret_cast<InvokeThunkPointer>(invokeThunkFunction->baseAddress);
}

void* LLVMJIT::getIntrinsicThunk(void* nativeFunction,
								 FunctionType functionType,
								 CallingConvention callingConvention,
								 MemoryBinding defaultMemory,
								 TableBinding defaultTable)
{
	wavmAssert(callingConvention == CallingConvention::intrinsic
			   || callingConvention == CallingConvention::intrinsicWithContextSwitch
			   || callingConvention == CallingConvention::intrinsicWithMemAndTable);

	LLVMContext llvmContext;

	// Reuse cached intrinsic thunks for the same function type.
	const IntrinsicThunkKey key{
		nativeFunction,
		callingConvention == CallingConvention::intrinsicWithMemAndTable ? defaultMemory.id
																		 : UINTPTR_MAX,
		callingConvention == CallingConvention::intrinsicWithMemAndTable ? defaultTable.id
																		 : UINTPTR_MAX};
	JITFunction*& intrinsicThunkFunction
		= intrinsicFunctionToThunkFunctionMap.getOrAdd(key, nullptr);
	if(intrinsicThunkFunction)
	{ return reinterpret_cast<void*>(intrinsicThunkFunction->baseAddress); }

	// Create a LLVM module containing a single function with the same signature as the native
	// function, but with the WASM calling convention.
	llvm::Module llvmModule("", llvmContext);
	auto llvmFunctionType = asLLVMType(llvmContext, functionType, CallingConvention::wasm);
	auto function = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	function->setCallingConv(asLLVMCallingConv(callingConvention));

	llvm::Constant* defaultMemoryOffset = nullptr;
	if(defaultMemory.id != UINTPTR_MAX)
	{
		defaultMemoryOffset = emitLiteral(
			llvmContext,
			offsetof(CompartmentRuntimeData, memoryBases) + sizeof(void*) * defaultMemory.id);
	}

	llvm::Constant* defaultTableOffset = nullptr;
	if(defaultTable.id != UINTPTR_MAX)
	{
		defaultTableOffset = emitLiteral(
			llvmContext,
			offsetof(CompartmentRuntimeData, tableBases) + sizeof(void*) * defaultTable.id);
	}

	EmitContext emitContext(llvmContext, defaultMemoryOffset, defaultTableOffset);
	emitContext.irBuilder.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "entry", function));

	emitContext.initContextVariables(&*function->args().begin());

	llvm::SmallVector<llvm::Value*, 8> args;
	for(auto argIt = function->args().begin() + 1; argIt != function->args().end(); ++argIt)
	{ args.push_back(&*argIt); }

	llvm::Type* llvmNativeFunctionType
		= asLLVMType(llvmContext, functionType, callingConvention)->getPointerTo();
	llvm::Value* llvmNativeFunction = emitLiteralPointer(nativeFunction, llvmNativeFunctionType);
	ValueVector results
		= emitContext.emitCallOrInvoke(llvmNativeFunction, args, functionType, callingConvention);

	// Emit the function return.
	emitContext.emitReturn(functionType.results(), results);

	// Compile the LLVM IR to object code.
	std::vector<U8> objectBytes = compileLLVMModule(llvmContext, std::move(llvmModule), false);

	// Load the object code.
	auto jitModule = new LoadedModule(objectBytes, {}, false);

#if(defined(_WIN32) && !defined(_WIN64))
	const char* thunkFunctionName = "_thunk";
#else
	const char* thunkFunctionName = "thunk";
#endif
	intrinsicThunkFunction = jitModule->nameToFunctionMap[thunkFunctionName];
	intrinsicThunkFunction->type = JITFunction::Type::intrinsicThunk;

	return reinterpret_cast<void*>(intrinsicThunkFunction->baseAddress);
}
