#include <stdint.h>
#include <string.h>
#include <atomic>
#include <memory>
#include <vector>

#include "IR/IR.h"
#include "IR/Types.h"
#include "IR/Value.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Lock.h"
#include "Platform/Intrinsic.h"
#include "Platform/Memory.h"
#include "Platform/Mutex.h"
#include "Runtime/Runtime.h"
#include "Runtime/RuntimeData.h"
#include "RuntimePrivate.h"

using namespace Runtime;

// Global lists of memories; used to query whether an address is reserved by one of them.
static Platform::Mutex memoriesMutex;
static std::vector<MemoryInstance*> memories;

enum
{
	numGuardPages = 1
};

static Uptr getPlatformPagesPerWebAssemblyPageLog2()
{
	errorUnless(Platform::getPageSizeLog2() <= IR::numBytesPerPageLog2);
	return IR::numBytesPerPageLog2 - Platform::getPageSizeLog2();
}

static MemoryInstance* createMemoryImpl(Compartment* compartment,
										IR::MemoryType type,
										Uptr numPages)
{
	MemoryInstance* memory = new MemoryInstance(compartment, type);

	// On a 64-bit runtime, allocate 8GB of address space for the memory.
	// This allows eliding bounds checks on memory accesses, since a 32-bit index + 32-bit offset
	// will always be within the reserved address-space.
	const Uptr pageBytesLog2 = Platform::getPageSizeLog2();
	const Uptr memoryMaxBytes = Uptr(8ull * 1024 * 1024 * 1024);
	const Uptr memoryMaxPages = memoryMaxBytes >> pageBytesLog2;

	memory->baseAddress = Platform::allocateVirtualPages(memoryMaxPages + numGuardPages);
	memory->numReservedBytes = memoryMaxBytes;
	if(!memory->baseAddress)
	{
		delete memory;
		return nullptr;
	}

	// Grow the memory to the type's minimum size.
	if(growMemory(memory, numPages) == -1)
	{
		delete memory;
		return nullptr;
	}

	// Add the memory to the global array.
	{
		Lock<Platform::Mutex> memoriesLock(memoriesMutex);
		memories.push_back(memory);
	}

	return memory;
}

MemoryInstance* Runtime::createMemory(Compartment* compartment, IR::MemoryType type)
{
	wavmAssert(type.size.min <= UINTPTR_MAX);
	MemoryInstance* memory = createMemoryImpl(compartment, type, Uptr(type.size.min));
	if(!memory) { return nullptr; }

	// Add the memory to the compartment's memories IndexMap.
	{
		Lock<Platform::Mutex> compartmentLock(compartment->mutex);

		memory->id = compartment->memories.add(UINTPTR_MAX, memory);
		if(memory->id == UINTPTR_MAX)
		{
			delete memory;
			return nullptr;
		}
		compartment->runtimeData->memoryBases[memory->id] = memory->baseAddress;
	}

	return memory;
}

MemoryInstance* Runtime::cloneMemory(MemoryInstance* memory, Compartment* newCompartment)
{
	MemoryInstance* newMemory = createMemoryImpl(newCompartment, memory->type, memory->numPages);
	if(!newMemory) { return nullptr; }

	// Insert the memory in the new compartment's memories array with the same index as it had in
	// the original compartment's memories IndexMap.
	{
		Lock<Platform::Mutex> compartmentLock(newCompartment->mutex);

		newMemory->id = memory->id;
		newCompartment->memories.insertOrFail(newMemory->id, newMemory);
		newCompartment->runtimeData->memoryBases[newMemory->id] = newMemory->baseAddress;
	}

	return newMemory;
}

void Runtime::MemoryInstance::finalize()
{
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);

	wavmAssert(compartment->memories[id] == this);
	compartment->memories.removeOrFail(id);

	wavmAssert(compartment->runtimeData->memoryBases[id] == baseAddress);
	compartment->runtimeData->memoryBases[id] = nullptr;
}

Runtime::MemoryInstance::~MemoryInstance()
{
	// Decommit all default memory pages.
	if(numPages > 0)
	{
		Platform::decommitVirtualPages(baseAddress,
									   numPages << getPlatformPagesPerWebAssemblyPageLog2());
	}

	// Free the virtual address space.
	const Uptr pageBytesLog2 = Platform::getPageSizeLog2();
	if(numReservedBytes > 0)
	{
		Platform::freeVirtualPages(baseAddress,
								   (numReservedBytes >> pageBytesLog2) + numGuardPages);
	}
	baseAddress = nullptr;

	// Remove the memory from the global array.
	{
		Lock<Platform::Mutex> memoriesLock(memoriesMutex);
		for(Uptr memoryIndex = 0; memoryIndex < memories.size(); ++memoryIndex)
		{
			if(memories[memoryIndex] == this)
			{
				memories.erase(memories.begin() + memoryIndex);
				break;
			}
		}
	}
}

bool Runtime::isAddressOwnedByMemory(U8* address)
{
	// Iterate over all memories and check if the address is within the reserved address space for
	// each.
	Lock<Platform::Mutex> memoriesLock(memoriesMutex);
	for(auto memory : memories)
	{
		U8* startAddress = memory->baseAddress;
		U8* endAddress = memory->baseAddress + memory->numReservedBytes;
		if(address >= startAddress && address < endAddress) { return true; }
	}
	return false;
}

Uptr Runtime::getMemoryNumPages(MemoryInstance* memory) { return memory->numPages; }
Uptr Runtime::getMemoryMaxPages(MemoryInstance* memory)
{
	wavmAssert(memory->type.size.max <= UINTPTR_MAX);
	return Uptr(memory->type.size.max);
}

Iptr Runtime::growMemory(MemoryInstance* memory, Uptr numNewPages)
{
	const Uptr previousNumPages = memory->numPages;
	if(numNewPages > 0)
	{
		// If the number of pages to grow would cause the memory's size to exceed its maximum,
		// return -1.
		if(numNewPages > memory->type.size.max
		   || memory->numPages > memory->type.size.max - numNewPages)
		{ return -1; }

		// Try to commit the new pages, and return -1 if the commit fails.
		if(!Platform::commitVirtualPages(
			   memory->baseAddress + (memory->numPages << IR::numBytesPerPageLog2),
			   numNewPages << getPlatformPagesPerWebAssemblyPageLog2()))
		{ return -1; }
		memory->numPages += numNewPages;
	}
	return previousNumPages;
}

Iptr Runtime::shrinkMemory(MemoryInstance* memory, Uptr numPagesToShrink)
{
	const Uptr previousNumPages = memory->numPages;
	if(numPagesToShrink > 0)
	{
		// If the number of pages to shrink would cause the memory's size to drop below its minimum,
		// return -1.
		if(numPagesToShrink > memory->numPages
		   || memory->numPages - numPagesToShrink < memory->type.size.min)
		{ return -1; }
		memory->numPages -= numPagesToShrink;

		// Decommit the pages that were shrunk off the end of the memory.
		Platform::decommitVirtualPages(
			memory->baseAddress + (memory->numPages << IR::numBytesPerPageLog2),
			numPagesToShrink << getPlatformPagesPerWebAssemblyPageLog2());
	}
	return previousNumPages;
}

void Runtime::unmapMemoryPages(MemoryInstance* memory, Uptr pageIndex, Uptr numPages)
{
	wavmAssert(pageIndex < memory->numPages);
	wavmAssert(pageIndex + numPages > pageIndex);
	wavmAssert(pageIndex + numPages < memory->numPages);

	// Decommit the pages.
	Platform::decommitVirtualPages(memory->baseAddress + (pageIndex << IR::numBytesPerPageLog2),
								   numPages << getPlatformPagesPerWebAssemblyPageLog2());
}

U8* Runtime::getMemoryBaseAddress(MemoryInstance* memory) { return memory->baseAddress; }

static U8* getValidatedMemoryOffsetRangeImpl(U8* memoryBase,
											 Uptr memoryNumBytes,
											 Uptr address,
											 Uptr numBytes)
{
	wavmAssert(memoryBase);

	numBytes = Platform::saturateToBounds(numBytes, memoryNumBytes);
	U8* pointer = memoryBase + Platform::saturateToBounds(address, memoryNumBytes - numBytes);
	if(pointer < memoryBase || pointer + numBytes < pointer
	   || pointer + numBytes > memoryBase + memoryNumBytes)
	{ throwException(Exception::accessViolationType, {}); }
	return pointer;
}

U8* Runtime::getReservedMemoryOffsetRange(MemoryInstance* memory, Uptr address, Uptr numBytes)
{
	wavmAssert(memory);

	// Validate that the range [offset..offset+numBytes) is contained by the memory's reserved
	// pages.
	return ::getValidatedMemoryOffsetRangeImpl(
		memory->baseAddress, memory->numReservedBytes, address, numBytes);
}

U8* Runtime::getValidatedMemoryOffsetRange(MemoryInstance* memory, Uptr address, Uptr numBytes)
{
	wavmAssert(memory);

	// Validate that the range [offset..offset+numBytes) is contained by the memory's committed
	// pages.
	return ::getValidatedMemoryOffsetRangeImpl(
		memory->baseAddress, memory->numPages << IR::numBytesPerPageLog2, address, numBytes);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "memory.grow",
						  I32,
						  memory_grow,
						  I32 deltaPages,
						  I64 memoryId)
{
	MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);
	wavmAssert(memory);
	if(getMemoryNumPages(memory) + Uptr(deltaPages) > IR::maxMemoryPages) { return -1; }
	const Iptr numPreviousMemoryPages = growMemory(memory, (Uptr)deltaPages);
	wavmAssert(numPreviousMemoryPages < INT32_MAX);
	return I32(numPreviousMemoryPages);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "memory.size", I32, memory_size, I64 memoryId)
{
	MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);
	wavmAssert(memory);
	Uptr numMemoryPages = getMemoryNumPages(memory);
	if(numMemoryPages > UINT32_MAX) { numMemoryPages = UINT32_MAX; }
	return (U32)numMemoryPages;
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "memory.init",
						  void,
						  memory_init,
						  U32 destAddress,
						  U32 sourceOffset,
						  U32 numBytes,
						  Uptr moduleInstanceBits,
						  Uptr memoryId,
						  Uptr dataSegmentIndex)
{
	ModuleInstance* moduleInstance = reinterpret_cast<ModuleInstance*>(moduleInstanceBits);
	Lock<Platform::Mutex> passiveDataSegmentsLock(moduleInstance->passiveDataSegmentsMutex);

	if(!moduleInstance->passiveDataSegments.contains(dataSegmentIndex))
	{ throwException(Exception::invalidArgumentType); }
	else
	{
		// Copy the passive data segment shared_ptr, and unlock the mutex. It's important to
		// explicitly unlock the mutex before calling memcpy, as memcpy might trigger a signal that
		// will unwind the stack without calling the Lock destructor.
		std::shared_ptr<std::vector<U8>> passiveDataSegmentBytes
			= moduleInstance->passiveDataSegments[dataSegmentIndex];
		passiveDataSegmentsLock.unlock();

		MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);
		U8* destPointer = getReservedMemoryOffsetRange(memory, destAddress, numBytes);

		if(U64(sourceOffset) + U64(numBytes) > passiveDataSegmentBytes->size())
		{
			// If the source range is outside the bounds of the data segment, copy the part that is
			// in range, then trap.
			if(sourceOffset < passiveDataSegmentBytes->size())
			{
				Platform::bytewiseMemCopy(destPointer,
										  passiveDataSegmentBytes->data() + sourceOffset,
										  passiveDataSegmentBytes->size() - sourceOffset);
			}
			throwException(Exception::accessViolationType);
		}
		else if(numBytes)
		{
			Platform::bytewiseMemCopy(
				destPointer, passiveDataSegmentBytes->data() + sourceOffset, numBytes);
		}
	}
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "memory.drop",
						  void,
						  memory_drop,
						  Uptr moduleInstanceBits,
						  Uptr dataSegmentIndex)
{
	ModuleInstance* moduleInstance = reinterpret_cast<ModuleInstance*>(moduleInstanceBits);
	Lock<Platform::Mutex> passiveDataSegmentsLock(moduleInstance->passiveDataSegmentsMutex);

	if(!moduleInstance->passiveDataSegments.contains(dataSegmentIndex))
	{ throwException(Exception::invalidArgumentType); }
	else
	{
		moduleInstance->passiveDataSegments.removeOrFail(dataSegmentIndex);
	}
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "memory.copy",
						  void,
						  memory_copy,
						  U32 destAddress,
						  U32 sourceAddress,
						  U32 numBytes,
						  Uptr memoryId)
{
	MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);

	U8* destPointer = getReservedMemoryOffsetRange(memory, destAddress, numBytes);
	U8* sourcePointer = getReservedMemoryOffsetRange(memory, sourceAddress, numBytes);
	if(numBytes) { Platform::bytewiseMemMove(destPointer, sourcePointer, numBytes); }
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "memory.fill",
						  void,
						  memory_fill,
						  U32 destAddress,
						  U32 value,
						  U32 numBytes,
						  Uptr memoryId)
{
	MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);

	U8* destPointer = getReservedMemoryOffsetRange(memory, destAddress, numBytes);
	if(numBytes) { Platform::bytewiseMemSet(destPointer, U8(value), numBytes); }
}
