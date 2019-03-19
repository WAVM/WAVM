#pragma once

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Platform {
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

#if WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
		PLATFORM_API bool isLockedByCurrentThread();
#endif

	private:
#ifdef _WIN64
		struct CriticalSection
		{
			Uptr data[5];
		} criticalSection;
#elif defined(WIN32)
		struct CriticalSection
		{
			Uptr data[6];
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
#elif defined(__WAVIX__)
		struct PthreadMutex
		{
			Uptr data[6];
		} pthreadMutex;
#else
#error unsupported platform
#endif

#if defined(WIN32) || WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
		bool isLocked;
#endif
	};
}}

#if WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
#define wavmAssertMutexIsLockedByCurrentThread(mutex) wavmAssert((mutex).isLockedByCurrentThread())
#else
#define wavmAssertMutexIsLockedByCurrentThread(mutex) wavmAssert(&(mutex) == &(mutex))
#endif
