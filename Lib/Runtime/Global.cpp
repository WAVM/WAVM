#include <stdint.h>
#include <string.h>
#include <atomic>
#include <vector>

#include "IR/Types.h"
#include "IR/Value.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Lock.h"
#include "Platform/Platform.h"
#include "Runtime/Runtime.h"
#include "Runtime/RuntimeData.h"
#include "RuntimePrivate.h"

using namespace IR;
using namespace Runtime;

GlobalInstance* Runtime::createGlobal(Compartment* compartment, GlobalType type, Value initialValue)
{
	wavmAssert(initialValue.type == type.valueType);

	U32 mutableGlobalId = UINT32_MAX;
	if(type.isMutable)
	{
		mutableGlobalId = compartment->globalDataAllocationMask.getSmallestNonMember();
		compartment->globalDataAllocationMask.add(mutableGlobalId);

		// Initialize the global value for each context, and the data used to initialize new
		// contexts.
		compartment->initialContextMutableGlobals[mutableGlobalId] = initialValue;
		for(Context* context : compartment->contexts)
		{ context->runtimeData->mutableGlobals[mutableGlobalId] = initialValue; }
	}

	// Create the global and add it to the compartment's list of globals.
	GlobalInstance* globalInstance
		= new GlobalInstance(compartment, type, mutableGlobalId, initialValue);
	{
		Lock<Platform::Mutex> compartmentLock(compartment->mutex);
		compartment->globals.addOrFail(globalInstance);
	}

	return globalInstance;
}

GlobalInstance* Runtime::cloneGlobal(GlobalInstance* global, Compartment* newCompartment)
{
	GlobalInstance* newGlobal = new GlobalInstance(
		newCompartment, global->type, global->mutableGlobalId, global->initialValue);
	newCompartment->globals.addOrFail(newGlobal);
	return newGlobal;
}

void Runtime::GlobalInstance::finalize()
{
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	compartment->globals.removeOrFail(this);
}

Value Runtime::getGlobalValue(Context* context, GlobalInstance* global)
{
	wavmAssert(context || !global->type.isMutable);
	return Value(global->type.valueType,
				 global->type.isMutable
					 ? context->runtimeData->mutableGlobals[global->mutableGlobalId]
					 : global->initialValue);
}

Value Runtime::setGlobalValue(Context* context, GlobalInstance* global, Value newValue)
{
	wavmAssert(context);
	wavmAssert(newValue.type == global->type.valueType);
	wavmAssert(global->type.isMutable);
	UntaggedValue& value = context->runtimeData->mutableGlobals[global->mutableGlobalId];
	const Value previousValue = Value(global->type.valueType, value);
	value = newValue;
	return previousValue;
}
