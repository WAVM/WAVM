#if _WIN32

#include "Core.h"
#include "Platform.h"
#include <Windows.h>
#include <iostream>

namespace Platform
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

	static size_t internalGetPreferredVirtualPageSizeLog2()
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		size_t preferredVirtualPageSize = systemInfo.dwPageSize;
		// Verify our assumption that the virtual page size is a power of two.
		assert(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
		return floorLogTwo(preferredVirtualPageSize);
	}
	uint32 getPageSizeLog2()
	{
		static size_t preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
		return (uint32)preferredVirtualPageSizeLog2;
	}

	uint32 memoryAccessAsWin32Flag(MemoryAccess access)
	{
		switch(access)
		{
		default:
		case MemoryAccess::None: return PAGE_NOACCESS;
		case MemoryAccess::ReadOnly: return PAGE_READONLY;
		case MemoryAccess::ReadWrite: return PAGE_READWRITE;
		case MemoryAccess::Execute: return PAGE_EXECUTE_READ;
		}
	}

	#ifdef _DEBUG
		static bool isPageAligned(uint8* address)
		{
			const uintptr addressBits = reinterpret_cast<uintptr>(address);
			return (addressBits & ((1ull << getPageSizeLog2()) - 1)) == 0;
		}
	#endif

	uint8* allocateVirtualPages(size_t numPages)
	{
		size_t numBytes = numPages << getPageSizeLog2();
		auto result = VirtualAlloc(nullptr,numBytes,MEM_RESERVE,PAGE_NOACCESS);
		if(result == NULL)
		{
			std::cerr << "VirtualAlloc(" << numBytes/1024 << "KB) failed: GetLastError=" << GetLastError() << std::endl;
			return nullptr;
		}
		return (uint8*)result;
	}

	bool commitVirtualPages(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		assert(isPageAligned(baseVirtualAddress));
		return baseVirtualAddress == VirtualAlloc(baseVirtualAddress,numPages << getPageSizeLog2(),MEM_COMMIT,memoryAccessAsWin32Flag(access));
	}

	bool setVirtualPageAccess(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		assert(isPageAligned(baseVirtualAddress));
		DWORD oldProtection = 0;
		return VirtualProtect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsWin32Flag(access),&oldProtection) != 0;
	}
	
	void decommitVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,numPages << getPageSizeLog2(),MEM_DECOMMIT);
		if(!result) { throw; }
	}

	void freeVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,0/*numPages << getPageSizeLog2()*/,MEM_RELEASE);
		if(!result) { throw; }
	}
}

#endif
