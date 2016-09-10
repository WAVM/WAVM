#ifndef _WIN32

#include "Core/Core.h"
#include "Core/Platform.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>

#include <iostream>
#include <errno.h>

#ifdef __APPLE__
    #define MAP_ANONYMOUS MAP_ANON
#endif

namespace Platform
{
	Mutex::Mutex()
	{
		handle = new pthread_mutex_t();
		if(pthread_mutex_init((pthread_mutex_t*)handle,nullptr)) { throw; }
	}

	Mutex::~Mutex()
	{
		if(pthread_mutex_destroy((pthread_mutex_t*)handle)) { throw; }
	}

	void Mutex::Lock()
	{
		if(pthread_mutex_lock((pthread_mutex_t*)handle)) { throw; }
	}

	void Mutex::Unlock()
	{
		if(pthread_mutex_unlock((pthread_mutex_t*)handle)) { throw; }
	}

	static size_t internalGetPreferredVirtualPageSizeLog2()
	{
		uint32 preferredVirtualPageSize = sysconf(_SC_PAGESIZE);
		// Verify our assumption that the virtual page size is a power of two.
		assert(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
		return floorLogTwo(preferredVirtualPageSize);
	}
	uintptr getPageSizeLog2()
	{
		static size_t preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
		return preferredVirtualPageSizeLog2;
	}
	
	uint32 memoryAccessAsPOSIXFlag(MemoryAccess access)
	{
		switch(access)
		{
		default:
		case MemoryAccess::None: return PROT_NONE;
		case MemoryAccess::ReadOnly: return PROT_READ;
		case MemoryAccess::ReadWrite: return PROT_READ | PROT_WRITE;
		case MemoryAccess::Execute: return PROT_EXEC;
		case MemoryAccess::ReadWriteExecute: return PROT_EXEC | PROT_READ | PROT_WRITE;
		}
	}

	bool isPageAligned(uint8* address)
	{
		const uintptr addressBits = reinterpret_cast<uintptr>(address);
		return (addressBits & ((1ull << getPageSizeLog2()) - 1)) == 0;
	}

	uint8* allocateVirtualPages(size_t numPages)
	{
		size_t numBytes = numPages << getPageSizeLog2();
		auto result = mmap(nullptr,numBytes,PROT_NONE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		if(result == MAP_FAILED)
		{
			std::cerr << "mmap(" << numBytes/1024 << "KB) failed: errno=" << strerror(errno) << std::endl;
			return nullptr;
		}
		return (uint8*)result;
	}

	bool commitVirtualPages(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		assert(isPageAligned(baseVirtualAddress));
		return mprotect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsPOSIXFlag(access)) == 0;
	}
	
	bool setVirtualPageAccess(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		assert(isPageAligned(baseVirtualAddress));
		return mprotect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsPOSIXFlag(access)) == 0;
	}

	void decommitVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto numBytes = numPages << getPageSizeLog2();
		if(madvise(baseVirtualAddress,numBytes,MADV_DONTNEED)) { throw; }
		if(mprotect(baseVirtualAddress,numBytes,PROT_NONE)) { throw; }
	}

	void freeVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		if(munmap(baseVirtualAddress,numPages << getPageSizeLog2())) { throw; }
	}
}

#endif
