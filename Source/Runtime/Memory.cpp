#include "Core/Core.h"
#include "Runtime.h"
#include "Core/Platform.h"

namespace Runtime
{
	bool instanceMemoryInitialized = false;
	uint8* instanceMemoryBase = nullptr;
	size_t instanceAddressSpaceMaxBytes = 0;

	uint8* unalignedInstanceMemoryBase = nullptr;

	static size_t numCommittedVirtualPages = 0;
	static uint32 numAllocatedBytes = 0;

	bool initInstanceMemory()
	{
		numCommittedVirtualPages = 0;
		numAllocatedBytes = 0;
		if(!instanceMemoryInitialized)
		{
#if INTPTR_MAX == INT32_MAX
			const size_t addressSpaceMaxBytes = 0x40000000;
			instanceAddressSpaceMaxBytes = addressSpaceMaxBytes;
			const size_t numAllocatedVirtualPages = addressSpaceMaxBytes >> Platform::getPreferredVirtualPageSizeLog2();
			unalignedInstanceMemoryBase = Platform::allocateVirtualPages(numAllocatedVirtualPages);
			if(!unalignedInstanceMemoryBase) { return false; }
			instanceMemoryBase = unalignedInstanceMemoryBase;
#else
			// Allocate 4TB of address space for the instance. This is a tradeoff:
			// - Windows 8+ and Linux user processes can allocate 128TB of virtual memory.
			// - Windows 7 user processes can allocate 8TB of virtual memory.
			// - Windows (haven't checked on Linux) allocates a fair amount of physical memory
			//   for memory management data structures: 128MB for 64TB.
			const size_t addressSpaceMaxBytes = 4ull*1024*1024*1024*1024;
			instanceAddressSpaceMaxBytes = addressSpaceMaxBytes;

			// Align the instance memory base to a 4GB boundary, so the lower 32-bits will all be zero. Maybe it will allow better code generation?
			// Note that this reserves a full extra 4GB, but only uses (4GB-1 page) for alignment, so there will always be a guard page at the end to
			// protect against unaligned loads/stores that straddle the end of the address-space.
			const size_t numAllocatedVirtualPages = addressSpaceMaxBytes >> Platform::getPreferredVirtualPageSizeLog2();
			const size_t alignment = 4ull*1024*1024*1024;
			const size_t pageAlignment = alignment >> Platform::getPreferredVirtualPageSizeLog2();
			unalignedInstanceMemoryBase = Platform::allocateVirtualPages(numAllocatedVirtualPages + pageAlignment);
			if(!unalignedInstanceMemoryBase) { return false; }
			instanceMemoryBase = (uint8*)((uintptr)(unalignedInstanceMemoryBase + alignment - 1) & ~(alignment - 1));
#endif
			instanceMemoryInitialized = true;
		}
		return true;
	}

	uint32 vmSbrk(int32 numBytes)
	{
		// Round up to an alignment boundary.
		numBytes = (numBytes + 7) & ~7;
		const uint32 existingNumBytes = numAllocatedBytes;
		if(numBytes > 0)
		{
			if(uint64(existingNumBytes) + numBytes > (1ull<<32))
			{
				return (uint32)-1;
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
					return (uint32)-1;
				}
				numCommittedVirtualPages += deltaPages;
			}
			numAllocatedBytes += numBytes;
		}
		else if(numBytes < 0)
		{
			numAllocatedBytes += numBytes;
		}
		return (int32)existingNumBytes;
	}
}
