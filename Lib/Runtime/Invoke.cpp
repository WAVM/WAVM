#include <string.h>
#include <memory>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

UntaggedValue* Runtime::invokeFunctionUnchecked(Context* context,
												Function* function,
												const UntaggedValue* arguments)
{
	FunctionType functionType = function->encodedType;

	// Get the invoke thunk for this function type. Cache it in the function's FunctionMutableData
	// to avoid the global lock implied by LLVMJIT::getInvokeThunk.
	InvokeThunkPointer invokeThunk
		= function->mutableData->invokeThunk.load(std::memory_order_acquire);
	while(!invokeThunk)
	{
		InvokeThunkPointer newInvokeThunk = LLVMJIT::getInvokeThunk(functionType);
		function->mutableData->invokeThunk.compare_exchange_strong(
			invokeThunk, newInvokeThunk, std::memory_order_acq_rel);
	};
	wavmAssert(invokeThunk);

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
			throwException(ExceptionTypes::outOfMemory);
		}
		memcpy(argData + argDataOffset, argument.bytes, getTypeByteWidth(type));
		argDataOffset += numArgBytes;
	}

	// Call the invoke thunk.
	contextRuntimeData = (*invokeThunk)(function, contextRuntimeData);

	// Return a pointer to the return value that was written to the ContextRuntimeData.
	return (UntaggedValue*)contextRuntimeData->thunkArgAndReturnData;
}

ValueTuple Runtime::invokeFunctionChecked(Context* context,
										  Function* function,
										  const std::vector<Value>& arguments)
{
	errorUnless(isInCompartment(asObject(function), context->compartment));

	FunctionType functionType{function->encodedType};

	// Check that the parameter types match the function, and copy them into a memory block that
	// stores each as a 64-bit value.
	if(arguments.size() != functionType.params().size())
	{ throwException(ExceptionTypes::invokeSignatureMismatch); }

	// Convert the arguments from a vector of Values to a stack-allocated block of
	// UntaggedValues.
	UntaggedValue* untaggedArguments
		= (UntaggedValue*)alloca(arguments.size() * sizeof(UntaggedValue));
	for(Uptr argumentIndex = 0; argumentIndex < arguments.size(); ++argumentIndex)
	{
		const Value& argument = arguments[argumentIndex];
		if(!isSubtype(argument.type, functionType.params()[argumentIndex]))
		{ throwException(ExceptionTypes::invokeSignatureMismatch); }

		errorUnless(!isReferenceType(argument.type) || !argument.object
					|| isInCompartment(argument.object, context->compartment));

		untaggedArguments[argumentIndex] = argument;
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
		case ValueType::anyref: results.values.push_back(Value(*(Object**)result)); break;
		case ValueType::funcref: results.values.push_back(Value(*(Function**)result)); break;
		default: Errors::unreachable();
		};

		resultOffset += resultNumBytes;
	}
	return results;
}
