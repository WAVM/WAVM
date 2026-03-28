#pragma once

#if WAVM_PLATFORM_WINDOWS

#include <intrin.h>
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Time.h"
#include "WAVM/Platform/Unwind.h"

#define NOMINMAX
#include <Windows.h> // IWYU pragma: export

#include <errhandlingapi.h>    // IWYU pragma: export
#include <handleapi.h>         // IWYU pragma: export
#include <minwinbase.h>        // IWYU pragma: export
#include <minwindef.h>         // IWYU pragma: export
#include <processthreadsapi.h> // IWYU pragma: export
#include <synchapi.h>          // IWYU pragma: export
#include <winerror.h>          // IWYU pragma: export
#include <winnt.h>             // IWYU pragma: export

namespace WAVM { namespace Platform {
	void initThread();

	Time fileTimeToWAVMRealTime(FILETIME fileTime);
	FILETIME wavmRealTimeToFileTime(Time realTime);

	UnwindState makeUnwindStateFromSignalContext(void* contextPtr);
}}

#endif // WAVM_PLATFORM_WINDOWS
