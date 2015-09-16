#pragma once

#include "Core.h"

#ifdef _WIN32
	#define THREAD_LOCAL __declspec(thread)
#else
	#define THREAD_LOCAL __thread
#endif

namespace Platform
{
	// A platform-independent mutex. Allows calling the constructor during static initialization, unlike std::mutex.
	struct Mutex
	{
		Mutex();
		~Mutex();

		void Lock();
		void Unlock();

	private:
		void* handle;
	};

	// RAII-style lock for Mutex.
	struct Lock
	{
		Lock(Mutex& inMutex) : mutex(&inMutex) { mutex->Lock(); }
		~Lock() { Release(); }

		void Release()
		{
			if(mutex)
			{
				mutex->Unlock();
			}
			mutex = NULL;
		}

	private:
		Mutex* mutex;
	};

	// Returns the base 2 logarithm of the preferred virtual page size.
	uint32 getPreferredVirtualPageSizeLog2();

	// Allocates virtual addresses without commiting physical pages to them.
	// Returns the base virtual address of the allocated addresses, or nullptr if the virtual address space has been exhausted.
	uint8* allocateVirtualPages(size_t numPages);

	// Commits physical memory to the specified virtual pages.
	// baseVirtualAddress must be a multiple of the preferred page size.
	// Return true if successful, or false if physical memory has been exhausted.
	bool commitVirtualPages(uint8* baseVirtualAddress,size_t numPages);

	// Decommits the physical memory that was committed to the specified virtual pages.
	// baseVirtualAddress must be a multiple of the preferred page size.
	void decommitVirtualPages(uint8* baseVirtualAddress,size_t numPages);

	// Frees virtual addresses. Any physical memory committed to the addresses must have already been decommitted.
	// baseVirtualAddress must be a multiple of the preferred page size.
	void freeVirtualPages(uint8* baseVirtualAddress,size_t numPages);
}