#include "Core/Core.h"
#include "Runtime.h"
#include "Core/Platform.h"
#include "RuntimePrivate.h"

#define HAS_64BIT_ADDRESS_SPACE (sizeof(uintptr) == 8 && !PRETEND_32BIT_ADDRESS_SPACE)

namespace Runtime
{
	std::vector<Memory*> memories;
	std::vector<Table*> tables;

	static uintptr getPlatformPagesPerWebAssemblyPageLog2()
	{
		assert(Platform::getPageSizeLog2() < WebAssembly::numBytesPerPageLog2);
		return WebAssembly::numBytesPerPageLog2 - Platform::getPageSizeLog2();
	}

	static size_t getNumPlatformPages(size_t numBytes)
	{
		return (numBytes + (uintptr(1)<<Platform::getPageSizeLog2()) - 1) >> Platform::getPageSizeLog2();
	}

	static uint8* allocateVirtualPagesAligned(size_t numBytes,size_t alignmentBytes,uint8*& outUnalignedBaseAddress,size_t& outUnalignedNumPlatformPages)
	{
		const size_t numAllocatedVirtualPages = numBytes >> Platform::getPageSizeLog2();
		const size_t alignmentPages = alignmentBytes >> Platform::getPageSizeLog2();
		outUnalignedNumPlatformPages = numAllocatedVirtualPages + alignmentPages;
		outUnalignedBaseAddress = Platform::allocateVirtualPages(outUnalignedNumPlatformPages);
		if(!outUnalignedBaseAddress) { return nullptr; }
		else { return (uint8*)((uintptr)(outUnalignedBaseAddress + alignmentBytes - 1) & ~(alignmentBytes - 1)); }
	}

	Memory* createMemory(MemoryType type)
	{
		Memory* memory = new Memory(type);

		// On a 64-bit runtime, allocate 8GB of address space for the memory.
		// This allows eliding bounds checks on memory accesses, since a 32-bit index + 32-bit offset will always be within the reserved address-space.
		// On a 32-bit runtime, allocate 1GB.
		const size_t memoryMaxBytes = HAS_64BIT_ADDRESS_SPACE ? 8ull*1024*1024*1024 : 0x40000000;
		
		// On a 64 bit runtime, align the instance memory base to a 4GB boundary, so the lower 32-bits will all be zero. Maybe it will allow better code generation?
		// Note that this reserves a full extra 4GB, but only uses (4GB-1 page) for alignment, so there will always be a guard page at the end to
		// protect against unaligned loads/stores that straddle the end of the address-space.
		const size_t alignmentBytes = HAS_64BIT_ADDRESS_SPACE ? 4ull*1024*1024*1024 : ((uintptr)1 << Platform::getPageSizeLog2());
		memory->baseAddress = allocateVirtualPagesAligned(memoryMaxBytes,alignmentBytes,memory->reservedBaseAddress,memory->reservedNumPlatformPages);
		memory->reservedNumBytes = memory->reservedNumPlatformPages << Platform::getPageSizeLog2();
		memory->maxPages = memoryMaxBytes >> WebAssembly::numBytesPerPageLog2;

		if(!memory->baseAddress) { delete memory; return nullptr; }
		if(growMemory(memory,type.size.min) == -1) { delete memory; return nullptr; }

		memories.push_back(memory);
		return memory;
	}

	Memory::~Memory()
	{
		// Decommit all default memory pages.
		if(numPages > 0) { Platform::decommitVirtualPages(baseAddress,numPages << getPlatformPagesPerWebAssemblyPageLog2()); }

		// Free the virtual address space.
		if(maxPages > 0) { Platform::freeVirtualPages(reservedBaseAddress,reservedNumPlatformPages); }

		reservedBaseAddress = baseAddress = nullptr;
		reservedNumPlatformPages = reservedNumBytes = 0;

		for(uintptr memoryIndex = 0;memoryIndex < tables.size();++memoryIndex)
		{
			if(memories[memoryIndex] == this) { memories.erase(memories.begin() + memoryIndex); break; }
		}
	}
	
	bool isAddressOwnedByMemory(uint8* address)
	{
		for(auto memory : memories)
		{
			uint8* startAddress = memory->reservedBaseAddress;
			uint8* endAddress = memory->reservedBaseAddress + memory->reservedNumBytes;
			if(address >= startAddress && address < endAddress) { return true; }
		}
		return false;
	}

	size_t getMemoryNumPages(Memory* memory) { return memory->numPages; }
	size_t getMemoryMaxPages(Memory* memory) { return memory->type.size.max; }

	intptr_t growMemory(Memory* memory,size_t numNewPages)
	{
		const size_t previousNumPages = memory->numPages;
		if(numNewPages > 0)
		{
			if(numNewPages > memory->maxPages
			|| memory->numPages > memory->maxPages - numNewPages
			|| memory->numPages > memory->type.size.max - numNewPages
			|| !Platform::commitVirtualPages(
				memory->baseAddress + (memory->numPages << WebAssembly::numBytesPerPageLog2),
				numNewPages << getPlatformPagesPerWebAssemblyPageLog2()
				))
			{
				return -1;
			}
			memory->numPages += numNewPages;
		}
		return previousNumPages;
	}

	intptr_t shrinkMemory(Memory* memory,size_t numPagesToShrink)
	{
		const size_t previousNumPages = memory->numPages;
		if(numPagesToShrink > 0)
		{
			if(numPagesToShrink > memory->numPages
			|| memory->numPages - numPagesToShrink < memory->type.size.min)
			{ return -1; }
			memory->numPages -= numPagesToShrink;
			Platform::decommitVirtualPages(
				memory->baseAddress + (memory->numPages << WebAssembly::numBytesPerPageLog2),
				numPagesToShrink << getPlatformPagesPerWebAssemblyPageLog2()
				);
		}
		return previousNumPages;
	}

	uint8* getMemoryBaseAddress(Memory* memory)
	{
		return memory->baseAddress;
	}
	
	uint8* getValidatedMemoryOffsetRange(Memory* memory,uintptr offset,size_t numBytes)
	{
		uint8* address = memory->baseAddress + offset;
		if(	!memory
		||	address < memory->reservedBaseAddress
		||	address + numBytes < address
		||	address + numBytes > memory->reservedBaseAddress + memory->reservedNumBytes)
		{
			causeException(Exception::Cause::accessViolation);
		}
		return address;
	}

	Table* createTable(TableType type)
	{
		Table* table = new Table(type);

		// In 64-bit, allocate enough address-space to safely access 32-bit table indices without bounds checking, or 16MB (4M elements) if the host is 32-bit.
		const size_t tableMaxBytes = HAS_64BIT_ADDRESS_SPACE ? (sizeof(Table::Element) << 32) : 16*1024*1024;
		
		// On a 64 bit runtime, align the table base to a 4GB boundary, so the lower 32-bits will all be zero. Maybe it will allow better code generation?
		// Note that this reserves a full extra 4GB, but only uses (4GB-1 page) for alignment, so there will always be a guard page at the end to
		// protect against unaligned loads/stores that straddle the end of the address-space.
		const size_t alignmentBytes = HAS_64BIT_ADDRESS_SPACE ? 4ull*1024*1024*1024 : (uintptr(1) << Platform::getPageSizeLog2());
		table->baseAddress = (Table::Element*)allocateVirtualPagesAligned(tableMaxBytes,alignmentBytes,table->reservedBaseAddress,table->reservedNumPlatformPages);
		table->maxPlatformPages = tableMaxBytes >> Platform::getPageSizeLog2();
		
		if(!table->baseAddress) { delete table; return nullptr; }
		if(growTable(table,type.size.min) == -1) { delete table; return nullptr; }

		tables.push_back(table);
		return table;
	}
	
	Table::~Table()
	{
		// Decommit all pages.
		if(numElements > 0) { Platform::decommitVirtualPages((uint8*)baseAddress,getNumPlatformPages(numElements * sizeof(Table::Element))); }

		// Free the virtual address space.
		if(maxPlatformPages > 0) { Platform::freeVirtualPages((uint8*)reservedBaseAddress,reservedNumPlatformPages); }

		reservedBaseAddress = nullptr;
		reservedNumPlatformPages = 0;
		baseAddress = nullptr;

		for(uintptr tableIndex = 0;tableIndex < tables.size();++tableIndex)
		{
			if(tables[tableIndex] == this) { tables.erase(tables.begin() + tableIndex); break; }
		}
	}

	bool isAddressOwnedByTable(uint8* address)
	{
		for(auto table : tables)
		{
			uint8* startAddress = (uint8*)table->reservedBaseAddress;
			uint8* endAddress = ((uint8*)table->reservedBaseAddress) + (table->reservedNumPlatformPages << Platform::getPageSizeLog2());
			if(address >= startAddress && address < endAddress) { return true; }
		}
		return false;
	}

	size_t getTableNumElements(Table* table)
	{
		return table->numElements;
	}

	intptr_t growTable(Table* table,size_t numNewElements)
	{
		const size_t previousNumElements = table->numElements;
		if(numNewElements > 0)
		{
			const size_t previousNumPlatformPages = getNumPlatformPages(table->numElements * sizeof(Table::Element));
			const size_t newNumPlatformPages = getNumPlatformPages((table->numElements+numNewElements) * sizeof(Table::Element));
			if(table->numElements > UINTPTR_MAX - numNewElements
			|| newNumPlatformPages > table->maxPlatformPages
			|| !Platform::commitVirtualPages(
				(uint8*)table->baseAddress + (previousNumPlatformPages << Platform::getPageSizeLog2()),
				newNumPlatformPages - previousNumPlatformPages
				))
			{
				return -1;
			}
			table->numElements += numNewElements;
		}
		return previousNumElements;
	}

	intptr_t shrinkTable(Table* table,size_t numElementsToShrink)
	{
		const size_t previousNumElements = table->numElements;
		if(numElementsToShrink > 0)
		{
			if(numElementsToShrink > table->numElements) { return -1; }
			const size_t previousNumPlatformPages = getNumPlatformPages(table->numElements * sizeof(Table::Element));
			table->numElements -= numElementsToShrink;
			const size_t newNumPlatformPages = getNumPlatformPages(table->numElements * sizeof(Table::Element));
			Platform::decommitVirtualPages(
				(uint8*)table->baseAddress + (newNumPlatformPages << Platform::getPageSizeLog2()),
				(previousNumPlatformPages - newNumPlatformPages) << Platform::getPageSizeLog2()
				);
		}
		return previousNumElements;
	}
}
