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
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/RuntimeData.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

namespace llvm {
	class Value;
}

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;
using namespace WAVM::Runtime;

// A map from function types to JIT symbols for cached invoke thunks (C++ -> WASM)
static Platform::Mutex invokeThunkMutex;
static HashMap<FunctionType, Runtime::Function*> invokeThunkTypeToFunctionMap;

// A map from function types to JIT symbols for cached native thunks (WASM -> C++)
static Platform::Mutex intrinsicThunkMutex;
static HashMap<void*, Runtime::Function*> intrinsicFunctionToThunkFunctionMap;

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
	auto llvmFunctionType = llvm::FunctionType::get(
		llvmContext.i8PtrType, {llvmContext.i8PtrType, llvmContext.i8PtrType}, false);
	auto function = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	setRuntimeFunctionPrefix(llvmContext,
							 function,
							 emitLiteralPointer(functionMutableData, llvmContext.iptrType),
							 emitLiteral(llvmContext, Uptr(UINTPTR_MAX)),
							 emitLiteral(llvmContext, functionType.getEncoding().impl));
	setFramePointerAttribute(function);

	llvm::Value* calleeFunction = &*(function->args().begin() + 0);
	llvm::Value* contextPointer = &*(function->args().begin() + 1);

	EmitContext emitContext(llvmContext, nullptr);
	emitContext.irBuilder.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "entry", function));

	emitContext.initContextVariables(contextPointer);

	// Load the function's arguments from an array of 64-bit values at an address provided by the
	// caller.
	std::vector<llvm::Value*> arguments;
	Uptr argDataOffset = 0;
	for(ValueType parameterType : functionType.params())
	{
		// Naturally align each argument.
		const U32 numArgBytes = getTypeByteWidth(parameterType);
		argDataOffset = (argDataOffset + numArgBytes - 1) & -numArgBytes;

		arguments.push_back(emitContext.loadFromUntypedPointer(
			emitContext.irBuilder.CreateInBoundsGEP(
				contextPointer,
				{emitLiteral(llvmContext,
							 argDataOffset + offsetof(ContextRuntimeData, thunkArgAndReturnData))}),
			asLLVMType(llvmContext, parameterType),
			numArgBytes));

		argDataOffset += numArgBytes;
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
	auto jitModule = new LLVMJIT::Module(objectBytes, {}, false);
	Platform::expectLeakedObject(jitModule);

#if(defined(_WIN32) && !defined(_WIN64))
	const char* thunkFunctionName = "_thunk";
#else
	const char* thunkFunctionName = "thunk";
#endif
	invokeThunkFunction = jitModule->nameToFunctionMap[thunkFunctionName];
	return reinterpret_cast<InvokeThunkPointer>(const_cast<U8*>(invokeThunkFunction->code));
}

Runtime::Function* LLVMJIT::getIntrinsicThunk(void* nativeFunction,
											  FunctionType functionType,
											  CallingConvention callingConvention,
											  const char* debugName)
{
	Lock<Platform::Mutex> intrinsicThunkLock(intrinsicThunkMutex);

	wavmAssert(callingConvention == CallingConvention::intrinsic
			   || callingConvention == CallingConvention::intrinsicWithContextSwitch);

	LLVMContext llvmContext;

	// Reuse cached intrinsic thunks for the same function type.
	Runtime::Function*& intrinsicThunkFunction
		= intrinsicFunctionToThunkFunctionMap.getOrAdd(nativeFunction, nullptr);
	if(intrinsicThunkFunction) { return intrinsicThunkFunction; }

	// Create a FunctionMutableData object for the thunk.
	FunctionMutableData* functionMutableData
		= new FunctionMutableData(std::string("thnk!WASM to C thunk!(") + debugName + ')');

	// Create a LLVM module containing a single function with the same signature as the native
	// function, but with the WASM calling convention.
	llvm::Module llvmModule("", llvmContext);
	auto llvmFunctionType = asLLVMType(llvmContext, functionType, CallingConvention::wasm);
	auto function = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	function->setCallingConv(asLLVMCallingConv(callingConvention));
	setRuntimeFunctionPrefix(llvmContext,
							 function,
							 emitLiteralPointer(functionMutableData, llvmContext.iptrType),
							 emitLiteral(llvmContext, Uptr(UINTPTR_MAX)),
							 emitLiteral(llvmContext, functionType.getEncoding().impl));
	setFramePointerAttribute(function);

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
	std::vector<U8> objectBytes = compileLLVMModule(llvmContext, std::move(llvmModule), false);

	// Load the object code.
	auto jitModule = new LLVMJIT::Module(objectBytes, {}, false);
	Platform::expectLeakedObject(jitModule);

#if(defined(_WIN32) && !defined(_WIN64))
	const char* thunkFunctionName = "_thunk";
#else
	const char* thunkFunctionName = "thunk";
#endif
	intrinsicThunkFunction = jitModule->nameToFunctionMap[thunkFunctionName];
	return intrinsicThunkFunction;
}
