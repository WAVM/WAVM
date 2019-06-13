#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Platform/Clock.h"

using namespace WAVM;
using namespace WAVM::Platform;

static I128 timespecToI128(timespec t)
{
	return assumeNoOverflow(I128(U64(t.tv_sec)) * 1000000000 + U64(t.tv_nsec));
}

static I128 getClockAsI128(clockid_t clockId)
{
	timespec clockTime;
	errorUnless(!clock_gettime(CLOCK_MONOTONIC, &clockTime));
	return timespecToI128(clockTime);
}

static I128 getClockResAsI128(clockid_t clockId)
{
	timespec clockResolution;
	errorUnless(!clock_getres(CLOCK_REALTIME, &clockResolution));
	return timespecToI128(clockResolution);
}

I128 Platform::getRealtimeClock() { return getClockAsI128(CLOCK_REALTIME); }
I128 Platform::getRealtimeClockResolution() { return getClockResAsI128(CLOCK_REALTIME); }

#ifdef __APPLE__

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

	return assumeNoOverflow(ns);
}
static I128 getMachAbsoluteClockResolution()
{
	mach_timebase_info_data_t cachedTimebaseInfoData = getCachedTimebaseInfoData();
	I128 ticksPerNanosecond = I128(cachedTimebaseInfoData.numer) / cachedTimebaseInfoData.denom;
	if(ticksPerNanosecond == 0) { ticksPerNanosecond = 1; }
	return ticksPerNanosecond;
}

I128 Platform::getMonotonicClock() { return getMachAbsoluteClock(); }
I128 Platform::getProcessClock() { return getMachAbsoluteClock(); }

I128 Platform::getMonotonicClockResolution() { return getMachAbsoluteClockResolution(); }
I128 Platform::getProcessClockResolution() { return getMachAbsoluteClockResolution(); }
#else

I128 Platform::getMonotonicClock() { return getClockAsI128(CLOCK_MONOTONIC); }
I128 Platform::getMonotonicClockResolution() { return getClockResAsI128(CLOCK_MONOTONIC); }

I128 Platform::getProcessClock() { return getClockAsI128(CLOCK_PROCESS_CPUTIME_ID); }
I128 Platform::getProcessClockResolution() { return getClockResAsI128(CLOCK_PROCESS_CPUTIME_ID); }

#endif
