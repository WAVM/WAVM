#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include "WAVM/Platform/Clock.h"

using namespace WAVM;
using namespace WAVM::Platform;

U64 Platform::getMonotonicClock()
{
#ifdef __APPLE__
	timeval timeVal;
	gettimeofday(&timeVal, nullptr);
	return U64(timeVal.tv_sec) * 1000000 + U64(timeVal.tv_usec);
#else
	timespec monotonicClock;
	clock_gettime(CLOCK_MONOTONIC, &monotonicClock);
	return U64(monotonicClock.tv_sec) * 1000000 + U64(monotonicClock.tv_nsec) / 1000;
#endif
}
