#include "IR/Types.h"
#include "IR/Value.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "LLVMJIT/LLVMJIT.h"
#include "Runtime/Runtime.h"
#include "RuntimePrivate.h"

using namespace IR;
using namespace Runtime;

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
		if(type == ValueType::v128)
		{
			// Use 16-byte alignment for V128 arguments.
			argDataOffset = (argDataOffset + 15) & ~15;
		}
		if(argDataOffset >= maxThunkArgAndReturnBytes)
		{
			// Throw an exception if the invoke uses too much memory for arguments.
			throwException(Exception::outOfMemoryType);
		}
		memcpy(argData + argDataOffset, argument.bytes, getTypeByteWidth(type));
		argDataOffset += type == ValueType::v128 ? 16 : 8;
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
		if(functionType.params()[argumentIndex] != arguments[argumentIndex].type)
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

		UntaggedValue* result = (UntaggedValue*)(resultStructBase + resultOffset);
		results.values.push_back(Value(resultType, *result));

		resultOffset += resultNumBytes;
	}
	return results;
}
