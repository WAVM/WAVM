#include "WAVM.h"
#include <Windows.h>
#include <intrin.h>

namespace WAVM
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

	size_t internalGetPreferredVirtualPageSizeLog2()
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		size_t preferredVirtualPageSize = systemInfo.dwPageSize;
		// Verify our assumption that the virtual page size is a power of two.
		assert(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
		return __lzcnt64(preferredVirtualPageSize);
	}
	uint32_t getPreferredVirtualPageSizeLog2()
	{
		static size_t preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
		return preferredVirtualPageSizeLog2;
	}

	bool isPageAligned(uint8_t* address)
	{
		const uint64_t addressBits = *(uint64_t*)&address;
		return (addressBits & ((1ull << getPreferredVirtualPageSizeLog2()) - 1)) == 0;
	}

	uint8_t* allocateVirtualPages(size_t numPages)
	{
		return (uint8_t*)VirtualAlloc(nullptr,numPages << getPreferredVirtualPageSizeLog2(),MEM_RESERVE,PAGE_READWRITE);
	}

	bool commitVirtualPages(uint8_t* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		return baseVirtualAddress == VirtualAlloc(baseVirtualAddress,numPages << getPreferredVirtualPageSizeLog2(),MEM_COMMIT,PAGE_READWRITE);
	}
	
	void decommitVirtualPages(uint8_t* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,numPages << getPreferredVirtualPageSizeLog2(),MEM_DECOMMIT);
		assert(result);
	}

	void freeVirtualPages(uint8_t* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,0/*numPages << getPreferredVirtualPageSizeLog2()*/,MEM_RELEASE);
		assert(result);
	}
}