#include "WAVM/Platform/Clock.h"
#include "WAVM/Inline/BasicTypes.h"

#define NOMINMAX
#include <Windows.h>

using namespace WAVM;
using namespace WAVM::Platform;

U64 Platform::getMonotonicClock()
{
	LARGE_INTEGER performanceCounter;
	LARGE_INTEGER performanceCounterFrequency;
	QueryPerformanceCounter(&performanceCounter);
	QueryPerformanceFrequency(&performanceCounterFrequency);

	const U64 wavmFrequency = 1000000;

	return U64(performanceCounterFrequency.QuadPart) > wavmFrequency
			   ? performanceCounter.QuadPart
					 / (performanceCounterFrequency.QuadPart / wavmFrequency)
			   : performanceCounter.QuadPart
					 * (wavmFrequency / performanceCounterFrequency.QuadPart);
}
