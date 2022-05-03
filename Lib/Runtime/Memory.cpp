#include "WAVM/Platform/Memory.h"
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#include "RuntimePrivate.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Intrinsic.h"
#include "WAVM/Platform/RWMutex.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

using namespace WAVM;
using namespace WAVM::Runtime;

namespace WAVM { namespace Runtime {
	WAVM_DEFINE_INTRINSIC_MODULE(wavmIntrinsicsMemory)
}}

// Global lists of memories; used to query whether an address is reserved by one of them.
static Platform::RWMutex memoriesMutex;
static std::vector<Memory*> memories;

static constexpr U64 maxMemory64WASMPages =
#if WAVM_ENABLE_TSAN
	(U64(8) * 1024 * 1024 * 1024) >> IR::numBytesPerPageLog2; // 8GB
#else
	(U64(1) * 1024 * 1024 * 1024 * 1024) >> IR::numBytesPerPageLog2; // 1TB
#endif

static Uptr getPlatformPagesPerWebAssemblyPageLog2()
{
	WAVM_ERROR_UNLESS(Platform::getBytesPerPageLog2() <= IR::numBytesPerPageLog2);
	return IR::numBytesPerPageLog2 - Platform::getBytesPerPageLog2();
}

static Memory* createMemoryImpl(Compartment* compartment,
								IR::MemoryType type,
								std::string&& debugName,
								ResourceQuotaRefParam resourceQuota)
{
	Memory* memory = new Memory(compartment, type, std::move(debugName), resourceQuota);

	const Uptr pageBytesLog2 = Platform::getBytesPerPageLog2();

	Uptr memoryMaxPages;
	if(type.indexType == IR::IndexType::i32)
	{
		static_assert(sizeof(Uptr) == 8, "WAVM's runtime requires a 64-bit host");

		// For 32-bit memories on a 64-bit runtime, allocate 8GB of address space for the memory.
		// This allows eliding bounds checks on memory accesses, since a 32-bit index + 32-bit
		// offset will always be within the reserved address-space.
		memoryMaxPages = (Uptr(8) * 1024 * 1024 * 1024) >> pageBytesLog2;
	}
	else
	{
		// Clamp the maximum size of 64-bit memories to maxMemory64Bytes.
		memoryMaxPages = std::min(type.size.max, maxMemory64WASMPages);

		// Convert maxMemoryPages from fixed size WASM pages (64KB) to platform-specific pages.
		memoryMaxPages <<= getPlatformPagesPerWebAssemblyPageLog2();
	}

	const Uptr numGuardPages = memoryNumGuardBytes >> pageBytesLog2;
	memory->baseAddress = Platform::allocateVirtualPages(memoryMaxPages + numGuardPages);
	memory->numReservedBytes = memoryMaxPages << pageBytesLog2;
	if(!memory->baseAddress)
	{
		delete memory;
		return nullptr;
	}

	// Grow the memory to the type's minimum size.
	if(growMemory(memory, type.size.min) != GrowResult::success)
	{
		delete memory;
		return nullptr;
	}

	// Add the memory to the global array.
	{
		Platform::RWMutex::ExclusiveLock memoriesLock(memoriesMutex);
		memories.push_back(memory);
	}

	return memory;
}

Memory* Runtime::createMemory(Compartment* compartment,
							  IR::MemoryType type,
							  std::string&& debugName,
							  ResourceQuotaRefParam resourceQuota)
{
	WAVM_ASSERT(type.size.min <= UINTPTR_MAX);
	Memory* memory = createMemoryImpl(compartment, type, std::move(debugName), resourceQuota);
	if(!memory) { return nullptr; }

	// Add the memory to the compartment's memories IndexMap.
	{
		Platform::RWMutex::ExclusiveLock compartmentLock(compartment->mutex);

		memory->id = compartment->memories.add(UINTPTR_MAX, memory);
		if(memory->id == UINTPTR_MAX)
		{
			delete memory;
			return nullptr;
		}
		MemoryRuntimeData& runtimeData = compartment->runtimeData->memories[memory->id];
		runtimeData.base = memory->baseAddress;
		runtimeData.endAddress = memory->numReservedBytes;
		runtimeData.numPages.store(memory->numPages.load(std::memory_order_acquire),
								   std::memory_order_release);
	}

	return memory;
}

Memory* Runtime::cloneMemory(Memory* memory, Compartment* newCompartment)
{
	Platform::RWMutex::ExclusiveLock resizingLock(memory->resizingMutex);
	const IR::MemoryType memoryType = getMemoryType(memory);
	std::string debugName = memory->debugName;
	Memory* newMemory
		= createMemoryImpl(newCompartment, memoryType, std::move(debugName), memory->resourceQuota);
	if(!newMemory) { return nullptr; }

	// Copy the memory contents to the new memory.
	memcpy(newMemory->baseAddress, memory->baseAddress, memoryType.size.min * IR::numBytesPerPage);

	resizingLock.unlock();

	// Insert the memory in the new compartment's memories array with the same index as it had in
	// the original compartment's memories IndexMap.
	{
		Platform::RWMutex::ExclusiveLock compartmentLock(newCompartment->mutex);

		newMemory->id = memory->id;
		newCompartment->memories.insertOrFail(newMemory->id, newMemory);

		MemoryRuntimeData& runtimeData = newCompartment->runtimeData->memories[newMemory->id];
		runtimeData.base = newMemory->baseAddress;
		runtimeData.numPages.store(newMemory->numPages, std::memory_order_release);
		runtimeData.endAddress = newMemory->numReservedBytes;
	}

	return newMemory;
}

Runtime::Memory::~Memory()
{
	if(id != UINTPTR_MAX)
	{
		WAVM_ASSERT_RWMUTEX_IS_EXCLUSIVELY_LOCKED_BY_CURRENT_THREAD(compartment->mutex);

		WAVM_ASSERT(compartment->memories[id] == this);
		compartment->memories.removeOrFail(id);

		MemoryRuntimeData& runtimeData = compartment->runtimeData->memories[id];
		WAVM_ASSERT(runtimeData.base == baseAddress);
		runtimeData.base = nullptr;
		runtimeData.numPages.store(0, std::memory_order_release);
		runtimeData.endAddress = 0;
	}

	// Remove the memory from the global array.
	{
		Platform::RWMutex::ExclusiveLock memoriesLock(memoriesMutex);
		for(Uptr memoryIndex = 0; memoryIndex < memories.size(); ++memoryIndex)
		{
			if(memories[memoryIndex] == this)
			{
				memories.erase(memories.begin() + memoryIndex);
				break;
			}
		}
	}

	// Free the virtual address space.
	const Uptr pageBytesLog2 = Platform::getBytesPerPageLog2();
	if(baseAddress && numReservedBytes > 0)
	{
		Platform::freeVirtualPages(baseAddress,
								   (numReservedBytes + memoryNumGuardBytes) >> pageBytesLog2);

		Platform::deregisterVirtualAllocation(numPages >> pageBytesLog2);
	}

	// Free the allocated quota.
	if(resourceQuota) { resourceQuota->memoryPages.free(numPages); }
}

bool Runtime::isAddressOwnedByMemory(U8* address, Memory*& outMemory, Uptr& outMemoryAddress)
{
	// Iterate over all memories and check if the address is within the reserved address space for
	// each.
	Platform::RWMutex::ShareableLock memoriesLock(memoriesMutex);
	for(auto memory : memories)
	{
		U8* startAddress = memory->baseAddress;
		U8* endAddress = memory->baseAddress + memory->numReservedBytes + memoryNumGuardBytes;
		if(address >= startAddress && address < endAddress)
		{
			outMemory = memory;
			outMemoryAddress = address - startAddress;
			return true;
		}
	}
	return false;
}

Uptr Runtime::getMemoryNumPages(const Memory* memory)
{
	return memory->numPages.load(std::memory_order_seq_cst);
}
IR::MemoryType Runtime::getMemoryType(const Memory* memory)
{
	return IR::MemoryType(memory->isShared,
						  memory->indexType,
						  IR::SizeConstraints{getMemoryNumPages(memory), memory->maxPages});
}

GrowResult Runtime::growMemory(Memory* memory, Uptr numPagesToGrow, Uptr* outOldNumPages)
{
	Uptr oldNumPages;
	if(numPagesToGrow == 0) { oldNumPages = memory->numPages.load(std::memory_order_seq_cst); }
	else
	{
		// Check the memory page quota.
		if(memory->resourceQuota && !memory->resourceQuota->memoryPages.allocate(numPagesToGrow))
		{ return GrowResult::outOfQuota; }

		Platform::RWMutex::ExclusiveLock resizingLock(memory->resizingMutex);
		oldNumPages = memory->numPages.load(std::memory_order_acquire);

		// If the number of pages to grow would cause the memory's size to exceed its maximum,
		// return GrowResult::outOfMaxSize.
		const U64 maxMemoryPages = memory->indexType == IR::IndexType::i32
									   ? IR::maxMemory32Pages
									   : std::min(maxMemory64WASMPages, IR::maxMemory64Pages);
		if(numPagesToGrow > memory->maxPages || oldNumPages > memory->maxPages - numPagesToGrow
		   || numPagesToGrow > maxMemoryPages || oldNumPages > maxMemoryPages - numPagesToGrow)
		{
			if(memory->resourceQuota) { memory->resourceQuota->memoryPages.free(numPagesToGrow); }
			return GrowResult::outOfMaxSize;
		}

		// Try to commit the new pages, and return GrowResult::outOfMemory if the commit fails.
		if(!Platform::commitVirtualPages(
			   memory->baseAddress + oldNumPages * IR::numBytesPerPage,
			   numPagesToGrow << getPlatformPagesPerWebAssemblyPageLog2()))
		{
			if(memory->resourceQuota) { memory->resourceQuota->memoryPages.free(numPagesToGrow); }
			return GrowResult::outOfMemory;
		}
		Platform::registerVirtualAllocation(numPagesToGrow
											<< getPlatformPagesPerWebAssemblyPageLog2());

		const Uptr newNumPages = oldNumPages + numPagesToGrow;
		memory->numPages.store(newNumPages, std::memory_order_release);
		if(memory->id != UINTPTR_MAX)
		{
			memory->compartment->runtimeData->memories[memory->id].numPages.store(
				newNumPages, std::memory_order_release);
		}
	}

	if(outOldNumPages) { *outOldNumPages = oldNumPages; }
	return GrowResult::success;
}

void Runtime::unmapMemoryPages(Memory* memory, Uptr pageIndex, Uptr numPages)
{
	WAVM_ASSERT(pageIndex + numPages > pageIndex);
	WAVM_ASSERT((pageIndex + numPages) * IR::numBytesPerPage <= memory->numReservedBytes);

	// Decommit the pages.
	Platform::decommitVirtualPages(memory->baseAddress + pageIndex * IR::numBytesPerPage,
								   numPages << getPlatformPagesPerWebAssemblyPageLog2());

	Platform::deregisterVirtualAllocation(numPages << getPlatformPagesPerWebAssemblyPageLog2());
}

U8* Runtime::getMemoryBaseAddress(Memory* memory) { return memory->baseAddress; }

static U8* getValidatedMemoryOffsetRangeImpl(Memory* memory,
											 U8* memoryBase,
											 Uptr memoryNumBytes,
											 Uptr address,
											 Uptr numBytes)
{
	if(address + numBytes > memoryNumBytes || address + numBytes < address)
	{
		throwException(
			ExceptionTypes::outOfBoundsMemoryAccess,
			{asObject(memory), U64(address > memoryNumBytes ? address : memoryNumBytes)});
	}
	WAVM_ASSERT(memoryBase);
	numBytes = branchlessMin(numBytes, memoryNumBytes);
	return memoryBase + branchlessMin(address, memoryNumBytes - numBytes);
}

U8* Runtime::getReservedMemoryOffsetRange(Memory* memory, Uptr address, Uptr numBytes)
{
	WAVM_ASSERT(memory);

	// Validate that the range [offset..offset+numBytes) is contained by the memory's reserved
	// pages.
	return ::getValidatedMemoryOffsetRangeImpl(
		memory, memory->baseAddress, memory->numReservedBytes, address, numBytes);
}

U8* Runtime::getValidatedMemoryOffsetRange(Memory* memory, Uptr address, Uptr numBytes)
{
	WAVM_ASSERT(memory);

	// Validate that the range [offset..offset+numBytes) is contained by the memory's committed
	// pages.
	return ::getValidatedMemoryOffsetRangeImpl(
		memory,
		memory->baseAddress,
		memory->numPages.load(std::memory_order_acquire) * IR::numBytesPerPage,
		address,
		numBytes);
}

void Runtime::initDataSegment(Instance* instance,
							  Uptr dataSegmentIndex,
							  const std::vector<U8>* dataVector,
							  Memory* memory,
							  Uptr destAddress,
							  Uptr sourceOffset,
							  Uptr numBytes)
{
	U8* destPointer = getValidatedMemoryOffsetRange(memory, destAddress, numBytes);
	if(sourceOffset + numBytes > dataVector->size() || sourceOffset + numBytes < sourceOffset)
	{
		throwException(
			ExceptionTypes::outOfBoundsDataSegmentAccess,
			{asObject(instance),
			 U64(dataSegmentIndex),
			 U64(sourceOffset > dataVector->size() ? sourceOffset : dataVector->size())});
	}
	else
	{
		Runtime::unwindSignalsAsExceptions([destPointer, sourceOffset, numBytes, dataVector] {
			bytewiseMemCopy(destPointer, dataVector->data() + sourceOffset, numBytes);
		});
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsMemory,
							   "memory.grow",
							   Iptr,
							   memory_grow,
							   Uptr deltaPages,
							   Uptr memoryId)
{
	Memory* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);
	Uptr oldNumPages = 0;
	if(growMemory(memory, deltaPages, &oldNumPages) != GrowResult::success) { return -1; }
	WAVM_ASSERT(oldNumPages <= INTPTR_MAX);
	return Iptr(oldNumPages);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsMemory,
							   "memory.init",
							   void,
							   memory_init,
							   Uptr destAddress,
							   Uptr sourceOffset,
							   Uptr numBytes,
							   Uptr instanceId,
							   Uptr memoryId,
							   Uptr dataSegmentIndex)
{
	Instance* instance = getInstanceFromRuntimeData(contextRuntimeData, instanceId);
	Memory* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);

	Platform::RWMutex::ShareableLock dataSegmentsLock(instance->dataSegmentsMutex);
	if(!instance->dataSegments[dataSegmentIndex])
	{
		if(sourceOffset != 0 || numBytes != 0)
		{
			throwException(ExceptionTypes::outOfBoundsDataSegmentAccess,
						   {instance, dataSegmentIndex, sourceOffset});
		}
	}
	else
	{
		// Make a copy of the shared_ptr to the data and unlock the data segments mutex.
		std::shared_ptr<std::vector<U8>> dataVector = instance->dataSegments[dataSegmentIndex];
		dataSegmentsLock.unlock();

		initDataSegment(instance,
						dataSegmentIndex,
						dataVector.get(),
						memory,
						destAddress,
						sourceOffset,
						numBytes);
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsMemory,
							   "data.drop",
							   void,
							   data_drop,
							   Uptr instanceId,
							   Uptr dataSegmentIndex)
{
	Instance* instance = getInstanceFromRuntimeData(contextRuntimeData, instanceId);
	Platform::RWMutex::ExclusiveLock dataSegmentsLock(instance->dataSegmentsMutex);

	if(instance->dataSegments[dataSegmentIndex])
	{ instance->dataSegments[dataSegmentIndex].reset(); }
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
							   "memoryOutOfBoundsTrap",
							   void,
							   outOfBoundsMemoryTrap,
							   Uptr address,
							   Uptr numBytes,
							   Uptr memoryNumBytes,
							   Uptr memoryId)
{
	Compartment* compartment = getCompartmentFromContextRuntimeData(contextRuntimeData);
	Platform::RWMutex::ShareableLock compartmentLock(compartment->mutex);
	Memory* memory = compartment->memories[memoryId];
	compartmentLock.unlock();

	const U64 outOfBoundsAddress = U64(address) > memoryNumBytes ? U64(address) : memoryNumBytes;

	throwException(ExceptionTypes::outOfBoundsMemoryAccess, {memory, outOfBoundsAddress});
}
