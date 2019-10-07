#include "WAVM/Runtime/Runtime.h"
#include "RuntimePrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/RWMutex.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

#define DEFINE_OBJECT_TYPE(kindId, kindName, Type)                                                 \
	Runtime::Type* Runtime::as##kindName(Object* object)                                           \
	{                                                                                              \
		WAVM_ASSERT(!object || object->kind == kindId);                                            \
		return (Runtime::Type*)object;                                                             \
	}                                                                                              \
	Runtime::Type* Runtime::as##kindName##Nullable(Object* object)                                 \
	{                                                                                              \
		return object && object->kind == kindId ? (Runtime::Type*)object : nullptr;                \
	}                                                                                              \
	const Runtime::Type* Runtime::as##kindName(const Object* object)                               \
	{                                                                                              \
		WAVM_ASSERT(!object || object->kind == kindId);                                            \
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
	void Runtime::setUserData(Runtime::Type* object, void* userData, void (*finalizer)(void*))     \
	{                                                                                              \
		object->userData = userData;                                                               \
		object->finalizeUserData = finalizer;                                                      \
	}                                                                                              \
	void* Runtime::getUserData(const Runtime::Type* object) { return object->userData; }

DEFINE_GCOBJECT_TYPE(ObjectKind::table, Table, Table);
DEFINE_GCOBJECT_TYPE(ObjectKind::memory, Memory, Memory);
DEFINE_GCOBJECT_TYPE(ObjectKind::global, Global, Global);
DEFINE_GCOBJECT_TYPE(ObjectKind::exceptionType, ExceptionType, ExceptionType);
DEFINE_GCOBJECT_TYPE(ObjectKind::moduleInstance, ModuleInstance, ModuleInstance);
DEFINE_GCOBJECT_TYPE(ObjectKind::context, Context, Context);
DEFINE_GCOBJECT_TYPE(ObjectKind::compartment, Compartment, Compartment);
DEFINE_GCOBJECT_TYPE(ObjectKind::foreign, Foreign, Foreign);

DEFINE_OBJECT_TYPE(ObjectKind::function, Function, Function);
void Runtime::setUserData(Runtime::Function* function, void* userData, void (*finalizer)(void*))
{
	function->mutableData->userData = userData;
	function->mutableData->finalizeUserData = finalizer;
}
void* Runtime::getUserData(const Runtime::Function* function)
{
	return function->mutableData->userData;
}

void Runtime::setUserData(Runtime::Object* object, void* userData, void (*finalizer)(void*))
{
	if(object->kind == ObjectKind::function)
	{ setUserData(asFunction(object), userData, finalizer); }
	else
	{
		auto gcObject = (GCObject*)object;
		gcObject->userData = userData;
		gcObject->finalizeUserData = finalizer;
	}
}

void* Runtime::getUserData(const Runtime::Object* object)
{
	if(object->kind == ObjectKind::function) { return getUserData(asFunction(object)); }
	else
	{
		auto gcObject = (GCObject*)object;
		return gcObject->userData;
	}
}

Runtime::GCObject::~GCObject()
{
	WAVM_ASSERT(numRootReferences.load(std::memory_order_acquire) == 0);
	if(finalizeUserData) { (*finalizeUserData)(userData); }
}

bool Runtime::isA(const Object* object, const ExternType& type)
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

	case ExternKind::invalid:
	default: WAVM_UNREACHABLE();
	}
}

ExternType Runtime::getExternType(const Object* object)
{
	switch(object->kind)
	{
	case ObjectKind::function: return FunctionType(asFunction(object)->encodedType);
	case ObjectKind::global: return asGlobal(object)->type;
	case ObjectKind::table: return asTable(object)->type;
	case ObjectKind::memory: return asMemory(object)->type;
	case ObjectKind::exceptionType: return asExceptionType(object)->sig;

	case ObjectKind::moduleInstance:
	case ObjectKind::context:
	case ObjectKind::compartment:
	case ObjectKind::foreign:
	case ObjectKind::invalid:
	default: WAVM_UNREACHABLE();
	};
}

FunctionType Runtime::getFunctionType(const Function* function) { return function->encodedType; }

Context* Runtime::getContextFromRuntimeData(ContextRuntimeData* contextRuntimeData)
{
	return contextRuntimeData->context;
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
	Platform::RWMutex::ShareableLock compartmentLock(compartment->mutex);
	WAVM_ASSERT(compartment->moduleInstances.contains(moduleInstanceId));
	return compartment->moduleInstances[moduleInstanceId];
}

Table* Runtime::getTableFromRuntimeData(ContextRuntimeData* contextRuntimeData, Uptr tableId)
{
	Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
	Platform::RWMutex::ShareableLock compartmentLock(compartment->mutex);
	WAVM_ASSERT(compartment->tables.contains(tableId));
	return compartment->tables[tableId];
}

Memory* Runtime::getMemoryFromRuntimeData(ContextRuntimeData* contextRuntimeData, Uptr memoryId)
{
	Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
	Platform::RWMutex::ShareableLock compartmentLock(compartment->mutex);
	return compartment->memories[memoryId];
}

Foreign* Runtime::createForeign(Compartment* compartment, void* userData, void (*finalizer)(void*))
{
	Foreign* foreign = new Foreign(compartment);
	setUserData(foreign, userData, finalizer);
	return foreign;
}
