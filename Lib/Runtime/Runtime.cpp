#include "WAVM/Runtime/Runtime.h"
#include "RuntimePrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

#define DEFINE_OBJECT_TYPE(kindId, kindName, Type)                                                 \
	Type* Runtime::as##kindName(Object* object)                                                    \
	{                                                                                              \
		wavmAssert(!object || object->kind == kindId);                                             \
		return (Type*)object;                                                                      \
	}                                                                                              \
	Type* Runtime::as##kindName##Nullable(Object* object)                                          \
	{                                                                                              \
		return object && object->kind == kindId ? (Type*)object : nullptr;                         \
	}                                                                                              \
	const Type* Runtime::as##kindName(const Object* object)                                        \
	{                                                                                              \
		wavmAssert(!object || object->kind == kindId);                                             \
		return (const Type*)object;                                                                \
	}                                                                                              \
	const Type* Runtime::as##kindName##Nullable(const Object* object)                              \
	{                                                                                              \
		return object && object->kind == kindId ? (const Type*)object : nullptr;                   \
	}                                                                                              \
	Object* Runtime::asObject(Type* object) { return (Object*)object; }                            \
	const Object* Runtime::asObject(const Type* object) { return (const Object*)object; }

DEFINE_OBJECT_TYPE(ObjectKind::function, Function, FunctionInstance);
DEFINE_OBJECT_TYPE(ObjectKind::table, Table, TableInstance);
DEFINE_OBJECT_TYPE(ObjectKind::memory, Memory, MemoryInstance);
DEFINE_OBJECT_TYPE(ObjectKind::global, Global, GlobalInstance);
DEFINE_OBJECT_TYPE(ObjectKind::exceptionType, ExceptionType, ExceptionTypeInstance);
DEFINE_OBJECT_TYPE(ObjectKind::moduleInstance, ModuleInstance, ModuleInstance);
DEFINE_OBJECT_TYPE(ObjectKind::context, Context, Context);
DEFINE_OBJECT_TYPE(ObjectKind::compartment, Compartment, Compartment);

bool Runtime::isA(Object* object, const ExternType& type)
{
	if(ObjectKind(type.kind) != object->kind) { return false; }

	switch(type.kind)
	{
	case ExternKind::function: return asFunction(object)->encodedType == asFunctionType(type);
	case ExternKind::global: return isSubtype(asGlobal(object)->type, asGlobalType(type));
	case ExternKind::table: return isSubtype(asTable(object)->type, asTableType(type));
	case ExternKind::memory: return isSubtype(asMemory(object)->type, asMemoryType(type));
	case ExternKind::exceptionType: return asExceptionType(type) == asExceptionType(object)->type;
	default: Errors::unreachable();
	}
}

ExternType Runtime::getObjectType(Object* object)
{
	switch(object->kind)
	{
	case ObjectKind::function: return FunctionType(asFunction(object)->encodedType);
	case ObjectKind::global: return asGlobal(object)->type;
	case ObjectKind::table: return asTable(object)->type;
	case ObjectKind::memory: return asMemory(object)->type;
	case ObjectKind::exceptionType: return asExceptionType(object)->type;
	default: Errors::unreachable();
	};
}

FunctionType Runtime::getFunctionType(FunctionInstance* function) { return function->encodedType; }

Context* Runtime::getContextFromRuntimeData(ContextRuntimeData* contextRuntimeData)
{
	const CompartmentRuntimeData* compartmentRuntimeData
		= getCompartmentRuntimeData(contextRuntimeData);
	const Uptr contextId = contextRuntimeData - compartmentRuntimeData->contexts;
	Lock<Platform::Mutex> compartmentLock(compartmentRuntimeData->compartment->mutex);
	return compartmentRuntimeData->compartment->contexts[contextId];
}

ContextRuntimeData* Runtime::getContextRuntimeData(const Context* context)
{
	return context->runtimeData;
}

ModuleInstance* Runtime::getModuleInstanceFromRuntimeData(ContextRuntimeData* contextRuntimeData,
														  Uptr moduleInstanceId)
{
	Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	wavmAssert(compartment->moduleInstances.contains(moduleInstanceId));
	return compartment->moduleInstances[moduleInstanceId];
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
