#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Platform/Clock.h"

#define NOMINMAX
#include <Windows.h>

using namespace WAVM;
using namespace WAVM::Platform;

static I128 fileTimeToI128(FILETIME fileTime)
{
	return assumeNoOverflow(I128(U64(fileTime.dwLowDateTime) | (U64(fileTime.dwHighDateTime) << 32))
							* 100);
}

static I128 getRealtimeClockOffset()
{
	SYSTEMTIME systemTime;
	systemTime.wYear = 1970;
	systemTime.wMonth = 1;
	systemTime.wDay = 1;
	systemTime.wHour = 0;
	systemTime.wMinute = 0;
	systemTime.wSecond = 0;
	systemTime.wMilliseconds = 0;
	FILETIME fileTime;
	errorUnless(SystemTimeToFileTime(&systemTime, &fileTime));

	return -fileTimeToI128(fileTime);
}

I128 Platform::getRealtimeClock()
{
	static const I128 cachedRealtimeClockOffset = getRealtimeClockOffset();

	FILETIME realtimeClock;
	GetSystemTimePreciseAsFileTime(&realtimeClock);

	return cachedRealtimeClockOffset + fileTimeToI128(realtimeClock);
}

I128 Platform::getRealtimeClockResolution() { return 100; }

static LARGE_INTEGER getQPCFrequency()
{
	LARGE_INTEGER result;
	errorUnless(QueryPerformanceFrequency(&result));
	return result;
}

static LARGE_INTEGER getCachedQPCFrequency()
{
	static const LARGE_INTEGER cachedResult = getQPCFrequency();
	return cachedResult;
}

I128 Platform::getMonotonicClock()
{
	LARGE_INTEGER ticksPerSecond = getCachedQPCFrequency();
	LARGE_INTEGER ticks;
	errorUnless(QueryPerformanceCounter(&ticks));
	return assumeNoOverflow(I128(ticks.QuadPart) * 1000000000) / ticksPerSecond.QuadPart;
}

I128 Platform::getMonotonicClockResolution()
{
	LARGE_INTEGER ticksPerSecond = getCachedQPCFrequency();
	U64 result = U64(1000000000) / ticksPerSecond.QuadPart;
	if(result == 0) { result = 1; }
	return I128(result);
}

I128 Platform::getProcessClock() { return getMonotonicClock(); }

I128 Platform::getProcessClockResolution() { return getMonotonicClockResolution(); }

I128 Platform::getThreadClock() { return getMonotonicClock(); }

I128 Platform::getThreadClockResolution() { return getMonotonicClockResolution(); }
