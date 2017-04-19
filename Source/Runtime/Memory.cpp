#include "Core/Core.h"
#include "Runtime.h"
#include "Core/Platform.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	// Global lists of memories; used to query whether an address is reserved by one of them.
	std::vector<MemoryInstance*> memories;

	static uintp getPlatformPagesPerWebAssemblyPageLog2()
	{
		errorUnless(Platform::getPageSizeLog2() <= IR::numBytesPerPageLog2);
		return IR::numBytesPerPageLog2 - Platform::getPageSizeLog2();
	}

	uint8* allocateVirtualPagesAligned(size_t numBytes,size_t alignmentBytes,uint8*& outUnalignedBaseAddress,size_t& outUnalignedNumPlatformPages)
	{
		const size_t numAllocatedVirtualPages = numBytes >> Platform::getPageSizeLog2();
		const size_t alignmentPages = alignmentBytes >> Platform::getPageSizeLog2();
		outUnalignedNumPlatformPages = numAllocatedVirtualPages + alignmentPages;
		outUnalignedBaseAddress = Platform::allocateVirtualPages(outUnalignedNumPlatformPages);
		if(!outUnalignedBaseAddress) { return nullptr; }
		else { return (uint8*)((uintp)(outUnalignedBaseAddress + alignmentBytes - 1) & ~(alignmentBytes - 1)); }
	}

	MemoryInstance* createMemory(MemoryType type)
	{
		MemoryInstance* memory = new MemoryInstance(type);

		// On a 64-bit runtime, allocate 8GB of address space for the memory.
		// This allows eliding bounds checks on memory accesses, since a 32-bit index + 32-bit offset will always be within the reserved address-space.
		// On a 32-bit runtime, allocate 1GB.
		const size_t memoryMaxBytes = HAS_64BIT_ADDRESS_SPACE ? 8ull*1024*1024*1024 : 0x40000000;
		
		// On a 64 bit runtime, align the instance memory base to a 4GB boundary, so the lower 32-bits will all be zero. Maybe it will allow better code generation?
		// Note that this reserves a full extra 4GB, but only uses (4GB-1 page) for alignment, so there will always be a guard page at the end to
		// protect against unaligned loads/stores that straddle the end of the address-space.
		const size_t alignmentBytes = HAS_64BIT_ADDRESS_SPACE ? 4ull*1024*1024*1024 : ((uintp)1 << Platform::getPageSizeLog2());
		memory->baseAddress = allocateVirtualPagesAligned(memoryMaxBytes,alignmentBytes,memory->reservedBaseAddress,memory->reservedNumPlatformPages);
		memory->endOffset = memoryMaxBytes;
		if(!memory->baseAddress) { delete memory; return nullptr; }

		// Grow the memory to the type's minimum size.
		if(growMemory(memory,type.size.min) == -1) { delete memory; return nullptr; }

		// Add the memory to the global array.
		memories.push_back(memory);
		return memory;
	}

	MemoryInstance::~MemoryInstance()
	{
		// Decommit all default memory pages.
		if(numPages > 0) { Platform::decommitVirtualPages(baseAddress,numPages << getPlatformPagesPerWebAssemblyPageLog2()); }

		// Free the virtual address space.
		if(reservedNumPlatformPages > 0) { Platform::freeVirtualPages(reservedBaseAddress,reservedNumPlatformPages); }
		reservedBaseAddress = baseAddress = nullptr;
		reservedNumPlatformPages = 0;

		// Remove the memory from the global array.
		for(uintp memoryIndex = 0;memoryIndex < memories.size();++memoryIndex)
		{
			if(memories[memoryIndex] == this) { memories.erase(memories.begin() + memoryIndex); break; }
		}
	}
	
	bool isAddressOwnedByMemory(uint8* address)
	{
		// Iterate over all memories and check if the address is within the reserved address space for each.
		for(auto memory : memories)
		{
			uint8* startAddress = memory->reservedBaseAddress;
			uint8* endAddress = memory->reservedBaseAddress + (memory->reservedNumPlatformPages << Platform::getPageSizeLog2());
			if(address >= startAddress && address < endAddress) { return true; }
		}
		return false;
	}

	size_t getMemoryNumPages(MemoryInstance* memory) { return memory->numPages; }
	size_t getMemoryMaxPages(MemoryInstance* memory) { return memory->type.size.max; }

	intp growMemory(MemoryInstance* memory,size_t numNewPages)
	{
		const size_t previousNumPages = memory->numPages;
		if(numNewPages > 0)
		{
			// If the number of pages to grow would cause the memory's size to exceed its maximum, return -1.
			if(numNewPages > memory->type.size.max || memory->numPages > memory->type.size.max - numNewPages) { return -1; }

			// Try to commit the new pages, and return -1 if the commit fails.
			if(!Platform::commitVirtualPages(
				memory->baseAddress + (memory->numPages << IR::numBytesPerPageLog2),
				numNewPages << getPlatformPagesPerWebAssemblyPageLog2()
				))
			{
				return -1;
			}
			memory->numPages += numNewPages;
		}
		return previousNumPages;
	}

	intp shrinkMemory(MemoryInstance* memory,size_t numPagesToShrink)
	{
		const size_t previousNumPages = memory->numPages;
		if(numPagesToShrink > 0)
		{
			// If the number of pages to shrink would cause the memory's size to drop below its minimum, return -1.
			if(numPagesToShrink > memory->numPages
			|| memory->numPages - numPagesToShrink < memory->type.size.min)
			{ return -1; }
			memory->numPages -= numPagesToShrink;

			// Decommit the pages that were shrunk off the end of the memory.
			Platform::decommitVirtualPages(
				memory->baseAddress + (memory->numPages << IR::numBytesPerPageLog2),
				numPagesToShrink << getPlatformPagesPerWebAssemblyPageLog2()
				);
		}
		return previousNumPages;
	}

	uint8* getMemoryBaseAddress(MemoryInstance* memory)
	{
		return memory->baseAddress;
	}
	
	uint8* getValidatedMemoryOffsetRange(MemoryInstance* memory,uintp offset,size_t numBytes)
	{
		// Validate that the range [offset..offset+numBytes) is contained by the memory's reserved pages.
		uint8* address = memory->baseAddress + offset;
		if(	!memory
		||	address < memory->reservedBaseAddress
		||	address + numBytes < address
		||	address + numBytes >= memory->reservedBaseAddress + (memory->reservedNumPlatformPages << Platform::getPageSizeLog2()))
		{
			causeException(Exception::Cause::accessViolation);
		}
		return address;
	}

}
