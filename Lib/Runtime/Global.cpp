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

	// Allow immutable globals to be created without a compartment.
	errorUnless(!type.isMutable || compartment);

	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	GlobalInstance* globalInstance;
	if(!type.isMutable)
	{ globalInstance = new GlobalInstance(compartment, type, UINT32_MAX, initialValue); }
	else
	{
		// Allocate a naturally aligned address to store the global at in the per-context data.
		const U32 numBytes = getTypeByteWidth(type.valueType);
		U32 dataOffset = (compartment->numGlobalBytes + numBytes - 1) & ~(numBytes - 1);
		if(dataOffset + numBytes >= maxGlobalBytes) { return nullptr; }
		compartment->numGlobalBytes = dataOffset + numBytes;

		// Initialize the global value for each context, and the data used to initialize new
		// contexts.
		memcpy(compartment->initialContextGlobalData + dataOffset, &initialValue, numBytes);
		for(Context* context : compartment->contexts)
		{ memcpy(context->runtimeData->globalData + dataOffset, &initialValue, numBytes); }

		globalInstance = new GlobalInstance(compartment, type, dataOffset, initialValue);
	}

	globalInstance->id = compartment->globals.size();
	compartment->globals.push_back(globalInstance);

	return globalInstance;
}

GlobalInstance* Runtime::cloneGlobal(GlobalInstance* global, Compartment* newCompartment)
{
	return createGlobal(
		newCompartment, global->type, Value(global->type.valueType, global->initialValue));
}

void Runtime::GlobalInstance::finalize()
{
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	compartment->globals[id] = nullptr;
}

Value Runtime::getGlobalValue(Context* context, GlobalInstance* global)
{
	wavmAssert(context || !global->type.isMutable);
	return Value(global->type.valueType,
				 global->type.isMutable ? *(UntaggedValue*)(context->runtimeData->globalData
															+ global->mutableDataOffset)
										: global->initialValue);
}

Value Runtime::setGlobalValue(Context* context, GlobalInstance* global, Value newValue)
{
	wavmAssert(context);
	wavmAssert(newValue.type == global->type.valueType);
	wavmAssert(global->type.isMutable);
	UntaggedValue& value
		= *(UntaggedValue*)(context->runtimeData->globalData + global->mutableDataOffset);
	const Value previousValue = Value(global->type.valueType, value);
	value = newValue;
	return previousValue;
}
