#include "Runtime/Runtime.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Errors.h"
#include "Inline/Lock.h"
#include "Runtime/RuntimeData.h"
#include "RuntimePrivate.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

const AnyReferee* Runtime::asAnyRef(const Object* object)
{
	return ((ObjectImpl*)object)->getAnyRef();
}

bool Runtime::isA(Object* object, const ObjectType& type)
{
	if(Runtime::ObjectKind(type.kind) != object->kind) { return false; }

	switch(type.kind)
	{
	case IR::ObjectKind::function: return asFunctionType(type) == asFunction(object)->type;
	case IR::ObjectKind::global: return isSubtype(asGlobal(object)->type, asGlobalType(type));
	case IR::ObjectKind::table: return isSubtype(asTable(object)->type, asTableType(type));
	case IR::ObjectKind::memory: return isSubtype(asMemory(object)->type, asMemoryType(type));
	case IR::ObjectKind::exceptionType:
		return asExceptionType(type) == asExceptionTypeInstance(object)->type;
	default: Errors::unreachable();
	}
}

IR::ObjectType Runtime::getObjectType(Object* object)
{
	switch(object->kind)
	{
	case Runtime::ObjectKind::function: return asFunction(object)->type;
	case Runtime::ObjectKind::global: return asGlobal(object)->type;
	case Runtime::ObjectKind::table: return asTable(object)->type;
	case Runtime::ObjectKind::memory: return asMemory(object)->type;
	case Runtime::ObjectKind::exceptionTypeInstance: return asExceptionTypeInstance(object)->type;
	default: Errors::unreachable();
	};
}

FunctionType Runtime::getFunctionType(FunctionInstance* function) { return function->type; }

Context* Runtime::getContextFromRuntimeData(ContextRuntimeData* contextRuntimeData)
{
	const CompartmentRuntimeData* compartmentRuntimeData
		= getCompartmentRuntimeData(contextRuntimeData);
	const Uptr contextId = contextRuntimeData - compartmentRuntimeData->contexts;
	Lock<Platform::Mutex> compartmentLock(compartmentRuntimeData->compartment->mutex);
	return compartmentRuntimeData->compartment->contexts[contextId];
}

ContextRuntimeData* Runtime::getContextRuntimeData(Context* context)
{
	return context->runtimeData;
}

TableInstance* Runtime::getTableFromRuntimeData(ContextRuntimeData* contextRuntimeData,
												Uptr tableId)
{
	Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	wavmAssert(compartment->tables.contains(tableId));
	return compartment->tables[tableId];
}

MemoryInstance* Runtime::getMemoryFromRuntimeData(ContextRuntimeData* contextRuntimeData,
												  Uptr memoryId)
{
	Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	return compartment->memories[memoryId];
}
