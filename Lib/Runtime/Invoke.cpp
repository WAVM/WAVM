#include <string.h>
#include <memory>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

void Runtime::invokeFunction(Context* context,
							 const Function* function,
							 FunctionType invokeSig,
							 const UntaggedValue arguments[],
							 UntaggedValue outResults[])
{
	FunctionType functionType{function->encodedType};

	// Verify that the invoke signature matches the function being invoked.
	if(invokeSig != functionType && !isSubtype(functionType, invokeSig))
	{
		if(Log::isCategoryEnabled(Log::debug))
		{
			Log::printf(
				Log::debug,
				"Invoke signature mismatch:\n  Invoke signature: %s\n  Invoked function type: %s\n",
				asString(invokeSig).c_str(),
				asString(getFunctionType(function)).c_str());
		}
		throwException(ExceptionTypes::invokeSignatureMismatch);
	}

	// Assert that the function, the context, and any reference arguments are all in the same
	// compartment.
	if(WAVM_ENABLE_ASSERTS)
	{
		WAVM_ASSERT(isInCompartment(asObject(function), context->compartment));
		for(Uptr argumentIndex = 0; argumentIndex < invokeSig.params().size(); ++argumentIndex)
		{
			const ValueType argType = invokeSig.params()[argumentIndex];
			const UntaggedValue& arg = arguments[argumentIndex];
			WAVM_ASSERT(!isReferenceType(argType) || !arg.object
						|| isInCompartment(arg.object, context->compartment));
		}
	}

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
	WAVM_ASSERT(invokeThunk);

	// Copy the arguments into the scratch buffer in ContextRuntimeData.
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

	// Use unwindSignalsAsExceptions to ensure that any signal that occurs in WebAssembly code calls
	// C++ destructors on the stack between here and where it is caught.
	unwindSignalsAsExceptions([&contextRuntimeData, invokeThunk, function] {
		// Call the invoke thunk.
		contextRuntimeData = (*invokeThunk)(function, contextRuntimeData);
	});

	// Copy the results from the scratch buffer in ContextRuntimeData.
	Uptr resultOffset = 0;
	for(Uptr resultIndex = 0; resultIndex < functionType.results().size(); ++resultIndex)
	{
		const ValueType resultType = functionType.results()[resultIndex];
		const U8 resultNumBytes = getTypeByteWidth(resultType);

		resultOffset = (resultOffset + resultNumBytes - 1) & -I8(resultNumBytes);
		WAVM_ASSERT(resultOffset < maxThunkArgAndReturnBytes);
		WAVM_ASSERT((resultOffset & (resultNumBytes - 1)) == 0)

		U8* result = contextRuntimeData->thunkArgAndReturnData + resultOffset;
		switch(resultType)
		{
		case ValueType::i32: outResults[resultIndex].i32 = *(I32*)result; break;
		case ValueType::i64: outResults[resultIndex].i64 = *(I64*)result; break;
		case ValueType::f32: outResults[resultIndex].f32 = *(F32*)result; break;
		case ValueType::f64: outResults[resultIndex].f64 = *(F64*)result; break;
		case ValueType::v128: outResults[resultIndex].v128 = *(V128*)result; break;
		case ValueType::anyref: outResults[resultIndex].object = *(Object**)result; break;
		case ValueType::funcref: outResults[resultIndex].function = *(Function**)result; break;

		case ValueType::none:
		case ValueType::any:
		case ValueType::nullref:
		default: WAVM_UNREACHABLE();
		};

		resultOffset += resultNumBytes;
	}
}
