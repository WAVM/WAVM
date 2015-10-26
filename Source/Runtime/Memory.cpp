#include "Core/Core.h"
#include "Runtime.h"
#include "Core/Platform.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	bool instanceMemoryInitialized = false;
	uint8* instanceMemoryBase = nullptr;
	size_t instanceAddressSpaceMaxBytes = 0;

	uint8* unalignedInstanceMemoryBase = nullptr;

	static size_t numCommittedVirtualPages = 0;
	static size_t numAllocatedBytes = 0;

	bool initInstanceMemory()
	{
		numCommittedVirtualPages = 0;
		numAllocatedBytes = 0;
		if(!instanceMemoryInitialized)
		{
		        // On a 64 runtime, allocate 4TB of address space for the instance. This is a tradeoff:
			// - Windows 8+ and Linux user processes can allocate 128TB of virtual memory.
			// - Windows 7 user processes can allocate 8TB of virtual memory.
			// - Windows (haven't checked on Linux) allocates a fair amount of physical memory
			//   for memory management data structures: 128MB for 64TB.
			const size_t addressSpaceMaxBytes = sizeof(uintptr) == 8 ? 4ull*1024*1024*1024*1024 : 0x40000000;
			instanceAddressSpaceMaxBytes = addressSpaceMaxBytes;

			// On a 64 bit runtime, align the instance memory base to a 4GB boundary, so the lower 32-bits will all be zero. Maybe it will allow better code generation?
			// Note that this reserves a full extra 4GB, but only uses (4GB-1 page) for alignment, so there will always be a guard page at the end to
			// protect against unaligned loads/stores that straddle the end of the address-space.
			const size_t numAllocatedVirtualPages = addressSpaceMaxBytes >> Platform::getPreferredVirtualPageSizeLog2();
			const size_t alignment = sizeof(uintptr) == 8 ? 4ull*1024*1024*1024 : (uintptr)1 << Platform::getPreferredVirtualPageSizeLog2();
			const size_t pageAlignment = alignment >> Platform::getPreferredVirtualPageSizeLog2();
			unalignedInstanceMemoryBase = Platform::allocateVirtualPages(numAllocatedVirtualPages + pageAlignment);
			if(!unalignedInstanceMemoryBase) { return false; }
			instanceMemoryBase = (uint8*)((uintptr)(unalignedInstanceMemoryBase + alignment - 1) & ~(alignment - 1));

			instanceMemoryInitialized = true;
		}
		return true;
	}

	size_t vmGrowMemory(size_t numBytes)
	{
		// Round up to an alignment boundary.
		numBytes = (numBytes + 7) & ~7;
		const size_t existingNumBytes = numAllocatedBytes;
		if(uint64(existingNumBytes) + numBytes > (1ull<<32))
		{
			causeException(Exception::Cause::OutOfMemory);
		}

		const uint32 pageSizeLog2 = Platform::getPreferredVirtualPageSizeLog2();
		const uint32 pageSize = 1ull << pageSizeLog2;
		const size_t numDesiredPages = (numAllocatedBytes + numBytes + pageSize - 1) >> pageSizeLog2;
		const intptr deltaPages = numDesiredPages - numCommittedVirtualPages;
		if(deltaPages > 0)
		{
			bool successfullyCommittedPhysicalMemory = Platform::commitVirtualPages(instanceMemoryBase + (numCommittedVirtualPages << pageSizeLog2),deltaPages);
			if(!successfullyCommittedPhysicalMemory)
			{
				causeException(Exception::Cause::OutOfMemory);
			}
			numCommittedVirtualPages += deltaPages;
		}
		numAllocatedBytes += numBytes;
		return existingNumBytes;
	}

	size_t vmShrinkMemory(size_t numBytes)
	{
		// Round up to an alignment boundary.
		numBytes = (numBytes + 7) & ~7;
		const size_t existingNumBytes = numAllocatedBytes;
		numAllocatedBytes -= numBytes;
		return existingNumBytes;
	}
}
