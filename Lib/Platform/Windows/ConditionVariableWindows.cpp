#if WAVM_PLATFORM_WINDOWS

#include <atomic>
#include <cstdint>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Inline/Time.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/ConditionVariable.h"
#include "WAVM/Platform/Mutex.h"
#include "WindowsPrivate.h"

#include <winbase.h>

using namespace WAVM;
using namespace WAVM::Platform;

Platform::ConditionVariable::ConditionVariable()
{
	static_assert(sizeof(CondData) >= sizeof(CONDITION_VARIABLE), "");
	static_assert(alignof(CondData) >= alignof(CONDITION_VARIABLE), "");
	InitializeConditionVariable((CONDITION_VARIABLE*)&condData);
}

Platform::ConditionVariable::~ConditionVariable() {}

bool Platform::ConditionVariable::wait(Mutex& mutex, Time waitDuration)
{
#if WAVM_ENABLE_ASSERTS
	mutex.lockingThreadId.store(0, std::memory_order_relaxed);
#endif

	if(isInfinity(waitDuration))
	{
		WAVM_ERROR_UNLESS(SleepConditionVariableSRW(
			(CONDITION_VARIABLE*)&condData, (SRWLOCK*)&mutex.lockData, INFINITE, 0));
#if WAVM_ENABLE_ASSERTS
		mutex.lockingThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
#endif
		return true;
	}
	else
	{
		Time currentTime = getClockTime(Clock::monotonic);
		const Time untilTime = Time{currentTime.ns + waitDuration.ns};
		while(true)
		{
			const I128 durationMS
				= currentTime.ns > untilTime.ns ? 0 : (untilTime.ns - currentTime.ns) / 1000000;
			const U32 durationMS32 = durationMS <= 0            ? 0
									 : durationMS >= UINT32_MAX ? (UINT32_MAX - 1)
																: U32(durationMS);

			if(SleepConditionVariableSRW(
				   (CONDITION_VARIABLE*)&condData, (SRWLOCK*)&mutex.lockData, durationMS32, 0))
			{
#if WAVM_ENABLE_ASSERTS
				mutex.lockingThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
#endif
				return true;
			}

			if(GetLastError() == ERROR_TIMEOUT)
			{
				currentTime = getClockTime(Clock::monotonic);
				if(currentTime.ns >= untilTime.ns)
				{
#if WAVM_ENABLE_ASSERTS
					mutex.lockingThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
#endif
					return false;
				}
			}
			else
			{
				Errors::fatal("SleepConditionVariableSRW failed");
			}
		}
	}
}

void Platform::ConditionVariable::signal()
{
	WakeConditionVariable((CONDITION_VARIABLE*)&condData);
}

void Platform::ConditionVariable::broadcast()
{
	WakeAllConditionVariable((CONDITION_VARIABLE*)&condData);
}

#endif // WAVM_PLATFORM_WINDOWS
