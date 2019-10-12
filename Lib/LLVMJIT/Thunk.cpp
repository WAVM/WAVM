#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "EmitContext.h"
#include "LLVMJITPrivate.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/RWMutex.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

namespace llvm {
	class Value;
}

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;
using namespace WAVM::Runtime;

struct IntrinsicThunkKey
{
	void* nativeFunction;
	FunctionType type;

	friend bool operator==(const IntrinsicThunkKey& a, const IntrinsicThunkKey& b)
	{
		return a.nativeFunction == b.nativeFunction && a.type == b.type;
	}
};

namespace WAVM {
	template<> struct Hash<IntrinsicThunkKey>
	{
		Uptr operator()(const IntrinsicThunkKey& key, Uptr seed = 0) const
		{
			Uptr hash = seed;
			hash = Hash<Uptr>()(reinterpret_cast<Uptr>(key.nativeFunction), hash);
			hash = Hash<Uptr>()(key.type.getHash(), hash);
			return hash;
		}
	};
}

// A global invoke thunk cache
struct InvokeThunkCache
{
	Platform::RWMutex mutex;

	HashMap<FunctionType, Runtime::Function*> typeToFunctionMap;
	std::vector<std::unique_ptr<LLVMJIT::Module>> modules;

	static InvokeThunkCache& get()
	{
		static InvokeThunkCache singleton;
		return singleton;
	}

private:
	InvokeThunkCache() {}
};

// A global intrinsic thunk cache.
struct IntrinsicThunkCache
{
	Platform::RWMutex mutex;
	HashMap<IntrinsicThunkKey, Runtime::Function*> functionToThunkMap;
	std::vector<std::unique_ptr<LLVMJIT::Module>> modules;

	static IntrinsicThunkCache& get()
	{
		static IntrinsicThunkCache singleton;
		return singleton;
	}

private:
	IntrinsicThunkCache() {}
};

InvokeThunkPointer LLVMJIT::getInvokeThunk(FunctionType functionType)
{
	InvokeThunkCache& invokeThunkCache = InvokeThunkCache::get();

	// First, take a shareable lock on the cache mutex, and check if the thunk is cached.
	{
		Platform::RWMutex::ShareableLock shareableLock(invokeThunkCache.mutex);
		Runtime::Function** invokeThunkFunction
			= invokeThunkCache.typeToFunctionMap.get(functionType);
		if(invokeThunkFunction)
		{
			return reinterpret_cast<InvokeThunkPointer>(
				const_cast<U8*>((*invokeThunkFunction)->code));
		}
	}

	// If the thunk is not cached, take an exclusive lock on the cache mutex.
	Platform::RWMutex::ExclusiveLock invokeThunkLock(invokeThunkCache.mutex);

	// Since the cache is unlocked briefly while switching from the shareable to the exclusive lock,
	// check again if the thunk is cached.
	Runtime::Function*& invokeThunkFunction
		= invokeThunkCache.typeToFunctionMap.getOrAdd(functionType, nullptr);
	if(invokeThunkFunction)
	{ return reinterpret_cast<InvokeThunkPointer>(const_cast<U8*>(invokeThunkFunction->code)); }

	// Create a FunctionMutableData object for the thunk.
	FunctionMutableData* functionMutableData
		= new FunctionMutableData("thnk!C to WASM thunk!" + asString(functionType));

	// Create a LLVM module and a LLVM function for the thunk.
	LLVMContext llvmContext;
	llvm::Module llvmModule("", llvmContext);
	std::unique_ptr<llvm::TargetMachine> targetMachine = getTargetMachine(getHostTargetSpec());
	llvmModule.setDataLayout(targetMachine->createDataLayout());
	auto llvmFunctionType = llvm::FunctionType::get(llvmContext.i8PtrType,
													{llvmContext.i8PtrType,
													 llvmContext.i8PtrType,
													 llvmContext.i8PtrType,
													 llvmContext.i8PtrType},
													false);
	auto function = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	setRuntimeFunctionPrefix(llvmContext,
							 function,
							 emitLiteralPointer(functionMutableData, llvmContext.iptrType),
							 emitLiteral(llvmContext, Uptr(UINTPTR_MAX)),
							 emitLiteral(llvmContext, functionType.getEncoding().impl));
	setFunctionAttributes(targetMachine.get(), function);

	llvm::Value* calleeFunction = &*(function->args().begin() + 0);
	llvm::Value* contextPointer = &*(function->args().begin() + 1);
	llvm::Value* argsArray = &*(function->args().begin() + 2);
	llvm::Value* resultsArray = &*(function->args().begin() + 3);

	EmitContext emitContext(llvmContext, {});
	emitContext.irBuilder.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "entry", function));

	emitContext.initContextVariables(contextPointer);

	// Load the function's arguments from the argument array.
	std::vector<llvm::Value*> arguments;
	for(Uptr argIndex = 0; argIndex < functionType.params().size(); ++argIndex)
	{
		const ValueType paramType = functionType.params()[argIndex];
		llvm::Value* argOffset = emitLiteral(llvmContext, argIndex * sizeof(UntaggedValue));
		llvm::Value* arg = emitContext.loadFromUntypedPointer(
			emitContext.irBuilder.CreateInBoundsGEP(argsArray, {argOffset}),
			asLLVMType(llvmContext, paramType),
			alignof(UntaggedValue));
		arguments.push_back(arg);
	}

	// Call the function.
	llvm::Value* functionCode = emitContext.irBuilder.CreateInBoundsGEP(
		calleeFunction, {emitLiteral(llvmContext, Uptr(offsetof(Runtime::Function, code)))});
	ValueVector results = emitContext.emitCallOrInvoke(
		emitContext.irBuilder.CreatePointerCast(
			functionCode, asLLVMType(llvmContext, functionType)->getPointerTo()),
		arguments,
		functionType);

	// Write the function's results to the results array.
	WAVM_ASSERT(results.size() == functionType.results().size());
	for(Uptr resultIndex = 0; resultIndex < results.size(); ++resultIndex)
	{
		llvm::Value* resultOffset = emitLiteral(llvmContext, resultIndex * sizeof(UntaggedValue));
		llvm::Value* result = results[resultIndex];
		emitContext.storeToUntypedPointer(
			result,
			emitContext.irBuilder.CreateInBoundsGEP(resultsArray, {resultOffset}),
			alignof(UntaggedValue));
	}

	// Return the new context pointer.
	emitContext.irBuilder.CreateRet(
		emitContext.irBuilder.CreateLoad(emitContext.contextPointerVariable));

	// Compile the LLVM IR to object code.
	std::vector<U8> objectBytes
		= compileLLVMModule(llvmContext, std::move(llvmModule), false, targetMachine.get());

	// Load the object code.
	auto jitModule = new LLVMJIT::Module(objectBytes, {}, false);
	invokeThunkCache.modules.push_back(std::unique_ptr<LLVMJIT::Module>(jitModule));

	invokeThunkFunction = jitModule->nameToFunctionMap[mangleSymbol("thunk")];
	return reinterpret_cast<InvokeThunkPointer>(const_cast<U8*>(invokeThunkFunction->code));
}

Runtime::Function* LLVMJIT::getIntrinsicThunk(void* nativeFunction,
											  FunctionType functionType,
											  const char* debugName)
{
	IntrinsicThunkCache& intrinsicThunkCache = IntrinsicThunkCache::get();
	Platform::RWMutex::ExclusiveLock intrinsicThunkLock(intrinsicThunkCache.mutex);

	const IR::CallingConvention callingConvention = functionType.callingConvention();
	WAVM_ASSERT(callingConvention == CallingConvention::intrinsic
				|| callingConvention == CallingConvention::intrinsicWithContextSwitch
				|| callingConvention == CallingConvention::cAPICallback);

	const FunctionType wasmFunctionType(
		functionType.results(), functionType.params(), CallingConvention::wasm);

	LLVMContext llvmContext;

	// Reuse cached intrinsic thunks for the same function+type.
	Runtime::Function*& intrinsicThunkFunction
		= intrinsicThunkCache.functionToThunkMap.getOrAdd({nativeFunction, functionType}, nullptr);
	if(intrinsicThunkFunction) { return intrinsicThunkFunction; }

	// Create a FunctionMutableData object for the thunk.
	FunctionMutableData* functionMutableData
		= new FunctionMutableData(std::string("thnk!WASM to C thunk!(") + debugName + ')');

	// Create a LLVM module containing a single function with the same signature as the native
	// function, but with the WASM calling convention.
	llvm::Module llvmModule("", llvmContext);
	std::unique_ptr<llvm::TargetMachine> targetMachine = getTargetMachine(getHostTargetSpec());
	llvmModule.setDataLayout(targetMachine->createDataLayout());
	auto llvmFunctionType = asLLVMType(llvmContext, wasmFunctionType);
	auto function = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	function->setCallingConv(asLLVMCallingConv(callingConvention));
	setRuntimeFunctionPrefix(llvmContext,
							 function,
							 emitLiteralPointer(functionMutableData, llvmContext.iptrType),
							 emitLiteral(llvmContext, Uptr(UINTPTR_MAX)),
							 emitLiteral(llvmContext, wasmFunctionType.getEncoding().impl));
	setFunctionAttributes(targetMachine.get(), function);

	EmitContext emitContext(llvmContext, {});
	emitContext.irBuilder.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "entry", function));

	emitContext.initContextVariables(&*function->args().begin());

	llvm::SmallVector<llvm::Value*, 8> args;
	for(auto argIt = function->args().begin() + 1; argIt != function->args().end(); ++argIt)
	{ args.push_back(&*argIt); }

	llvm::Type* llvmNativeFunctionType = asLLVMType(llvmContext, functionType)->getPointerTo();
	llvm::Value* llvmNativeFunction = emitLiteralPointer(nativeFunction, llvmNativeFunctionType);
	ValueVector results = emitContext.emitCallOrInvoke(llvmNativeFunction, args, functionType);

	// Emit the function return.
	emitContext.emitReturn(functionType.results(), results);

	// Compile the LLVM IR to object code.
	std::vector<U8> objectBytes
		= compileLLVMModule(llvmContext, std::move(llvmModule), false, targetMachine.get());

	// Load the object code.
	auto jitModule = new LLVMJIT::Module(objectBytes, {}, false);
	intrinsicThunkCache.modules.push_back(std::unique_ptr<LLVMJIT::Module>(jitModule));

	intrinsicThunkFunction = jitModule->nameToFunctionMap[mangleSymbol("thunk")];
	return intrinsicThunkFunction;
}
