#include <stddef.h>
#include <atomic>
#include <memory>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::Runtime;

Runtime::Compartment::Compartment()
: GCObject(ObjectKind::compartment, this)
, unalignedRuntimeData(nullptr)
, moduleInstances(0, UINTPTR_MAX - 1) // Use UINTPTR_MAX as an invalid index.
, memories(0, maxMemories - 1)
, tables(0, maxTables - 1)
, contexts(0, maxContexts - 1)
, exceptionTypes(0, UINTPTR_MAX - 1) // Use UINTPTR_MAX as an invalid index.
{
	runtimeData = (CompartmentRuntimeData*)Platform::allocateAlignedVirtualPages(
		compartmentReservedBytes >> Platform::getPageSizeLog2(),
		compartmentRuntimeDataAlignmentLog2,
		unalignedRuntimeData);

	errorUnless(Platform::commitVirtualPages(
		(U8*)runtimeData,
		offsetof(CompartmentRuntimeData, contexts) >> Platform::getPageSizeLog2()));

	runtimeData->compartment = this;
}

Runtime::Compartment::~Compartment()
{
	Lock<Platform::Mutex> compartmentLock(mutex);

	wavmAssert(!memories.size());
	wavmAssert(!tables.size());
	wavmAssert(!exceptionTypes.size());
	wavmAssert(!globals.size());
	wavmAssert(!moduleInstances.size());
	wavmAssert(!contexts.size());

	Platform::freeAlignedVirtualPages(unalignedRuntimeData,
									  compartmentReservedBytes >> Platform::getPageSizeLog2(),
									  compartmentRuntimeDataAlignmentLog2);
	runtimeData = nullptr;
	unalignedRuntimeData = nullptr;
}

Compartment* Runtime::createCompartment() { return new Compartment; }

Compartment* Runtime::cloneCompartment(const Compartment* compartment)
{
	Compartment* newCompartment = new Compartment;

	Lock<Platform::Mutex> compartmentLock(compartment->mutex);

	// Clone globals.
	newCompartment->globalDataAllocationMask = compartment->globalDataAllocationMask;
	for(Global* global : compartment->globals)
	{
		Global* newGlobal = cloneGlobal(global, newCompartment);
		wavmAssert(newGlobal->mutableGlobalId == global->mutableGlobalId);
	}

	// Clone memories.
	for(Memory* memory : compartment->memories)
	{
		Memory* newMemory = cloneMemory(memory, newCompartment);
		wavmAssert(newMemory->id == memory->id);
	}

	// Clone tables.
	for(Table* table : compartment->tables)
	{
		Table* newTable = cloneTable(table, newCompartment);
		wavmAssert(newTable->id == table->id);
	}

	return newCompartment;
}

Uptr Runtime::getCompartmentModuleInstanceId(ModuleInstance* moduleInstance)
{
	return moduleInstance->id;
}
Uptr Runtime::getCompartmentTableId(const Table* table) { return table->id; }
Uptr Runtime::getCompartmentMemoryId(const Memory* memory) { return memory->id; }

ModuleInstance* Runtime::getCompartmentModuleInstanceById(const Compartment* compartment,
														  Uptr moduleInstanceId)
{
	Lock<Platform::Mutex> lock(compartment->mutex);
	return compartment->moduleInstances[moduleInstanceId];
}
Table* Runtime::getCompartmentTableById(const Compartment* compartment, Uptr tableId)
{
	Lock<Platform::Mutex> lock(compartment->mutex);
	return compartment->tables[tableId];
}
Memory* Runtime::getCompartmentMemoryById(const Compartment* compartment, Uptr memoryId)
{
	Lock<Platform::Mutex> lock(compartment->mutex);
	return compartment->memories[memoryId];
}

bool Runtime::isInCompartment(Object* object, const Compartment* compartment)
{
	if(object->kind == ObjectKind::function)
	{
		// The function may be in multiple compartments, but if this compartment maps the function's
		// moduleInstanceId to a ModuleInstance with the LLVMJIT LoadedModule that contains this
		// function, then the function is in this compartment.
		Function* function = (Function*)object;

		// Treat functions with moduleInstanceId=UINTPTR_MAX as if they are in all compartments.
		if(function->moduleInstanceId == UINTPTR_MAX) { return true; }

		if(!compartment->moduleInstances.contains(function->moduleInstanceId)) { return false; }
		ModuleInstance* moduleInstance = compartment->moduleInstances[function->moduleInstanceId];
		return moduleInstance->jitModule.get() == function->mutableData->jitModule;
	}
	else
	{
		GCObject* gcObject = (GCObject*)object;
		return gcObject->compartment == compartment;
	}
}
