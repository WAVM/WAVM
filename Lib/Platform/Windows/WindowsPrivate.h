#pragma once

#include <intrin.h>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Time.h"
#include "WAVM/Platform/Diagnostics.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef min
#undef max

namespace WAVM { namespace Platform {
	void initThread();

	CallStack unwindStack(const CONTEXT& immutableContext, Uptr numOmittedFramesFromTop);

	Time fileTimeToWAVMRealTime(FILETIME fileTime);
	FILETIME wavmRealTimeToFileTime(Time realTime);
}}
