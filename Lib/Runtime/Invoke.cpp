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

	// Use unwindSignalsAsExceptions to ensure that any signal that occurs in WebAssembly code calls
	// C++ destructors on the stack between here and where it is caught.
	unwindSignalsAsExceptions([&] {
		ContextRuntimeData* contextRuntimeData = getContextRuntimeData(context);

		// Call the invoke thunk.
		(*invokeThunk)(function, contextRuntimeData, arguments, outResults);
	});
}
