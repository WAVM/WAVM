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

DEFINE_INTRINSIC_FUNCTION1(wavix,__syscall_exit,__syscall_exit,i32,i32,exitCode)
{
	traceSyscallf("exit","(%i)",exitCode);
	Platform::exitThread(exitCode);
}

DEFINE_INTRINSIC_FUNCTION1(wavix,__syscall_exit_group,__syscall_exit_group,i32,i32,exitCode)
{
	traceSyscallf("exit_group","(%i)",exitCode);
	Platform::exitThread(exitCode);
}

DEFINE_INTRINSIC_FUNCTION1(wavix,__syscall_fork,__syscall_fork,i32,i32,dummy)
{
	traceSyscallf("fork","");
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION3(wavix,__syscall_execve,__syscall_execve,i32,i32,a,i32,b,i32,c)
{
	traceSyscallf("execve","(%i,%i,%i)",a,b,c);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION2(wavix,__syscall_kill,__syscall_kill,i32,i32,a,i32,b)
{
	traceSyscallf("kill","(%i,%i)",a,b);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION1(wavix,__syscall_getpid,__syscall_getpid,i32,i32,a)
{
	traceSyscallf("getpid","");
	return 1;
}

DEFINE_INTRINSIC_FUNCTION1(wavix,__syscall_getppid,__syscall_getppid,i32,i32,)
{
	traceSyscallf("getppid","");
	return 0;
}

DEFINE_INTRINSIC_FUNCTION3(wavix,__syscall_sched_getaffinity,__syscall_sched_getaffinity,i32,i32,a,i32,b,i32,c)
{
	traceSyscallf("sched_getaffinity","(%i,%i,%i,%i)",a,b,c);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

#define WNOHANG    1
#define WUNTRACED  2

DEFINE_INTRINSIC_FUNCTION4(wavix,__syscall_wait4,__syscall_wait4,i32,i32,pid,i32,statusAddress,i32,options,i32,rusageAddress)
{
	traceSyscallf("wait4","(%i,0x%08x,%i,0x%08x)",pid,statusAddress,options,rusageAddress);

	if(rusageAddress != 0)
	{
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	return -1;
}

DEFINE_INTRINSIC_FUNCTION1(wavix,__syscall_gettid,__syscall_gettid,i32,i32,a)
{
	traceSyscallf("gettid","(%i)",a);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

void staticInitializeProcess()
{
}