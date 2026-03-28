#if WAVM_PLATFORM_POSIX

#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Inline/Time.h"
#include "WAVM/Platform/ConditionVariable.h"
#include "WAVM/Platform/Mutex.h"

#ifndef __APPLE__
#include "WAVM/Platform/Clock.h"
#endif

using namespace WAVM;
using namespace WAVM::Platform;

Platform::ConditionVariable::ConditionVariable()
{
	static_assert(sizeof(CondData) >= sizeof(pthread_cond_t), "");
	static_assert(alignof(CondData) >= alignof(pthread_cond_t), "");

	pthread_condattr_t condAttr;
	WAVM_ERROR_UNLESS(!pthread_condattr_init(&condAttr));

// Set the condition variable to use the monotonic clock for wait timeouts.
#ifndef __APPLE__
	WAVM_ERROR_UNLESS(!pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC));
#endif

	WAVM_ERROR_UNLESS(!pthread_cond_init((pthread_cond_t*)&condData, &condAttr));
	WAVM_ERROR_UNLESS(!pthread_condattr_destroy(&condAttr));
}

Platform::ConditionVariable::~ConditionVariable()
{
	pthread_cond_destroy((pthread_cond_t*)&condData);
}

bool Platform::ConditionVariable::wait(Mutex& mutex, Time waitDuration)
{
#if WAVM_ENABLE_ASSERTS
	mutex.isLocked = false;
#endif

	if(isInfinity(waitDuration))
	{
		int result
			= pthread_cond_wait((pthread_cond_t*)&condData, (pthread_mutex_t*)&mutex.lockData);
		WAVM_ERROR_UNLESS(!result);
#if WAVM_ENABLE_ASSERTS
		mutex.isLocked = true;
#endif
		return true;
	}
	else
	{
#ifdef __APPLE__
		// On Apple, pthread_cond_timedwait uses the wall clock rather than the monotonic
		// clock, so use the relative-time wait instead.
		timespec remainingSpec;
		remainingSpec.tv_sec = U64(waitDuration.ns / 1000000000);
		remainingSpec.tv_nsec = U64(waitDuration.ns % 1000000000);
		int result = pthread_cond_timedwait_relative_np(
			(pthread_cond_t*)&condData, (pthread_mutex_t*)&mutex.lockData, &remainingSpec);
#else
		const I128 deadlineNS = getClockTime(Clock::monotonic).ns + waitDuration.ns;
		timespec deadlineSpec;
		deadlineSpec.tv_sec = U64(deadlineNS / 1000000000);
		deadlineSpec.tv_nsec = U64(deadlineNS % 1000000000);
		int result = pthread_cond_timedwait(
			(pthread_cond_t*)&condData, (pthread_mutex_t*)&mutex.lockData, &deadlineSpec);
#endif

		if(result == ETIMEDOUT)
		{
#if WAVM_ENABLE_ASSERTS
			mutex.isLocked = true;
#endif
			return false;
		}
		WAVM_ERROR_UNLESS(!result);
#if WAVM_ENABLE_ASSERTS
		mutex.isLocked = true;
#endif
		return true;
	}
}

void Platform::ConditionVariable::signal()
{
	WAVM_ERROR_UNLESS(!pthread_cond_signal((pthread_cond_t*)&condData));
}

void Platform::ConditionVariable::broadcast()
{
	WAVM_ERROR_UNLESS(!pthread_cond_broadcast((pthread_cond_t*)&condData));
}

#endif // WAVM_PLATFORM_POSIX
