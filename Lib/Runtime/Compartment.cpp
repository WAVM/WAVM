#include <stddef.h>
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

Runtime::Compartment::Compartment()
: ObjectImpl(ObjectKind::compartment), unalignedRuntimeData(nullptr), numGlobalBytes(0)
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
	for(GlobalInstance* global : compartment->globals)
	{
		GlobalInstance* newGlobal = cloneGlobal(global, newCompartment);
		wavmAssert(newGlobal->mutableDataOffset == global->mutableDataOffset);
	}
	wavmAssert(newCompartment->numGlobalBytes == compartment->numGlobalBytes);

	// Clone memories.
	for(Uptr memoryIndex = 0; memoryIndex < compartment->memories.size(); ++memoryIndex)
	{
		MemoryInstance* memory = compartment->memories[memoryIndex];
		MemoryInstance* newMemory = cloneMemory(memory, newCompartment);
		SUPPRESS_UNUSED(newMemory);
		wavmAssert(newMemory->id == memory->id);
	}

	// Clone tables.
	for(Uptr tableIndex = 0; tableIndex < compartment->tables.size(); ++tableIndex)
	{
		TableInstance* table = compartment->tables[tableIndex];
		TableInstance* newTable = cloneTable(table, newCompartment);
		SUPPRESS_UNUSED(newTable);
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
