#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"
#include "Logging/Logging.h"
#include "Runtime/Intrinsics.h"
#include "wavix.h"

Process::Process()
: cwdMutex(Platform::createMutex())
, fileMutex(Platform::createMutex())
, threadsMutex(Platform::createMutex())
{
	Platform::Lock fileLock(fileMutex);
	files.push_back(Platform::getStdFile(Platform::StdDevice::in));
	files.push_back(Platform::getStdFile(Platform::StdDevice::out));
	files.push_back(Platform::getStdFile(Platform::StdDevice::err));
}

Process::~Process()
{
	Platform::destroyMutex(cwdMutex);
	Platform::destroyMutex(fileMutex);
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_exit",I32,__syscall_exit,I32 exitCode)
{
	traceSyscallf("exit","(%i)",exitCode);
	Platform::exitThread(exitCode);
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_exit_group",I32,__syscall_exit_group,I32 exitCode)
{
	traceSyscallf("exit_group","(%i)",exitCode);
	Platform::exitThread(exitCode);
}

DEFINE_INTRINSIC_FUNCTION_WITH_CONTEXT_SWITCH(wavix,"__syscall_fork",I32,__syscall_fork,I32 dummy)
{
	traceSyscallf("fork","");
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__syscall_execve",I32,__syscall_execve,
	U32 a, U32 b, U32 c)
{
	traceSyscallf("execve","(%i,%i,%i)",a,b,c);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_kill",I32,__syscall_kill,I32 a,I32 b)
{
	traceSyscallf("kill","(%i,%i)",a,b);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getpid",I32,__syscall_getpid,I32 a)
{
	traceSyscallf("getpid","");
	return 1;
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getppid",I32,__syscall_getppid,I32 dummy)
{
	traceSyscallf("getppid","");
	return 0;
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_sched_getaffinity",I32,__syscall_sched_getaffinity,I32 a,I32 b,I32 c)
{
	traceSyscallf("sched_getaffinity","(%i,%i,%i,%i)",a,b,c);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

#define WAVIX_WNOHANG    1
#define WAVIX_WUNTRACED  2


DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__syscall_wait4",I32,__syscall_wait4,
	I32 pid, U32 statusAddress, U32 options, U32 rusageAddress)
{
	traceSyscallf("wait4","(%i,0x%08x,%i,0x%08x)",pid,statusAddress,options,rusageAddress);

	if(rusageAddress != 0)
	{
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	return -1;
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_gettid",I32,__syscall_gettid,I32)
{
	traceSyscallf("gettid","()");
	throwException(Exception::calledUnimplementedIntrinsicType);
}

void staticInitializeProcess()
{
}