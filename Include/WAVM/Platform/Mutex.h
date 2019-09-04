#pragma once

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Platform {
	// Platform-independent mutexes.
	struct Mutex
	{
		WAVM_API Mutex();
		WAVM_API ~Mutex();

		// Don't allow copying or moving a Mutex.
		Mutex(const Mutex&) = delete;
		Mutex(Mutex&&) = delete;
		void operator=(const Mutex&) = delete;
		void operator=(Mutex&&) = delete;

		WAVM_API void lock();
		WAVM_API void unlock();

#if WAVM_ENABLE_ASSERTS
		WAVM_API bool isLockedByCurrentThread();
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
#elif defined(__linux__) && defined(__x86_64__)
		struct PthreadMutex
		{
			Uptr data[5];
		} pthreadMutex;
#elif defined(__linux__) && defined(__aarch64__)
		struct PthreadMutex
		{
			Uptr data[6];
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

#if defined(WIN32) || WAVM_ENABLE_ASSERTS
		bool isLocked;
#endif
	};
}}

#if WAVM_ENABLE_ASSERTS
#define WAVM_ASSERT_MUTEX_IS_LOCKED_BY_CURRENT_THREAD(mutex)                                       \
	WAVM_ASSERT((mutex).isLockedByCurrentThread())
#else
#define WAVM_ASSERT_MUTEX_IS_LOCKED_BY_CURRENT_THREAD(mutex) WAVM_ASSERT(&(mutex) == &(mutex))
#endif
