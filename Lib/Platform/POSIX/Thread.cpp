#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <cstring>
#include <memory>
#include <thread>

#include "POSIXPrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Exception.h"
#include "WAVM/Platform/Thread.h"

#ifdef __APPLE__
#define UC_RESET_ALT_STACK 0x80000000
#define MAP_ANONYMOUS MAP_ANON
#endif

#ifdef __linux__
#define MAP_STACK_FLAGS (MAP_STACK)
#else
#define MAP_STACK_FLAGS 0
#endif

using namespace WAVM;
using namespace WAVM::Platform;

struct Platform::Thread
{
	pthread_t id;
};

struct CreateThreadArgs
{
	I64 (*entry)(void*);
	void* entryArgument;
};

struct ForkThreadArgs
{
	ExecutionContext forkContext;
	U8* threadEntryFramePointer;
};

struct ExitThreadException
{
	I64 exitCode;
};

enum
{
	sigAltStackNumBytes = 65536
};

SigAltStack::~SigAltStack()
{
	if(base)
	{
		// Disable the sig alt stack.
		// According to the docs, ss_size is ignored if SS_DISABLE is set, but MacOS returns an
		// ENOMEM error if ss_size is too small regardless of whether SS_DISABLE is set.
		stack_t disableAltStack;
		memset(&disableAltStack, 0, sizeof(stack_t));
		disableAltStack.ss_flags = SS_DISABLE;
		disableAltStack.ss_size = sigAltStackNumBytes;
		errorUnless(!sigaltstack(&disableAltStack, nullptr));

		// Free the alt stack's memory.
		errorUnless(!munmap(base, sigAltStackNumBytes));
		base = nullptr;
	}
}

void SigAltStack::init()
{
	if(!base)
	{
		// Allocate a stack to use when handling signals, so stack overflow can be handled
		// safely.
		base = (U8*)mmap(nullptr,
						 sigAltStackNumBytes,
						 PROT_READ | PROT_WRITE,
						 MAP_PRIVATE | MAP_ANONYMOUS,
						 -1,
						 0);
		errorUnless(base != MAP_FAILED);
		stack_t sigAltStackInfo;
		sigAltStackInfo.ss_size = sigAltStackNumBytes;
		sigAltStackInfo.ss_sp = base;
		sigAltStackInfo.ss_flags = 0;
		errorUnless(!sigaltstack(&sigAltStackInfo, nullptr));
	}
}

thread_local SigAltStack Platform::sigAltStack;

static thread_local U8* threadEntryFramePointer = nullptr;

void Platform::getCurrentThreadStack(U8*& outMinAddr, U8*& outMaxAddr)
{
	// Get the stack address from pthreads, but use getrlimit to find the maximum size of the stack
	// instead of the current.
	struct rlimit stackLimit;
	getrlimit(RLIMIT_STACK, &stackLimit);

#ifdef __linux__
	// Linux uses pthread_getattr_np/pthread_attr_getstack, and returns a pointer to the minimum
	// address of the stack.
	pthread_attr_t threadAttributes;
	memset(&threadAttributes, 0, sizeof(threadAttributes));
	pthread_getattr_np(pthread_self(), &threadAttributes);
	Uptr numStackBytes;
	pthread_attr_getstack(&threadAttributes, (void**)&outMinAddr, &numStackBytes);
	pthread_attr_destroy(&threadAttributes);
	outMaxAddr = outMinAddr + numStackBytes;
#elif defined(__APPLE__)
	// MacOS uses pthread_get_stackaddr_np, and returns a pointer to the maximum address of the
	// stack.
	outMaxAddr = (U8*)pthread_get_stackaddr_np(pthread_self());
#elif defined(__WAVIX__)
	Errors::fatal("getCurrentThreadStack is unimplemented on Wavix.");
#else
#error unsupported platform
#endif

	outMinAddr = outMaxAddr - stackLimit.rlim_cur;
}

NO_ASAN static void* createThreadEntry(void* argsVoid)
{
	std::unique_ptr<CreateThreadArgs> args((CreateThreadArgs*)argsVoid);
	I64 result = 0;
	try
	{
		sigAltStack.init();

		threadEntryFramePointer = getStackPointer();

		result = (*args->entry)(args->entryArgument);
	}
	catch(ExitThreadException exception)
	{
		result = exception.exitCode;
	}

	return reinterpret_cast<void*>(result);
}

Platform::Thread* Platform::createThread(Uptr numStackBytes,
										 I64 (*threadEntry)(void*),
										 void* argument)
{
	auto thread = new Thread;
	auto createArgs = new CreateThreadArgs;
	createArgs->entry = threadEntry;
	createArgs->entryArgument = argument;

	pthread_attr_t threadAttr;
	errorUnless(!pthread_attr_init(&threadAttr));
	errorUnless(!pthread_attr_setstacksize(&threadAttr, numStackBytes));

	// Create a new pthread.
	errorUnless(!pthread_create(&thread->id, &threadAttr, createThreadEntry, createArgs));
	errorUnless(!pthread_attr_destroy(&threadAttr));

	return thread;
}

void Platform::detachThread(Thread* thread)
{
	errorUnless(!pthread_detach(thread->id));
	delete thread;
}

I64 Platform::joinThread(Thread* thread)
{
	void* returnValue = nullptr;
	errorUnless(!pthread_join(thread->id, &returnValue));
	delete thread;
	return reinterpret_cast<I64>(returnValue);
}

void Platform::exitThread(I64 argument)
{
	throw ExitThreadException{argument};
	Errors::unreachable();
}

NO_ASAN static void* forkThreadEntry(void* argsVoid)
{
	std::unique_ptr<ForkThreadArgs> args((ForkThreadArgs*)argsVoid);
	I64 result = 0;
	try
	{
		threadEntryFramePointer = args->threadEntryFramePointer;

		result = switchToForkedStackContext(&args->forkContext, args->threadEntryFramePointer);
	}
	catch(ExitThreadException exception)
	{
		result = exception.exitCode;
	}

	return reinterpret_cast<void*>(result);
}

// This provides a non-always_inline version of memcpy to call from NO_ASAN functions. It's needed
// to compile on GCC w/ ASAN without triggering this error:
// inlining failed in call to always_inline ‘void* memcpy(void*, const void*, size_t) throw ()’:
//   function attribute mismatch
static void memcpyToCallFromNoASAN(void* dest, const void* source, Uptr numBytes)
{
	memcpy(dest, source, numBytes);
}

NO_ASAN Thread* Platform::forkCurrentThread()
{
	auto forkThreadArgs = new ForkThreadArgs;

	if(!threadEntryFramePointer)
	{ Errors::fatal("Cannot fork a thread that wasn't created by Platform::createThread"); }
	if(innermostSignalContext)
	{ Errors::fatal("Cannot fork a thread with catchSignals on the stack"); }

	// Capture the current execution state in forkThreadArgs->forkContext.
	// The forked thread will load this execution context, and "return" from this function on the
	// forked stack.
	const I64 isExecutingInFork = saveExecutionState(&forkThreadArgs->forkContext, 0);
	if(isExecutingInFork)
	{
		// Allocate a sigaltstack for the new thread.
		sigAltStack.init();

		return nullptr;
	}
	else
	{
		// Compute the address extent of this thread's stack.
		U8* minStackAddr;
		U8* maxStackAddr;
		getCurrentThreadStack(minStackAddr, maxStackAddr);
		const Uptr numStackBytes = maxStackAddr - minStackAddr;

		// Use the current stack pointer derive a conservative bounds on the area of the stack that
		// is active.
		const U8* minActiveStackAddr = getStackPointer() - 128;
		const U8* maxActiveStackAddr = threadEntryFramePointer;
		const Uptr numActiveStackBytes = maxActiveStackAddr - minActiveStackAddr;

		if(numActiveStackBytes + PTHREAD_STACK_MIN > numStackBytes)
		{ Errors::fatal("not enough stack space to fork thread"); }

		// Allocate a stack for the forked thread, and copy this thread's stack to it.
		U8* forkedMinStackAddr = (U8*)mmap(nullptr,
										   numStackBytes,
										   PROT_READ | PROT_WRITE,
										   MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK_FLAGS,
										   -1,
										   0);
		errorUnless(forkedMinStackAddr != MAP_FAILED);
		U8* forkedMaxStackAddr = forkedMinStackAddr + numStackBytes - PTHREAD_STACK_MIN;
		memcpyToCallFromNoASAN(
			forkedMaxStackAddr - numActiveStackBytes, minActiveStackAddr, numActiveStackBytes);

		// Compute the offset to add to stack pointers to translate them to the forked thread's
		// stack.
		const Iptr forkedStackOffset = forkedMaxStackAddr - maxActiveStackAddr;

		// Translate this thread's saved stack pointer to the forked stack.
		forkThreadArgs->forkContext.rsp += forkedStackOffset;

		// Translate this thread's entry stack pointer to the forked stack.
		forkThreadArgs->threadEntryFramePointer = threadEntryFramePointer + forkedStackOffset;

		// Create a pthread with a small temp stack that will just load
		// forkThreadArgs->forkContext.rsp. Allocate the pthread_attr_t on the heap try to avoid the
		// stack cookie check.
		pthread_attr_t* threadAttr = new pthread_attr_t;
		errorUnless(!pthread_attr_init(threadAttr));
		errorUnless(!pthread_attr_setstack(threadAttr, forkedMinStackAddr, numStackBytes));

		auto thread = new Thread;
		errorUnless(!pthread_create(
			&thread->id, threadAttr, (void* (*)(void*))forkThreadEntry, forkThreadArgs));

		errorUnless(!pthread_attr_destroy(threadAttr));
		delete threadAttr;

		return thread;
	}
}

Uptr Platform::getNumberOfHardwareThreads() { return std::thread::hardware_concurrency(); }
