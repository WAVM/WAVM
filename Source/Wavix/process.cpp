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

FORCENOINLINE static void setCurrentThread(Thread* newThread)
{
	currentThread = newThread;
}

DEFINE_INTRINSIC_FUNCTION_WITH_CONTEXT_SWITCH(wavix,"__syscall_fork",I32,__syscall_fork,I32 dummy)
{
	assert(currentThread);
	Process* currentProcess = currentThread->process;
	auto newProcess = new Process;
	newProcess->compartment = cloneCompartment(currentProcess->compartment);
	newProcess->args = currentProcess->args;
	newProcess->cwd = currentProcess->cwd;
	//newProcess->files = process->files;
	//newProcess->freeFileIndices = process->freeFileIndices;

	auto newContext = cloneContext(currentThread->context,newProcess->compartment);
	Thread* newThread = new Thread(newProcess,newContext);

	Platform::Thread* platformThread = Platform::forkCurrentThread();
	if(platformThread)
	{
		return Intrinsics::resultInContextRuntimeData<I32>(contextRuntimeData,1);
	}
	else
	{
		// Move the childThread pointer into the thread-local currentThread variable.
		// Since some compilers will cache a pointer to thread-local data that's accessed multiple times in one
		// function, and currentThread is accessed before calling forkCurrentThread, we can't directly write to it
		// in this function in case the compiler tries to write to the original thread's currentThread variable.
		// Instead, call a FORCENOINLINE function (setCurrentThread) to set the variable.
		setCurrentThread(newThread);

		// Switch the contextRuntimeData to point to the new context's runtime data.
		contextRuntimeData = getContextRuntimeData(newContext);

		return Intrinsics::resultInContextRuntimeData<I32>(contextRuntimeData,0);
	}
}

DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__syscall_execve",I32,__syscall_execve,
	U32 pathAddress, U32 argsAddress, U32 envsAddress)
{
	MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData,defaultMemoryId.id);

	std::string pathString = readUserString(memory,pathAddress);
	std::vector<std::string> args;
	std::vector<std::string> envs;
	while(true)
	{
		const U32 argStringAddress = memoryRef<U32>(memory,argsAddress);
		if(!argStringAddress) { break; }
		args.push_back(readUserString(memory,argStringAddress));
		argsAddress += sizeof(U32);
	};
	while(true)
	{
		const U32 envStringAddress = memoryRef<U32>(memory,envsAddress);
		if(!envStringAddress) { break; }
		envs.push_back(readUserString(memory,envStringAddress));
		envsAddress += sizeof(U32);
	};

	if(isTracingSyscalls)
	{
		std::string argsString;
		for(const auto& argString : args)
		{
			if(argsString.size()) { argsString += ','; }
			argsString += '\"';
			argsString += argString;
			argsString += '\"';
		}
		std::string envsString;
		for(const auto& envString : args)
		{
			if(envsString.size()) { envsString += ','; }
			envsString += '\"';
			envsString += envString;
			envsString += '\"';
		}
		traceSyscallf("execve","(\"%s\", {%s}, {%s})",pathString.c_str(), argsString.c_str(), envsString.c_str());
	}

	spawnProcess(pathString.c_str(),std::move(args),std::move(envs));

	Platform::exitThread(0);

	Errors::unreachable();
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

#define WAVIX_WSTOPPED   2
#define WAVIX_WEXITED    4
#define WAVIX_WCONTINUED 8
#define WAVIX_WNOWAIT    0x1000000

DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__syscall_wait4",I32,__syscall_wait4,
	I32 pid, U32 statusAddress, U32 options, U32 rusageAddress)
{
	MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData,defaultMemoryId.id);

	traceSyscallf("wait4","(%i,0x%08x,%i,0x%08x)",pid,statusAddress,options,rusageAddress);

	if(rusageAddress != 0)
	{
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	if(options & WAVIX_WNOHANG)
	{
		memoryRef<I32>(memory,statusAddress) = 0;
		return 0;
	}
	else
	{
		memoryRef<I32>(memory,statusAddress) = WAVIX_WEXITED;
		return pid == -1 ? 1 : pid;
	}
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_gettid",I32,__syscall_gettid,I32)
{
	traceSyscallf("gettid","()");
	//throwException(Exception::calledUnimplementedIntrinsicType);
	return 1;
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_tkill",I32,__syscall_tkill,U32 threadId,I32 signalNumber)
{
	traceSyscallf("tkill", "(%i,%i)", threadId, signalNumber);
	Platform::exitThread(-1);
	Errors::unreachable();
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_rt_sigprocmask",I32,__syscall_rt_sigprocmask,
	I32 how, U32 setAddress, U32 oldSetAddress)
{
	traceSyscallf("rt_sigprocmask", "(%i, 0x%08x, 0x%08x)", how, setAddress, oldSetAddress);
	return 0;
}

void staticInitializeProcess()
{
}