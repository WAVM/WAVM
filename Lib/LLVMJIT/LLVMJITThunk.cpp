#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "IR/Types.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Lock.h"
#include "LLVMEmitContext.h"
#include "LLVMJIT/LLVMJIT.h"
#include "LLVMJITPrivate.h"
#include "Platform/Mutex.h"
#include "Runtime/RuntimeData.h"

#include "LLVMPreInclude.h"

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

#include "LLVMPostInclude.h"

namespace llvm
{
	class Value;
}

using namespace IR;
using namespace LLVMJIT;
using namespace Runtime;

// A map from function types to JIT symbols for cached invoke thunks (C++ -> WASM)
static Platform::Mutex invokeThunkMutex;
static HashMap<FunctionType, struct JITFunction*> invokeThunkTypeToFunctionMap;

// A map from function types to JIT symbols for cached native thunks (WASM -> C++)
static Platform::Mutex intrinsicThunkMutex;
static HashMap<void*, struct JITFunction*> intrinsicFunctionToThunkFunctionMap;

InvokeThunkPointer LLVMJIT::getInvokeThunk(FunctionType functionType,
										   CallingConvention callingConvention)
{
	Lock<Platform::Mutex> invokeThunkLock(invokeThunkMutex);

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
		const Uptr numArgBytes = getTypeByteWidth(parameterType);
		argDataOffset = (argDataOffset + numArgBytes - 1) & -numArgBytes;

		arguments.push_back(emitContext.loadFromUntypedPointer(
			emitContext.irBuilder.CreateInBoundsGEP(
				contextPointer,
				{emitLiteral(llvmContext,
							 argDataOffset + offsetof(ContextRuntimeData, thunkArgAndReturnData))}),
			asLLVMType(llvmContext, parameterType)));

		argDataOffset += numArgBytes;
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
								 const FunctionInstance* functionInstance,
								 FunctionType functionType,
								 CallingConvention callingConvention)
{
	Lock<Platform::Mutex> intrinsicThunkLock(intrinsicThunkMutex);

	wavmAssert(callingConvention == CallingConvention::intrinsic
			   || callingConvention == CallingConvention::intrinsicWithContextSwitch);

	LLVMContext llvmContext;

	// Reuse cached intrinsic thunks for the same function type.
	JITFunction*& intrinsicThunkFunction
		= intrinsicFunctionToThunkFunctionMap.getOrAdd(nativeFunction, nullptr);
	if(intrinsicThunkFunction)
	{ return reinterpret_cast<void*>(intrinsicThunkFunction->baseAddress); }

	// Create a LLVM module containing a single function with the same signature as the native
	// function, but with the WASM calling convention.
	llvm::Module llvmModule("", llvmContext);
	auto llvmFunctionType = asLLVMType(llvmContext, functionType, CallingConvention::wasm);
	auto function = llvm::Function::Create(
		llvmFunctionType, llvm::Function::ExternalLinkage, "thunk", &llvmModule);
	function->setCallingConv(asLLVMCallingConv(callingConvention));
	function->setPrefixData(llvm::ConstantArray::get(
		llvm::ArrayType::get(llvmContext.iptrType, 2),
		{emitLiteral(llvmContext, reinterpret_cast<Uptr>(functionInstance)),
		 emitLiteral(llvmContext, functionType.getEncoding().impl)}));

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
