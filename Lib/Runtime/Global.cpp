#include <stdint.h>
#include <string.h>
#include <atomic>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

GlobalInstance* Runtime::createGlobal(Compartment* compartment, GlobalType type, Value initialValue)
{
	errorUnless(isSubtype(initialValue.type, type.valueType));
	errorUnless(!isReferenceType(type.valueType) || !initialValue.object
				|| isInCompartment(initialValue.object, compartment));

	U32 mutableGlobalId = UINT32_MAX;
	if(type.isMutable)
	{
		mutableGlobalId = compartment->globalDataAllocationMask.getSmallestNonMember();
		if(mutableGlobalId == maxMutableGlobals) { return nullptr; }
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

Runtime::GlobalInstance::~GlobalInstance()
{
	wavmAssertMutexIsLockedByCurrentThread(compartment->mutex);
	compartment->globals.removeOrFail(this);
	if(type.isMutable)
	{
		wavmAssert(mutableGlobalId < maxMutableGlobals);
		wavmAssert(compartment->globalDataAllocationMask.contains(mutableGlobalId));
		compartment->globalDataAllocationMask.remove(mutableGlobalId);
	}
}

Value Runtime::getGlobalValue(const Context* context, GlobalInstance* global)
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
	errorUnless(context->compartment == global->compartment);
	errorUnless(!isReferenceType(global->type.valueType) || !newValue.object
				|| isInCompartment(newValue.object, context->compartment));
	UntaggedValue& value = context->runtimeData->mutableGlobals[global->mutableGlobalId];
	const Value previousValue = Value(global->type.valueType, value);
	value = newValue;
	return previousValue;
}
