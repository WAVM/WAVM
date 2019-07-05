#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Platform/Clock.h"

using namespace WAVM;
using namespace WAVM::Platform;

static I128 timespecToNS(timespec t) { return I128(U64(t.tv_sec)) * 1000000000 + U64(t.tv_nsec); }

static I128 getClockAsI128(clockid_t clockId)
{
	timespec clockTime;
	errorUnless(!clock_gettime(clockId, &clockTime));
	return timespecToNS(clockTime);
}

static I128 getClockResAsI128(clockid_t clockId)
{
	timespec clockResolution;
	errorUnless(!clock_getres(clockId, &clockResolution));
	return timespecToNS(clockResolution);
}

// Real-time clock
I128 Platform::getRealtimeClock() { return getClockAsI128(CLOCK_REALTIME); }
I128 Platform::getRealtimeClockResolution() { return getClockResAsI128(CLOCK_REALTIME); }

// Monotonic clock
#ifdef CLOCK_MONOTONIC
I128 Platform::getMonotonicClock() { return getClockAsI128(CLOCK_MONOTONIC); }
I128 Platform::getMonotonicClockResolution() { return getClockResAsI128(CLOCK_MONOTONIC); }
#elif defined(__APPLE__)

#include <mach/mach_time.h>

static mach_timebase_info_data_t getTimebaseInfoData()
{
	mach_timebase_info_data_t timebaseInfoData;
	errorUnless(mach_timebase_info(&timebaseInfoData) == KERN_SUCCESS);
	return timebaseInfoData;
}

static mach_timebase_info_data_t getCachedTimebaseInfoData()
{
	static mach_timebase_info_data_t cachedTimebaseInfoData = getTimebaseInfoData();
	return cachedTimebaseInfoData;
}

static I128 getMachAbsoluteClock()
{
	mach_timebase_info_data_t cachedTimebaseInfoData = getCachedTimebaseInfoData();
	const I128 ticks = mach_absolute_time();
	const I128 ns = ticks * cachedTimebaseInfoData.numer / cachedTimebaseInfoData.denom;

	return ns;
}
static I128 getMachAbsoluteClockResolution()
{
	mach_timebase_info_data_t cachedTimebaseInfoData = getCachedTimebaseInfoData();
	I128 ticksPerNanosecond = I128(cachedTimebaseInfoData.numer) / cachedTimebaseInfoData.denom;
	if(ticksPerNanosecond == 0) { ticksPerNanosecond = 1; }
	return ticksPerNanosecond;
}

I128 Platform::getMonotonicClock() { return getMachAbsoluteClock(); }
I128 Platform::getMonotonicClockResolution() { return getMachAbsoluteClockResolution(); }
#else
#error CLOCK_MONOTONIC not supported on this platform.
#endif

// Process CPU time clock
#ifdef CLOCK_PROCESS_CPUTIME_ID
I128 Platform::getProcessClock() { return getClockAsI128(CLOCK_PROCESS_CPUTIME_ID); }
I128 Platform::getProcessClockResolution() { return getClockResAsI128(CLOCK_PROCESS_CPUTIME_ID); }
#else
static I128 timevalToNS(timeval t) { return I128(U64(t.tv_sec)) * 1000000000 + U64(t.tv_usec); }
I128 Platform::getProcessClock()
{
	struct rusage ru;
	errorUnless(!getrusage(RUSAGE_SELF, &ru));
	return timevalToNS(ru.ru_stime) + timevalToNS(ru.ru_utime);
}
I128 Platform::getProcessClockResolution() { return 1000; }
#endif
