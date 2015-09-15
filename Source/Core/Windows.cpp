#include "Core.h"
#include <Windows.h>
#include <intrin.h>

namespace Core
{
	Mutex::Mutex()
	{
		handle = new CRITICAL_SECTION();
		InitializeCriticalSection((CRITICAL_SECTION*)handle);
	}

	Mutex::~Mutex()
	{
		DeleteCriticalSection((CRITICAL_SECTION*)handle);
	}

	void Mutex::Lock()
	{
		EnterCriticalSection((CRITICAL_SECTION*)handle);
	}

	void Mutex::Unlock()
	{
		LeaveCriticalSection((CRITICAL_SECTION*)handle);
	}
}

namespace Memory
{
	size_t internalGetPreferredVirtualPageSizeLog2()
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		size_t preferredVirtualPageSize = systemInfo.dwPageSize;
		// Verify our assumption that the virtual page size is a power of two.
		assert(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
		return __lzcnt64(preferredVirtualPageSize);
	}
	uint32 getPreferredVirtualPageSizeLog2()
	{
		static size_t preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
		return (uint32)preferredVirtualPageSizeLog2;
	}

	bool isPageAligned(uint8* address)
	{
		const uint64 addressBits = *(uint64*)&address;
		return (addressBits & ((1ull << getPreferredVirtualPageSizeLog2()) - 1)) == 0;
	}

	uint8* allocateVirtualPages(size_t numPages)
	{
		return (uint8*)VirtualAlloc(nullptr,numPages << getPreferredVirtualPageSizeLog2(),MEM_RESERVE,PAGE_READWRITE);
	}

	bool commitVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		return baseVirtualAddress == VirtualAlloc(baseVirtualAddress,numPages << getPreferredVirtualPageSizeLog2(),MEM_COMMIT,PAGE_READWRITE);
	}
	
	void decommitVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,numPages << getPreferredVirtualPageSizeLog2(),MEM_DECOMMIT);
		if(!result) { throw; }
	}

	void freeVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,0/*numPages << getPreferredVirtualPageSizeLog2()*/,MEM_RELEASE);
		if(!result) { throw; }
	}
}