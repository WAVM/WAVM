#include "Core/Core.h"
#include "Runtime.h"
#include "Core/Platform.h"

namespace Runtime
{
	uint8* instanceMemoryBase = nullptr;
	size_t instanceAddressSpaceMaxBytes = 0;

	uint8* unalignedInstanceMemoryBase = nullptr;

	static size_t numCommittedVirtualPages = 0;
	static uint32 numAllocatedBytes = 0;

	bool initInstanceMemory(size_t maxBytes)
	{
		assert(!instanceMemoryBase);
		assert(!instanceAddressSpaceMaxBytes);

		// Allocate 4TB of address space for the instance. This is a tradeoff:
		// - Windows 8+ and Linux user processes can allocate 128TB of virtual memory.
		// - Windows 7 user processes can allocate 8TB of virtual memory.
		// - Windows (haven't checked on Linux) allocates a fair amount of physical memory
		//   for memory management data structures: 128MB for 64TB.
		const size_t addressSpaceMaxBytes = 4ull*1024*1024*1024*1024;
		if(maxBytes > addressSpaceMaxBytes) { return false; }

		instanceAddressSpaceMaxBytes = maxBytes;

		// Align the instance memory base to a 4GB boundary, so the lower 32-bits will all be zero. Maybe it will allow better code generation?
		const size_t numAllocatedVirtualPages = addressSpaceMaxBytes >> Platform::getPreferredVirtualPageSizeLog2();
		const size_t alignment = 4ull*1024*1024*1024;
		const size_t pageAlignment = alignment >> Platform::getPreferredVirtualPageSizeLog2();
		unalignedInstanceMemoryBase = Platform::allocateVirtualPages(numAllocatedVirtualPages + pageAlignment - 1);
		if(!unalignedInstanceMemoryBase) { return false; }
		instanceMemoryBase = (uint8*)((uintptr_t)(unalignedInstanceMemoryBase + alignment - 1) & ~(alignment - 1));
		return true;
	}

	uint32 vmSbrk(int32 numBytes)
	{
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
			const size_t numNewPages = numDesiredPages - numCommittedVirtualPages;
			if(numNewPages > 0)
			{
				bool successfullyCommittedPhysicalMemory = Platform::commitVirtualPages(instanceMemoryBase + (numCommittedVirtualPages << pageSizeLog2),numNewPages);
				if(!successfullyCommittedPhysicalMemory)
				{
					return (uint32)-1;
				}
				numAllocatedBytes += numBytes;
				numCommittedVirtualPages += numNewPages;
			}
		}
		else if(numBytes < 0)
		{
			numAllocatedBytes -= numBytes;
		}
		return (int32)existingNumBytes;
	}
}