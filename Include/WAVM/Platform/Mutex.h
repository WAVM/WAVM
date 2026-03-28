#pragma once

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"

#ifdef _WIN32
#include <atomic>
#endif

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

		// Scoped lock: automatically unlocks when destructed.
		struct Lock
		{
			Lock() : mutex(nullptr) {}
			Lock(Mutex& inMutex) : mutex(&inMutex) { mutex->lock(); }
			~Lock() { unlock(); }

			void unlock()
			{
				if(mutex)
				{
					mutex->unlock();
					mutex = nullptr;
				}
			}

		private:
			Mutex* mutex;
		};

		friend struct ConditionVariable;

	private:
		struct alignas(WAVM_PLATFORM_MUTEX_ALIGN) LockData
		{
			U8 data[WAVM_PLATFORM_MUTEX_SIZE];
		} lockData;

#if WAVM_ENABLE_ASSERTS
#if defined(_WIN32)
		std::atomic<U32> lockingThreadId{0};
#else
		bool isLocked;
#endif
#endif
	};
}}

#if WAVM_ENABLE_ASSERTS
#define WAVM_ASSERT_MUTEX_IS_LOCKED_BY_CURRENT_THREAD(mutex)                                       \
	WAVM_ASSERT((mutex).isLockedByCurrentThread())
#else
#define WAVM_ASSERT_MUTEX_IS_LOCKED_BY_CURRENT_THREAD(mutex) WAVM_ASSERT(&(mutex) == &(mutex))
#endif
