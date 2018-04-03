#pragma once

#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"
#include "Logging/Logging.h"
#include "Runtime/Runtime.h"
#include "Runtime/Intrinsics.h"

using namespace Runtime;

typedef I32 wavix_time_t;

struct wavix_timespec
{
	wavix_time_t tv_sec;
	I32 tv_nsec;
};

struct Thread;

struct Process
{
	GCPointer<Compartment> compartment;

	GCPointer<MemoryInstance> memory;
	GCPointer<TableInstance> table;

	Platform::Mutex* cwdMutex;
	std::string cwd;

	Platform::Mutex* fileMutex;
	std::vector<Platform::File*> files;
	std::vector<I32> freeFileIndices;

	std::vector<std::string> args;

	Platform::Mutex* threadsMutex;
	std::vector<Thread*> threads;

	Process();
	~Process();
};

struct Thread
{
	Process* process;
	GCPointer<Context> context;

	Thread(Process* inProcess,Context* inContext)
	: process(inProcess)
	, context(inContext)
	{}
};

DECLARE_INTRINSIC_MODULE(wavix);

extern thread_local Thread* currentThread;

extern std::string sysroot;
extern bool isTracingSyscalls;

inline void traceSyscallf(const char* syscallName,const char* argFormat,...)
{
	if(isTracingSyscalls)
	{
		va_list argList;
		va_start(argList,argFormat);
		Log::printf(Log::Category::error,"SYSCALL: %s",syscallName);
		Log::vprintf(Log::Category::error,argFormat,argList);
		Log::printf(Log::Category::error,"\n");
		va_end(argList);
	}
}

inline U32 coerce32bitAddress(Uptr address)
{
	if(address >= UINT32_MAX) { Runtime::throwException(Runtime::Exception::integerDivideByZeroOrIntegerOverflowType); }
	return (U32)address;
}
