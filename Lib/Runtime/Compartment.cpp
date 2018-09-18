#include <stddef.h>
#include <atomic>
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
: ObjectImplWithAnyRef(ObjectKind::compartment)
, unalignedRuntimeData(nullptr)
, memories(0, maxMemories)
, tables(0, maxTables)
, contexts(0, maxContexts)
{
	runtimeData = (CompartmentRuntimeData*)Platform::allocateAlignedVirtualPages(
		compartmentReservedBytes >> Platform::getPageSizeLog2(),
		compartmentRuntimeDataAlignmentLog2,
		unalignedRuntimeData);

	errorUnless(Platform::commitVirtualPages(
		(U8*)runtimeData,
		offsetof(CompartmentRuntimeData, contexts) >> Platform::getPageSizeLog2()));

	runtimeData->compartment = this;

	wavmIntrinsics = instantiateWAVMIntrinsics(this);
}

Runtime::Compartment::~Compartment()
{
	Platform::decommitVirtualPages((U8*)runtimeData,
								   compartmentReservedBytes >> Platform::getPageSizeLog2());
	Platform::freeAlignedVirtualPages(unalignedRuntimeData,
									  compartmentReservedBytes >> Platform::getPageSizeLog2(),
									  compartmentRuntimeDataAlignmentLog2);
	runtimeData = nullptr;
	unalignedRuntimeData = nullptr;
}

Compartment* Runtime::createCompartment() { return new Compartment(); }

Compartment* Runtime::cloneCompartment(Compartment* compartment)
{
	Compartment* newCompartment = new Compartment;

	Lock<Platform::Mutex> lock(compartment->mutex);

	// Clone globals.
	newCompartment->globalDataAllocationMask = compartment->globalDataAllocationMask;
	for(GlobalInstance* global : compartment->globals)
	{
		GlobalInstance* newGlobal = cloneGlobal(global, newCompartment);
		wavmAssert(newGlobal->mutableGlobalId == global->mutableGlobalId);
	}

	// Clone memories.
	for(MemoryInstance* memory : compartment->memories)
	{
		MemoryInstance* newMemory = cloneMemory(memory, newCompartment);
		wavmAssert(newMemory->id == memory->id);
	}

	// Clone tables.
	for(TableInstance* table : compartment->tables)
	{
		TableInstance* newTable = cloneTable(table, newCompartment);
		wavmAssert(newTable->id == table->id);
	}

	return newCompartment;
}

Uptr Runtime::getCompartmentTableId(const TableInstance* table) { return table->id; }

Uptr Runtime::getCompartmentMemoryId(const MemoryInstance* memory) { return memory->id; }

TableInstance* Runtime::getCompartmentTableById(const Compartment* compartment, Uptr tableId)
{
	Lock<Platform::Mutex> lock(compartment->mutex);
	return compartment->tables[tableId];
}
MemoryInstance* Runtime::getCompartmentMemoryById(const Compartment* compartment, Uptr memoryId)
{
	Lock<Platform::Mutex> lock(compartment->mutex);
	return compartment->memories[memoryId];
}
