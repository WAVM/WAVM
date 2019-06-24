#include "./WASIPrivate.h"
#include "./WASITypes.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"

using namespace WAVM;
using namespace WAVM::WASI;
using namespace WAVM::Runtime;

namespace WAVM { namespace WASI {
	DEFINE_INTRINSIC_MODULE(wasiClocks)
}}

DEFINE_INTRINSIC_FUNCTION(wasiClocks,
						  "clock_res_get",
						  __wasi_errno_t,
						  __wasi_clock_res_get,
						  __wasi_clockid_t clockId,
						  WASIAddress resolutionAddress)
{
	TRACE_SYSCALL("clock_res_get", "(%u, " WASIADDRESS_FORMAT ")", clockId, resolutionAddress);

	I128 resolution128;
	switch(clockId)
	{
	case __WASI_CLOCK_REALTIME: resolution128 = Platform::getRealtimeClockResolution(); break;
	case __WASI_CLOCK_MONOTONIC: resolution128 = Platform::getMonotonicClockResolution(); break;
	case __WASI_CLOCK_PROCESS_CPUTIME_ID:
	case __WASI_CLOCK_THREAD_CPUTIME_ID:
		resolution128 = Platform::getProcessClockResolution();
		break;
	default: return TRACE_SYSCALL_RETURN(EINVAL);
	}
	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	__wasi_timestamp_t resolution = __wasi_timestamp_t(resolution128);
	memoryRef<__wasi_timestamp_t>(process->memory, resolutionAddress) = resolution;

	return TRACE_SYSCALL_RETURN(ESUCCESS, " (%" PRIu64 ")", resolution);
}

DEFINE_INTRINSIC_FUNCTION(wasiClocks,
						  "clock_time_get",
						  __wasi_errno_t,
						  __wasi_clock_time_get,
						  __wasi_clockid_t clockId,
						  __wasi_timestamp_t precision,
						  WASIAddress timeAddress)
{
	TRACE_SYSCALL("clock_time_get",
				  "(%u, %" PRIu64 ", " WASIADDRESS_FORMAT ")",
				  clockId,
				  precision,
				  timeAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	I128 currentTime128;
	switch(clockId)
	{
	case __WASI_CLOCK_REALTIME: currentTime128 = Platform::getRealtimeClock(); break;
	case __WASI_CLOCK_MONOTONIC: currentTime128 = Platform::getMonotonicClock(); break;
	case __WASI_CLOCK_PROCESS_CPUTIME_ID:
	case __WASI_CLOCK_THREAD_CPUTIME_ID:
		currentTime128 = Platform::getProcessClock() - process->processClockOrigin;
		break;
	default: return TRACE_SYSCALL_RETURN(EINVAL);
	}

	__wasi_timestamp_t currentTime = __wasi_timestamp_t(currentTime128);
	memoryRef<__wasi_timestamp_t>(process->memory, timeAddress) = currentTime;

	return TRACE_SYSCALL_RETURN(ESUCCESS, " (%" PRIu64 ")", currentTime);
}
