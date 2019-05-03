#include <intrin.h>
#include <atomic>
#include <memory>

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Event.h"
#include "WAVM/Platform/Thread.h"
#include "WindowsPrivate.h"

#define NOMINMAX
#include <Windows.h>

#define POISON_FORKED_STACK_SELF_POINTERS 0

using namespace WAVM;
using namespace WAVM::Platform;

static thread_local bool isThreadInitialized = false;
static thread_local U8* threadEntryFramePointer = nullptr;

struct Platform::Thread
{
	HANDLE handle = INVALID_HANDLE_VALUE;
	DWORD id = 0xffffffff;
	std::atomic<I32> numRefs{2};
	I64 result = -1;

	void releaseRef()
	{
		if(--numRefs == 0) { delete this; }
	}

private:
	~Thread()
	{
		errorUnless(CloseHandle(handle));
		handle = nullptr;
		id = 0;
	}
};

struct ThreadArgs
{
	Thread* thread;

	ThreadArgs() : thread(nullptr) {}

	~ThreadArgs()
	{
		if(thread)
		{
			thread->releaseRef();
			thread = nullptr;
		}
	}
};

struct CreateThreadArgs : ThreadArgs
{
	I64 (*entry)(void*);
	void* entryArgument;
};

struct ForkThreadArgs : ThreadArgs
{
	ExecutionContext forkContext;
	U8* threadEntryFramePointer;
};

void Platform::initThread()
{
	if(!isThreadInitialized)
	{
		isThreadInitialized = true;

		// Ensure that there's enough space left on the stack in the case of a stack overflow to
		// prepare the stack trace.
		ULONG stackOverflowReserveBytes = 32768;
		SetThreadStackGuarantee(&stackOverflowReserveBytes);
	}
}

static DWORD WINAPI createThreadEntry(void* argsVoid)
{
	initThread();

	std::unique_ptr<CreateThreadArgs> args((CreateThreadArgs*)argsVoid);

	threadEntryFramePointer = getStackPointer();

	args->thread->result = (*args->entry)(args->entryArgument);

	return 0;
}

struct ProcessorGroupInfo
{
	U32 numProcessors;
};

static std::vector<ProcessorGroupInfo> getProcessorGroupInfos()
{
	std::vector<ProcessorGroupInfo> results;
	const U16 numProcessorGroups = GetActiveProcessorGroupCount();
	for(U16 groupIndex = 0; groupIndex < numProcessorGroups; ++groupIndex)
	{ results.push_back({GetActiveProcessorCount(groupIndex)}); }
	return results;
}

Platform::Thread* Platform::createThread(Uptr numStackBytes,
										 I64 (*entry)(void*),
										 void* entryArgument)
{
	CreateThreadArgs* args = new CreateThreadArgs;
	auto thread = new Thread;
	args->thread = thread;
	args->entry = entry;
	args->entryArgument = entryArgument;

	thread->handle
		= CreateThread(nullptr, numStackBytes, createThreadEntry, args, 0, &args->thread->id);

	// On systems with more than 64 logical processors, Windows splits them into processor groups,
	// and a thread may only be assigned to run on a single processor group. By default, all threads
	// in a process are assigned to a processor group that is chosen when creating the process.
	// To allow running threads on all the processors in a system, assign new threads to processor
	// groups in a round robin manner.

	static std::vector<ProcessorGroupInfo> processorGroupInfos = getProcessorGroupInfos();
	static std::atomic<U16> nextProcessorGroup{0};

	GROUP_AFFINITY groupAffinity;
	memset(&groupAffinity, 0, sizeof(groupAffinity));
	groupAffinity.Group = nextProcessorGroup++ % processorGroupInfos.size();
	groupAffinity.Mask = (1ull << U64(processorGroupInfos[groupAffinity.Group].numProcessors)) - 1;
	if(!SetThreadGroupAffinity(thread->handle, &groupAffinity, nullptr))
	{ Errors::fatalf("SetThreadGroupAffinity failed: GetLastError=%x", GetLastError()); }

	return args->thread;
}

void Platform::detachThread(Thread* thread)
{
	wavmAssert(thread);
	thread->releaseRef();
}

I64 Platform::joinThread(Thread* thread)
{
	DWORD waitResult = WaitForSingleObject(thread->handle, INFINITE);
	switch(waitResult)
	{
	case WAIT_OBJECT_0: break;
	case WAIT_ABANDONED:
		Errors::fatal("WaitForSingleObject(INFINITE) on thread returned WAIT_ABANDONED");
		break;
	case WAIT_TIMEOUT:
		Errors::fatal("WaitForSingleObject(INFINITE) on thread returned WAIT_TIMEOUT");
		break;
	case WAIT_FAILED:
		Errors::fatalf(
			"WaitForSingleObject(INFINITE) on thread returned WAIT_FAILED. GetLastError()=%u",
			GetLastError());
		break;
	};

	const I64 result = thread->result;
	thread->releaseRef();
	return result;
}

#ifdef _WIN64
static DWORD WINAPI forkThreadEntry(void* argsVoid)
{
	std::unique_ptr<ForkThreadArgs> args((ForkThreadArgs*)argsVoid);

	threadEntryFramePointer = args->threadEntryFramePointer;

	args->thread->result
		= switchToForkedStackContext(&args->forkContext, args->threadEntryFramePointer);

	return 0;
}

Thread* Platform::forkCurrentThread()
{
	auto forkThreadArgs = new ForkThreadArgs;
	auto thread = new Thread;
	forkThreadArgs->thread = thread;

	if(!threadEntryFramePointer)
	{ Errors::fatal("Cannot fork a thread that wasn't created by Platform::createThread"); }

	// Capture the current execution state in forkThreadArgs->forkContext.
	// The forked thread will load this execution context, and "return" from this function on the
	// forked stack.
	const I64 isExecutingInFork = saveExecutionState(&forkThreadArgs->forkContext, 0);
	if(isExecutingInFork)
	{
		initThread();

		return nullptr;
	}
	else
	{
		// Compute the address extent of this thread's stack.
		const U8* minStackAddr;
		const U8* maxStackAddr;
		GetCurrentThreadStackLimits(reinterpret_cast<ULONG_PTR*>(&minStackAddr),
									reinterpret_cast<ULONG_PTR*>(&maxStackAddr));
		const Uptr numStackBytes = maxStackAddr - minStackAddr;

		// Use the current stack pointer derive a conservative bounds on the area of this thread's
		// stack that is active.
		const U8* minActiveStackAddr = getStackPointer() - 128;
		const U8* maxActiveStackAddr = threadEntryFramePointer;
		const Uptr numActiveStackBytes = maxActiveStackAddr - minActiveStackAddr;

		if(numActiveStackBytes + 65536 + 4096 > numStackBytes)
		{ Errors::fatal("not enough stack space to fork thread"); }

		// Create a suspended thread.
		forkThreadArgs->thread->handle
			= CreateThread(nullptr,
						   numStackBytes,
						   forkThreadEntry,
						   forkThreadArgs,
						   /*STACK_SIZE_PARAM_IS_A_RESERVATION |*/ CREATE_SUSPENDED,
						   &forkThreadArgs->thread->id);

		// Read the thread's initial stack pointer.
		CONTEXT* threadContext = new CONTEXT;
		threadContext->ContextFlags = CONTEXT_FULL;
		errorUnless(GetThreadContext(forkThreadArgs->thread->handle, threadContext));

		// Query the virtual address range allocated for the thread's stack.
		auto forkedStackInfo = new MEMORY_BASIC_INFORMATION;
		errorUnless(VirtualQuery(reinterpret_cast<void*>(threadContext->Rsp),
								 forkedStackInfo,
								 sizeof(MEMORY_BASIC_INFORMATION))
					== sizeof(MEMORY_BASIC_INFORMATION));
		U8* forkedStackMinAddr = reinterpret_cast<U8*>(forkedStackInfo->AllocationBase);
		U8* forkedStackMaxAddr = reinterpret_cast<U8*>(threadContext->Rsp & -16) - 4096;
		delete threadContext;
		delete forkedStackInfo;

		errorUnless(numActiveStackBytes < Uptr(forkedStackMaxAddr - forkedStackMinAddr));

		// Copy the forked stack data.
		if(POISON_FORKED_STACK_SELF_POINTERS)
		{
			const Uptr* source = (const Uptr*)minActiveStackAddr;
			const Uptr* sourceEnd = (const Uptr*)maxActiveStackAddr;
			Uptr* dest = (Uptr*)(forkedStackMaxAddr - numActiveStackBytes);
			wavmAssert(!(reinterpret_cast<Uptr>(source) & 7));
			wavmAssert(!(reinterpret_cast<Uptr>(dest) & 7));
			while(source < sourceEnd)
			{
				if(*source >= reinterpret_cast<Uptr>(minStackAddr)
				   && *source < reinterpret_cast<Uptr>(maxStackAddr))
				{
					*dest++ = 0xCCCCCCCCCCCCCCCC;
					source++;
				}
				else
				{
					*dest++ = *source++;
				}
			};
		}
		else
		{
			memcpy(
				forkedStackMaxAddr - numActiveStackBytes, minActiveStackAddr, numActiveStackBytes);
		}

		// Compute the offset to add to stack pointers to translate them to the forked thread's
		// stack.
		const Iptr forkedStackOffset = forkedStackMaxAddr - maxActiveStackAddr;
		wavmAssert(!(forkedStackOffset & 15));

		// Translate this thread's captured stack and frame pointers to the forked stack.
		forkThreadArgs->forkContext.rsp += forkedStackOffset;

		// Translate this thread's entry stack pointer to the forked stack.
		forkThreadArgs->threadEntryFramePointer = threadEntryFramePointer + forkedStackOffset;

		ResumeThread(forkThreadArgs->thread->handle);

		return forkThreadArgs->thread;
	}
}
#else
Thread* Platform::forkCurrentThread()
{
	Errors::fatal("Platform::forkCurrentThread isn't implemented on 32-bit Windows");
}
#endif

static Uptr getNumberOfHardwareThreadsImpl()
{
	Uptr result = 0;
	const U16 numProcessorGroups = GetActiveProcessorGroupCount();
	for(U16 groupIndex = 0; groupIndex < numProcessorGroups; ++groupIndex)
	{ result += GetActiveProcessorCount(groupIndex); }
	return result;
}

Uptr Platform::getNumberOfHardwareThreads()
{
	static Uptr cachedNumberOfHardwareThreads = getNumberOfHardwareThreadsImpl();
	return cachedNumberOfHardwareThreads;
}
