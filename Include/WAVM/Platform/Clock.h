
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace Platform {
	// Returns the current value of a clock that may be used as an absolute time for wait timeouts.
	// The resolution is microseconds, and the origin is arbitrary.
	PLATFORM_API U64 getMonotonicClock();
}}
