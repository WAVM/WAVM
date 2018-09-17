#include <string.h>
#include <memory>
#include <vector>

#include "IR/Types.h"
#include "IR/Value.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "LLVMJIT/LLVMJIT.h"
#include "Runtime/Runtime.h"
#include "Runtime/RuntimeData.h"
#include "RuntimePrivate.h"

using namespace IR;
using namespace Runtime;

const AnyFunc* Runtime::asAnyFunc(const FunctionInstance* functionInstance,
								  MemoryInstance* intrinsicDefaultMemory,
								  TableInstance* intrinsicDefaultTable)
{
	void* wasmFunction = functionInstance->nativeFunction;

	// If the function isn't a WASM function, generate a thunk for it.
	if(functionInstance->callingConvention != IR::CallingConvention::wasm)
	{
		wasmFunction = LLVMJIT::getIntrinsicThunk(
			wasmFunction,
			functionInstance,
			functionInstance->type,
			functionInstance->callingConvention,
			LLVMJIT::MemoryBinding{intrinsicDefaultMemory ? intrinsicDefaultMemory->id
														  : UINTPTR_MAX},
			LLVMJIT::TableBinding{intrinsicDefaultTable ? intrinsicDefaultTable->id : UINTPTR_MAX});
	}

	// Get the pointer to the AnyFunc struct that is emitted as a prefix to the function's code.
	return (AnyFunc*)((U8*)wasmFunction - offsetof(AnyFunc, code));
}

UntaggedValue* Runtime::invokeFunctionUnchecked(Context* context,
												FunctionInstance* function,
												const UntaggedValue* arguments)
{
	FunctionType functionType = function->type;

	// Get the invoke thunk for this function type.
	auto invokeFunctionPointer = LLVMJIT::getInvokeThunk(functionType, function->callingConvention);

	// Copy the arguments into the thunk arguments buffer in ContextRuntimeData.
	ContextRuntimeData* contextRuntimeData
		= &context->compartment->runtimeData->contexts[context->id];
	U8* argData = contextRuntimeData->thunkArgAndReturnData;
	Uptr argDataOffset = 0;
	for(Uptr argumentIndex = 0; argumentIndex < functionType.params().size(); ++argumentIndex)
	{
		const ValueType type = functionType.params()[argumentIndex];
		const UntaggedValue& argument = arguments[argumentIndex];

		// Naturally align each argument.
		const Uptr numArgBytes = getTypeByteWidth(type);
		argDataOffset = (argDataOffset + numArgBytes - 1) & -numArgBytes;

		if(argDataOffset >= maxThunkArgAndReturnBytes)
		{
			// Throw an exception if the invoke uses too much memory for arguments.
			throwException(Exception::outOfMemoryType);
		}
		memcpy(argData + argDataOffset, argument.bytes, getTypeByteWidth(type));
		argDataOffset += numArgBytes;
	}

	// Call the invoke thunk.
	contextRuntimeData = (ContextRuntimeData*)(*invokeFunctionPointer)(function->nativeFunction,
																	   contextRuntimeData);

	// Return a pointer to the return value that was written to the ContextRuntimeData.
	return (UntaggedValue*)contextRuntimeData->thunkArgAndReturnData;
}

ValueTuple Runtime::invokeFunctionChecked(Context* context,
										  FunctionInstance* function,
										  const std::vector<Value>& arguments)
{
	FunctionType functionType = function->type;

	// Check that the parameter types match the function, and copy them into a memory block that
	// stores each as a 64-bit value.
	if(arguments.size() != functionType.params().size())
	{ throwException(Exception::invokeSignatureMismatchType); }

	// Convert the arguments from a vector of Values to a stack-allocated block of
	// UntaggedValues.
	UntaggedValue* untaggedArguments
		= (UntaggedValue*)alloca(arguments.size() * sizeof(UntaggedValue));
	for(Uptr argumentIndex = 0; argumentIndex < arguments.size(); ++argumentIndex)
	{
		if(!isSubtype(arguments[argumentIndex].type, functionType.params()[argumentIndex]))
		{ throwException(Exception::invokeSignatureMismatchType); }

		untaggedArguments[argumentIndex] = arguments[argumentIndex];
	}

	// Call the unchecked version of this function to do the actual invoke.
	U8* resultStructBase = (U8*)invokeFunctionUnchecked(context, function, untaggedArguments);

	// Read the return values out of the context's scratch memory.
	ValueTuple results;
	Uptr resultOffset = 0;
	for(ValueType resultType : functionType.results())
	{
		const U8 resultNumBytes = getTypeByteWidth(resultType);

		resultOffset = (resultOffset + resultNumBytes - 1) & -I8(resultNumBytes);
		wavmAssert(resultOffset < maxThunkArgAndReturnBytes);

		U8* result = resultStructBase + resultOffset;
		switch(resultType)
		{
		case ValueType::i32: results.values.push_back(Value(*(I32*)result)); break;
		case ValueType::i64: results.values.push_back(Value(*(I64*)result)); break;
		case ValueType::f32: results.values.push_back(Value(*(F32*)result)); break;
		case ValueType::f64: results.values.push_back(Value(*(F64*)result)); break;
		case ValueType::v128: results.values.push_back(Value(*(V128*)result)); break;
		case ValueType::anyref: results.values.push_back(Value(*(const AnyReferee**)result)); break;
		case ValueType::anyfunc: results.values.push_back(Value(*(const AnyFunc**)result)); break;
		default: Errors::unreachable();
		};

		resultOffset += resultNumBytes;
	}
	return results;
}
