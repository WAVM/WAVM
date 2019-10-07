#include <string.h>
#include <atomic>
#include <vector>
#include "RuntimePrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Platform/RWMutex.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

using namespace WAVM;
using namespace WAVM::Runtime;

Context* Runtime::createContext(Compartment* compartment)
{
	WAVM_ASSERT(compartment);
	Context* context = new Context(compartment);
	{
		Platform::RWMutex::ExclusiveLock lock(compartment->mutex);

		// Allocate an ID for the context in the compartment.
		context->id = compartment->contexts.add(UINTPTR_MAX, context);
		if(context->id == UINTPTR_MAX)
		{
			delete context;
			return nullptr;
		}
		context->runtimeData = &compartment->runtimeData->contexts[context->id];

		// Commit the page(s) for the context's runtime data.
		WAVM_ERROR_UNLESS(Platform::commitVirtualPages(
			(U8*)context->runtimeData,
			sizeof(ContextRuntimeData) >> Platform::getBytesPerPageLog2()));

		// Initialize the context's global data.
		memcpy(context->runtimeData->mutableGlobals,
			   compartment->initialContextMutableGlobals,
			   maxMutableGlobals * sizeof(IR::UntaggedValue));

		context->runtimeData->context = context;
	}

	return context;
}

Runtime::Context::~Context()
{
	WAVM_ASSERT_RWMUTEX_IS_EXCLUSIVELY_LOCKED_BY_CURRENT_THREAD(compartment->mutex);
	compartment->contexts.removeOrFail(id);
}

Compartment* Runtime::getCompartment(const Context* context) { return context->compartment; }

Context* Runtime::cloneContext(const Context* context, Compartment* newCompartment)
{
	// Create a new context and initialize its runtime data with the values from the source context.
	Context* clonedContext = createContext(newCompartment);
	memcpy(clonedContext->runtimeData->mutableGlobals,
		   context->runtimeData->mutableGlobals,
		   maxMutableGlobals * sizeof(IR::UntaggedValue));
	return clonedContext;
}
