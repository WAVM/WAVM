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
	Runtime::Type* Runtime::as##kindName(Object* object)                                           \
	{                                                                                              \
		wavmAssert(!object || object->kind == kindId);                                             \
		return (Runtime::Type*)object;                                                             \
	}                                                                                              \
	Runtime::Type* Runtime::as##kindName##Nullable(Object* object)                                 \
	{                                                                                              \
		return object && object->kind == kindId ? (Runtime::Type*)object : nullptr;                \
	}                                                                                              \
	const Runtime::Type* Runtime::as##kindName(const Object* object)                               \
	{                                                                                              \
		wavmAssert(!object || object->kind == kindId);                                             \
		return (const Runtime::Type*)object;                                                       \
	}                                                                                              \
	const Runtime::Type* Runtime::as##kindName##Nullable(const Object* object)                     \
	{                                                                                              \
		return object && object->kind == kindId ? (const Runtime::Type*)object : nullptr;          \
	}                                                                                              \
	Object* Runtime::asObject(Runtime::Type* object) { return (Object*)object; }                   \
	const Object* Runtime::asObject(const Runtime::Type* object) { return (const Object*)object; }

#define DEFINE_GCOBJECT_TYPE(kindId, kindName, Type)                                               \
	DEFINE_OBJECT_TYPE(kindId, kindName, Type)                                                     \
	void Runtime::setUserData(Runtime::Type* object, void* userData)                               \
	{                                                                                              \
		object->userData = userData;                                                               \
	}                                                                                              \
	void* Runtime::getUserData(const Runtime::Type* object) { return object->userData; }

DEFINE_GCOBJECT_TYPE(ObjectKind::table, Table, Table);
DEFINE_GCOBJECT_TYPE(ObjectKind::memory, Memory, Memory);
DEFINE_GCOBJECT_TYPE(ObjectKind::global, Global, Global);
DEFINE_GCOBJECT_TYPE(ObjectKind::exceptionType, ExceptionType, ExceptionType);
DEFINE_GCOBJECT_TYPE(ObjectKind::moduleInstance, ModuleInstance, ModuleInstance);
DEFINE_GCOBJECT_TYPE(ObjectKind::context, Context, Context);
DEFINE_GCOBJECT_TYPE(ObjectKind::compartment, Compartment, Compartment);

DEFINE_OBJECT_TYPE(ObjectKind::function, Function, Function);
void Runtime::setUserData(Runtime::Function* function, void* userData)
{
	function->mutableData->userData = userData;
}
void* Runtime::getUserData(const Runtime::Function* function)
{
	return function->mutableData->userData;
}

bool Runtime::isA(Object* object, const ExternType& type)
{
	if(ObjectKind(type.kind) != object->kind) { return false; }

	switch(type.kind)
	{
	case ExternKind::function: return asFunction(object)->encodedType == asFunctionType(type);
	case ExternKind::global: return isSubtype(asGlobal(object)->type, asGlobalType(type));
	case ExternKind::table: return isSubtype(asTable(object)->type, asTableType(type));
	case ExternKind::memory: return isSubtype(asMemory(object)->type, asMemoryType(type));
	case ExternKind::exceptionType:
		return isSubtype(asExceptionType(type).params, asExceptionType(object)->sig.params);
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
	case ObjectKind::exceptionType: return asExceptionType(object)->sig;
	default: Errors::unreachable();
	};
}

FunctionType Runtime::getFunctionType(Function* function) { return function->encodedType; }

Context* Runtime::getContextFromRuntimeData(ContextRuntimeData* contextRuntimeData)
{
	const CompartmentRuntimeData* compartmentRuntimeData
		= getCompartmentRuntimeData(contextRuntimeData);
	const Uptr contextId = contextRuntimeData - compartmentRuntimeData->contexts;
	Lock<Platform::Mutex> compartmentLock(compartmentRuntimeData->compartment->mutex);
	return compartmentRuntimeData->compartment->contexts[contextId];
}

Compartment* Runtime::getCompartmentFromContextRuntimeData(
	struct ContextRuntimeData* contextRuntimeData)
{
	const CompartmentRuntimeData* compartmentRuntimeData
		= getCompartmentRuntimeData(contextRuntimeData);
	return compartmentRuntimeData->compartment;
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

Table* Runtime::getTableFromRuntimeData(ContextRuntimeData* contextRuntimeData, Uptr tableId)
{
	Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	wavmAssert(compartment->tables.contains(tableId));
	return compartment->tables[tableId];
}

Memory* Runtime::getMemoryFromRuntimeData(ContextRuntimeData* contextRuntimeData, Uptr memoryId)
{
	Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	return compartment->memories[memoryId];
}
