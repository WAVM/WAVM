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
#include "WAVM/Inline/Lock.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Mutex.h"
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

// A map from function types to JIT symbols for cached invoke thunks (C++ -> WASM)
static Platform::Mutex invokeThunkMutex;
static HashMap<FunctionType, Runtime::Function*> invokeThunkTypeToFunctionMap;

// A map from function types to JIT symbols for cached native thunks (WASM -> C++)
static Platform::Mutex intrinsicThunkMutex;
static HashMap<IntrinsicThunkKey, Runtime::Function*> intrinsicFunctionToThunkFunctionMap;

InvokeThunkPointer LLVMJIT::getInvokeThunk(FunctionType functionType)
{
	Lock<Platform::Mutex> invokeThunkLock(invokeThunkMutex);

	// Reuse cached invoke thunks for the same function type.
	Runtime::Function*& invokeThunkFunction
		= invokeThunkTypeToFunctionMap.getOrAdd(functionType, nullptr);
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

	EmitContext emitContext(llvmContext, nullptr);
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
			functionCode,
			asLLVMType(llvmContext, functionType, IR::CallingConvention::wasm)->getPointerTo()),
		arguments,
		functionType,
		IR::CallingConvention::wasm);

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
	Platform::expectLeakedObject(jitModule);

	invokeThunkFunction = jitModule->nameToFunctionMap[mangleSymbol("thunk")];
	return reinterpret_cast<InvokeThunkPointer>(const_cast<U8*>(invokeThunkFunction->code));
}

Runtime::Function* LLVMJIT::getIntrinsicThunk(void* nativeFunction,
											  FunctionType functionType,
											  CallingConvention callingConvention,
											  const char* debugName)
{
	Lock<Platform::Mutex> intrinsicThunkLock(intrinsicThunkMutex);

	WAVM_ASSERT(callingConvention == CallingConvention::intrinsic
				|| callingConvention == CallingConvention::intrinsicWithContextSwitch
				|| callingConvention == CallingConvention::cAPICallback);

	LLVMContext llvmContext;

	// Reuse cached intrinsic thunks for the same function+type.
	Runtime::Function*& intrinsicThunkFunction
		= intrinsicFunctionToThunkFunctionMap.getOrAdd({nativeFunction, functionType}, nullptr);
	if(intrinsicThunkFunction) { return intrinsicThunkFunction; }

	// Create a FunctionMutableData object for the thunk.
	FunctionMutableData* functionMutableData
		= new FunctionMutableData(std::string("thnk!WASM to C thunk!(") + debugName + ')');

	// Create a LLVM module containing a single function with the same signature as the native
	// function, but with the WASM calling convention.
	llvm::Module llvmModule("", llvmContext);
	std::unique_ptr<llvm::TargetMachine> targetMachine = getTargetMachine(getHostTargetSpec());
	llvmModule.setDataLayout(targetMachine->createDataLayout());
	auto llvmFunctionType = asLLVMType(llvmContext, functionType, CallingConvention::wasm);
	auto function = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	function->setCallingConv(asLLVMCallingConv(callingConvention));
	setRuntimeFunctionPrefix(llvmContext,
							 function,
							 emitLiteralPointer(functionMutableData, llvmContext.iptrType),
							 emitLiteral(llvmContext, Uptr(UINTPTR_MAX)),
							 emitLiteral(llvmContext, functionType.getEncoding().impl));
	setFunctionAttributes(targetMachine.get(), function);

	EmitContext emitContext(llvmContext, nullptr);
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
	std::vector<U8> objectBytes
		= compileLLVMModule(llvmContext, std::move(llvmModule), false, targetMachine.get());

	// Load the object code.
	auto jitModule = new LLVMJIT::Module(objectBytes, {}, false);
	Platform::expectLeakedObject(jitModule);

	intrinsicThunkFunction = jitModule->nameToFunctionMap[mangleSymbol("thunk")];
	return intrinsicThunkFunction;
}
