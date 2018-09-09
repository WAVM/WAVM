#pragma once

#include "Inline/BasicTypes.h"
#include "Platform/Defines.h"

namespace Platform
{
	// Platform-independent mutexes.
	struct Mutex
	{
		PLATFORM_API Mutex();
		PLATFORM_API ~Mutex();

		// Don't allow copying or moving a Mutex.
		Mutex(const Mutex&) = delete;
		Mutex(Mutex&&) = delete;
		void operator=(const Mutex&) = delete;
		void operator=(Mutex&&) = delete;

		PLATFORM_API void lock();
		PLATFORM_API void unlock();

	private:
#ifdef WIN32
		struct CriticalSection
		{
			struct _RTL_CRITICAL_SECTION_DEBUG* debugInfo;
			I32 lockCount;
			I32 recursionCount;
			void* owningThreadHandle;
			void* lockSemaphoreHandle;
			Uptr spinCount;
		} criticalSection;
#elif defined(__linux__)
		struct PthreadMutex
		{
			Uptr data[5];
		} pthreadMutex;
#elif defined(__APPLE__)
		struct PthreadMutex
		{
			Uptr data[8];
		} pthreadMutex;
#else
#error unsupported platform
#endif
	};
}