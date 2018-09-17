#pragma once

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "./errno.h"
#include "Inline/BasicTypes.h"
#include "Logging/Logging.h"
#include "Platform/Diagnostics.h"
#include "Platform/Event.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Runtime.h"

namespace Wavix
{
	typedef I32 wavix_time_t;

	struct wavix_timespec
	{
		wavix_time_t tv_sec;
		I32 tv_nsec;
	};

	struct Process;
	struct Thread;

	struct Thread
	{
		Process* process;
		Runtime::GCPointer<Runtime::Context> context;

		Platform::Event wakeEvent;

		Thread(Process* inProcess, Runtime::Context* inContext)
		: process(inProcess), context(inContext)
		{
		}
	};

	DECLARE_INTRINSIC_MODULE(wavix);

	extern thread_local Thread* currentThread;
	extern thread_local Process* currentProcess;

	extern std::string sysroot;
	extern bool isTracingSyscalls;

	VALIDATE_AS_PRINTF(2, 3)
	inline void traceSyscallf(const char* syscallName, const char* argFormat, ...)
	{
		if(isTracingSyscalls)
		{
			va_list argList;
			va_start(argList, argFormat);
			Log::printf(Log::debug,
						"SYSCALL(%" PRIxPTR "): %s",
						reinterpret_cast<Uptr>(currentThread),
						syscallName);
			Log::vprintf(Log::debug, argFormat, argList);
			Log::printf(Log::debug, "\n");
			va_end(argList);

			Platform::CallStack callStack = Platform::captureCallStack(2);
			if(callStack.stackFrames.size() > 4) { callStack.stackFrames.resize(4); }
			std::vector<std::string> callStackFrameDescriptions
				= Runtime::describeCallStack(callStack);
			for(const std::string& frameDescription : callStackFrameDescriptions)
			{ Log::printf(Log::debug, "  %s\n", frameDescription.c_str()); }
		}
	}

	VALIDATE_AS_PRINTF(2, 3)
	inline void traceSyscallReturnf(const char* syscallName, const char* returnFormat, ...)
	{
		if(isTracingSyscalls)
		{
			va_list argList;
			va_start(argList, returnFormat);
			Log::printf(Log::debug,
						"SYSCALL(%" PRIxPTR "): %s -> ",
						reinterpret_cast<Uptr>(currentThread),
						syscallName);
			Log::vprintf(Log::debug, returnFormat, argList);
			Log::printf(Log::debug, "\n");
			va_end(argList);
		}
	}

	inline U32 coerce32bitAddress(Uptr address)
	{
		if(address >= UINT32_MAX)
		{ Runtime::throwException(Runtime::Exception::integerDivideByZeroOrOverflowType); }
		return (U32)address;
	}

	inline std::string readUserString(Runtime::MemoryInstance* memory, I32 stringAddress)
	{
		// Validate the path name and make a local copy of it.
		std::string pathString;
		while(true)
		{
			const char c = Runtime::memoryRef<char>(memory, stringAddress + pathString.size());
			if(c == 0) { break; }
			else
			{
				pathString += c;
			}
		};

		return pathString;
	}
}
