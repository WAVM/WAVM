#include <string.h>
#include <atomic>
#include <vector>

#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Lock.h"
#include "Platform/Platform.h"
#include "Runtime/Runtime.h"
#include "Runtime/RuntimeData.h"
#include "RuntimePrivate.h"

using namespace Runtime;

Context* Runtime::createContext(Compartment* compartment)
{
	wavmAssert(compartment);
	Context* context = new Context(compartment);
	{
		Lock<Platform::Mutex> lock(compartment->mutex);

		// Allocate an ID for the context in the compartment.
		context->id = compartment->contexts.add(UINTPTR_MAX, context);
		if(context->id == UINTPTR_MAX)
		{
			delete context;
			return nullptr;
		}
		context->runtimeData = &compartment->runtimeData->contexts[context->id];

		// Commit the page(s) for the context's runtime data.
		errorUnless(Platform::commitVirtualPages(
			(U8*)context->runtimeData, sizeof(ContextRuntimeData) >> Platform::getPageSizeLog2()));

		// Initialize the context's global data.
		memcpy(context->runtimeData->mutableGlobals,
			   compartment->initialContextMutableGlobals,
			   maxGlobalBytes);
	}

	return context;
}

void Runtime::Context::finalize()
{
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	compartment->contexts.removeOrFail(id);
}

Compartment* Runtime::getCompartmentFromContext(Context* context) { return context->compartment; }

Context* Runtime::cloneContext(Context* context, Compartment* newCompartment)
{
	// Create a new context and initialize its runtime data with the values from the source context.
	Context* clonedContext = createContext(newCompartment);
	memcpy(clonedContext->runtimeData->mutableGlobals,
		   context->runtimeData->mutableGlobals,
		   maxGlobalBytes);
	return clonedContext;
}
