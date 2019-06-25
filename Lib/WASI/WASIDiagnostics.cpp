#include <atomic>
#include "./WASIPrivate.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASI/WASI.h"

using namespace WAVM;
using namespace WAVM::WASI;

std::atomic<SyscallTraceLevel> syscallTraceLevel{SyscallTraceLevel::none};

void WASI::setSyscallTraceLevel(SyscallTraceLevel newLevel)
{
	syscallTraceLevel.store(newLevel, std::memory_order_relaxed);
}

static void traceSyscallv(const char* syscallName, const char* argFormat, va_list argList)
{
	SyscallTraceLevel syscallTraceLevelSnapshot = syscallTraceLevel.load(std::memory_order_relaxed);
	if(syscallTraceLevelSnapshot != SyscallTraceLevel::none)
	{
		Log::printf(Log::output, "SYSCALL: %s", syscallName);
		Log::vprintf(Log::output, argFormat, argList);
		va_end(argList);

		if(syscallTraceLevelSnapshot != SyscallTraceLevel::syscallsWithCallstacks)
		{ Log::printf(Log::output, "\n"); }
		else
		{
			Log::printf(Log::output, " - Call stack:\n");

			Platform::CallStack callStack = Platform::captureCallStack(4);
			if(callStack.stackFrames.size() > 4) { callStack.stackFrames.resize(4); }
			std::vector<std::string> callStackFrameDescriptions
				= Runtime::describeCallStack(callStack);
			for(const std::string& frameDescription : callStackFrameDescriptions)
			{ Log::printf(Log::output, "SYSCALL:     %s\n", frameDescription.c_str()); }
		}
	}
}

VALIDATE_AS_PRINTF(2, 3)
void WASI::traceSyscallf(const char* syscallName, const char* argFormat, ...)
{
	va_list argList;
	va_start(argList, argFormat);
	traceSyscallv(syscallName, argFormat, argList);
	va_end(argList);
}

VALIDATE_AS_PRINTF(2, 3)
void WASI::traceSyscallReturnf(const char* syscallName, const char* returnFormat, ...)
{
	SyscallTraceLevel syscallTraceLevelSnapshot = syscallTraceLevel.load(std::memory_order_relaxed);
	if(syscallTraceLevelSnapshot != SyscallTraceLevel::none)
	{
		va_list argList;
		va_start(argList, returnFormat);
		Log::printf(Log::output, "SYSCALL: %s -> ", syscallName);
		Log::vprintf(Log::output, returnFormat, argList);
		Log::printf(Log::output, "\n");
		va_end(argList);
	}
}
