#include "./WASIPrivate.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/WASI/WASI.h"
#include "WAVM/WASI/WASIABI64.h"

using namespace WAVM;
using namespace WAVM::WASI;
using namespace WAVM::Runtime;

namespace WAVM { namespace WASI {
	WAVM_DEFINE_INTRINSIC_MODULE(wasiClocks)
}}

static bool getPlatformClock(__wasi_clockid_t clock, Platform::Clock& outPlatformClock)
{
	switch(clock)
	{
	case __WASI_CLOCK_REALTIME: outPlatformClock = Platform::Clock::realtime; return true;
	case __WASI_CLOCK_MONOTONIC: outPlatformClock = Platform::Clock::monotonic; return true;
	case __WASI_CLOCK_PROCESS_CPUTIME_ID:
	case __WASI_CLOCK_THREAD_CPUTIME_ID:
		outPlatformClock = Platform::Clock::processCPUTime;
		return true;
	default: return false;
	}
}

#include "DefineIntrinsicsI32.h"
#include "WASIClocks.h"
#if UINT32_MAX < SIZE_MAX
#include "DefineIntrinsicsI64.h"
#include "WASIClocks.h"
#endif
