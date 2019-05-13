#pragma once

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/I128.h"

namespace WAVM { namespace Platform {
	// Returns the real-time nanoseconds that have elapsed since 1970/1/1 00:00:00 UTC.
	PLATFORM_API I128 getRealtimeClock();

	// Returns the precision of getRealtimeClock, in nanoseconds per clock tick.
	PLATFORM_API I128 getRealtimeClockResolution();

	// Returns the current value of a clock that may be used as an absolute time for wait timeouts.
	// The resolution is nanoseconds, and the origin is arbitrary.
	PLATFORM_API I128 getMonotonicClock();

	// Returns the precision of getMonotonicClock, in nanoseconds per clock tick.
	PLATFORM_API I128 getMonotonicClockResolution();

	// Returns a high-resolution per-process clock: nanoseconds since some arbitrary reference time.
	PLATFORM_API I128 getProcessClock();

	// Returns the precision of getProcessClock, in nanoseconds per clock tick.
	PLATFORM_API I128 getProcessClockResolution();

	// Returns a high-resolution per-thread clock: nanoseconds since some arbitrary reference time.
	PLATFORM_API I128 getThreadClock();

	// Returns the precision of getThreadClock, in nanoseconds per clock tick.
	PLATFORM_API I128 getThreadClockResolution();
}}
