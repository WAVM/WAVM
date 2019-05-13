#include <errno.h>
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

I128 Platform::getRealtimeClock()
{
	timespec realtimeClock;
	errorUnless(!clock_gettime(CLOCK_REALTIME, &realtimeClock));
	return timespecToI128(realtimeClock);
}

I128 Platform::getRealtimeClockResolution()
{
	timespec realtimeClockResolution;
	errorUnless(!clock_getres(CLOCK_REALTIME, &realtimeClockResolution));
	return timespecToI128(realtimeClockResolution);
}

I128 Platform::getMonotonicClock()
{
#ifdef __APPLE__
	timeval timeVal;
	gettimeofday(&timeVal, nullptr);
	return assumeNoOverflow(I128(U64(timeVal.tv_sec)) * 1000000000 + U64(timeVal.tv_usec) * 1000);
#else
	timespec monotonicClock;
	errorUnless(!clock_gettime(CLOCK_MONOTONIC, &monotonicClock));
	return timespecToI128(monotonicClock);
#endif
}

I128 Platform::getMonotonicClockResolution()
{
#ifdef __APPLE__
	return 1000;
#else
	timespec monotonicClockResolution;
	errorUnless(!clock_getres(CLOCK_MONOTONIC, &monotonicClockResolution));
	return timespecToI128(monotonicClockResolution);
#endif
}

// Returns a high-resolution per-process clock: nanoseconds since some arbitrary reference time.
I128 Platform::getProcessClock()
{
	timespec highResolutionProcessClock;
	errorUnless(!clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &highResolutionProcessClock));
	return timespecToI128(highResolutionProcessClock);
}

// Returns the precision of getProcessClock, in nanoseconds per clock tick.
I128 Platform::getProcessClockResolution()
{
	timespec highResolutionProcessClockResolution;
	errorUnless(!clock_getres(CLOCK_PROCESS_CPUTIME_ID, &highResolutionProcessClockResolution));
	return timespecToI128(highResolutionProcessClockResolution);
}

// Returns a high-resolution per-thread clock: nanoseconds since some arbitrary reference time.
I128 Platform::getThreadClock()
{
	timespec highResolutionThreadClock;
	errorUnless(!clock_gettime(CLOCK_THREAD_CPUTIME_ID, &highResolutionThreadClock));
	return timespecToI128(highResolutionThreadClock);
}

// Returns the precission of getThreadClock, in nanoseconds per clock tick.
I128 Platform::getThreadClockResolution()
{
	timespec highResolutionThreadClockResolution;
	errorUnless(!clock_getres(CLOCK_THREAD_CPUTIME_ID, &highResolutionThreadClockResolution));
	return timespecToI128(highResolutionThreadClockResolution);
}
