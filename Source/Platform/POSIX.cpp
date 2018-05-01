#ifndef _WIN32

#include "Inline/BasicTypes.h"
#include "Inline/Errors.h"
#include "Platform/Platform.h"
#include "Logging/Logging.h"

#include <atomic>
#include <errno.h>
#include <exception>
#include <fcntl.h>
#include <limits.h>
#include <memory>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __linux__
	#include <dlfcn.h>
#endif

#ifdef __APPLE__
	#define MAP_ANONYMOUS MAP_ANON
	#define UC_RESET_ALT_STACK	0x80000000			
	extern "C" int __sigreturn(ucontext_t *, int);
#endif

#ifdef __linux__
	#define MAP_STACK_FLAGS (MAP_STACK)
#else
	#define MAP_STACK_FLAGS 0
#endif

// This struct layout is replicated in POSIX.S
struct ExecutionContext
{
	U64 rbx;
	U64 rsp;
	U64 rbp;
	U64 r12;
	U64 r13;
	U64 r14;
	U64 r15;
	U64 rip;
};
static_assert(offsetof(ExecutionContext,rbx) == 0,  "unexpected offset");
static_assert(offsetof(ExecutionContext,rsp) == 8,  "unexpected offset");
static_assert(offsetof(ExecutionContext,rbp) == 16, "unexpected offset");
static_assert(offsetof(ExecutionContext,r12) == 24, "unexpected offset");
static_assert(offsetof(ExecutionContext,r13) == 32, "unexpected offset");
static_assert(offsetof(ExecutionContext,r14) == 40, "unexpected offset");
static_assert(offsetof(ExecutionContext,r15) == 48, "unexpected offset");
static_assert(offsetof(ExecutionContext,rip) == 56, "unexpected offset");
static_assert(sizeof(ExecutionContext)       == 64, "unexpected size");

extern "C"
{
	// C++ ABI exception handling functions
	extern void* __cxa_allocate_exception(size_t numBytes) throw();
	extern void __cxa_free_exception(void* exception) throw();
	extern void __cxa_throw(void* exception,void* exceptionTypeInfo,void (*dest)(void*));
	extern std::type_info* __cxa_current_exception_type();

	// Defined in POSIX.S
	extern I64 saveExecutionState(ExecutionContext* outContext,I64 returnCode) noexcept(false);
	[[noreturn]] extern void loadExecutionState(ExecutionContext* context,I64 returnCode);
	extern I64 switchToForkedStackContext(ExecutionContext* forkedContext,U8* trampolineFramePointer) noexcept(false);
	extern U8* getStackPointer();

	const char *__asan_default_options()
	{
		return "handle_segv=false:handle_sigbus=false:handle_sigfpe=false:replace_intrin=false";
	}
}

namespace Platform
{
	static Uptr internalGetPreferredVirtualPageSizeLog2()
	{
		U32 preferredVirtualPageSize = sysconf(_SC_PAGESIZE);
		// Verify our assumption that the virtual page size is a power of two.
		assert(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
		return floorLogTwo(preferredVirtualPageSize);
	}
	Uptr getPageSizeLog2()
	{
		static Uptr preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
		return preferredVirtualPageSizeLog2;
	}
	
	U32 memoryAccessAsPOSIXFlag(MemoryAccess access)
	{
		switch(access)
		{
		default:
		case MemoryAccess::None: return PROT_NONE;
		case MemoryAccess::ReadOnly: return PROT_READ;
		case MemoryAccess::ReadWrite: return PROT_READ | PROT_WRITE;
		case MemoryAccess::Execute: return PROT_EXEC;
		case MemoryAccess::ReadWriteExecute: return PROT_EXEC | PROT_READ | PROT_WRITE;
		}
	}

	bool isPageAligned(U8* address)
	{
		const Uptr addressBits = reinterpret_cast<Uptr>(address);
		return (addressBits & ((1ull << getPageSizeLog2()) - 1)) == 0;
	}

	U8* allocateVirtualPages(Uptr numPages)
	{
		Uptr numBytes = numPages << getPageSizeLog2();
		void* result = mmap(nullptr,numBytes,PROT_NONE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		if(result == MAP_FAILED) { return nullptr; }
		return (U8*)result;
	}

	U8* allocateAlignedVirtualPages(Uptr numPages,Uptr alignmentLog2,U8*& outUnalignedBaseAddress)
	{
		const Uptr pageSizeLog2 = getPageSizeLog2();
		const Uptr numBytes = numPages << pageSizeLog2;
		if(alignmentLog2 > pageSizeLog2)
		{
			// Call mmap with enough padding added to the size to align the allocation within the unaligned mapping.
			const Uptr alignmentBytes = 1ull << alignmentLog2;
			U8* unalignedBaseAddress = (U8*)mmap(nullptr,numBytes + alignmentBytes,PROT_NONE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
			if(unalignedBaseAddress == MAP_FAILED) { return nullptr; }

			const Uptr address = reinterpret_cast<Uptr>(unalignedBaseAddress);
			const Uptr alignedAddress = (address + alignmentBytes - 1) & ~(alignmentBytes - 1);
			U8* result = reinterpret_cast<U8*>(alignedAddress);

			// Unmap the start and end of the unaligned mapping, leaving the aligned mapping in the middle.
			errorUnless(!munmap(unalignedBaseAddress,alignedAddress - address));
			errorUnless(!munmap(
				result + (numPages << pageSizeLog2),
				alignmentBytes - (alignedAddress - address)));

			outUnalignedBaseAddress = unalignedBaseAddress;
			return result;
		}
		else
		{
			outUnalignedBaseAddress = allocateVirtualPages(numPages);
			return outUnalignedBaseAddress;
		}
	}

	bool commitVirtualPages(U8* baseVirtualAddress,Uptr numPages,MemoryAccess access)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		return mprotect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsPOSIXFlag(access)) == 0;
	}
	
	bool setVirtualPageAccess(U8* baseVirtualAddress,Uptr numPages,MemoryAccess access)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		return mprotect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsPOSIXFlag(access)) == 0;
	}

	void decommitVirtualPages(U8* baseVirtualAddress,Uptr numPages)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		auto numBytes = numPages << getPageSizeLog2();
		if(madvise(baseVirtualAddress,numBytes,MADV_DONTNEED)) { Errors::fatal("madvise failed"); }
		if(mprotect(baseVirtualAddress,numBytes,PROT_NONE)) { Errors::fatal("mprotect failed"); }
	}

	void freeVirtualPages(U8* baseVirtualAddress,Uptr numPages)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		if(munmap(baseVirtualAddress,numPages << getPageSizeLog2())) { Errors::fatal("munmap failed"); }
	}

	void freeAlignedVirtualPages(U8* unalignedBaseAddress,Uptr numPages,Uptr alignmentLog2)
	{
		errorUnless(isPageAligned(unalignedBaseAddress));
		if(munmap(unalignedBaseAddress,numPages << getPageSizeLog2())) { Errors::fatal("munmap failed"); }
	}

	bool describeInstructionPointer(Uptr ip,std::string& outDescription)
	{
		#ifdef __linux__
			// Look up static symbol information for the address.
			Dl_info symbolInfo;
			if(dladdr((void*)ip,&symbolInfo) && symbolInfo.dli_sname)
			{
				outDescription = symbolInfo.dli_sname;
				return true;
			}
		#endif
		return false;
	}

	void getCurrentThreadStack(U8*& outMinAddr,U8*& outMaxAddr)
	{
		// Get the stack address from pthreads, but use getrlimit to find the maximum size of the stack instead of the current.
		struct rlimit stackLimit;
		getrlimit(RLIMIT_STACK,&stackLimit);

#ifdef __linux__
		// Linux uses pthread_getattr_np/pthread_attr_getstack, and returns a pointer to the minimum address of the stack.
		pthread_attr_t threadAttributes;
		memset(&threadAttributes,0,sizeof(threadAttributes));
		pthread_getattr_np(pthread_self(),&threadAttributes);
		Uptr numStackBytes;
		pthread_attr_getstack(&threadAttributes,(void**)&outMinAddr,&numStackBytes);
		pthread_attr_destroy(&threadAttributes);
		outMaxAddr = outMinAddr + numStackBytes;
#else
		// MacOS uses pthread_get_stackaddr_np, and returns a pointer to the maximum address of the stack.
		outMaxAddr = (U8*)pthread_get_stackaddr_np(pthread_self());
#endif

		outMinAddr = outMaxAddr - stackLimit.rlim_cur;
	}

	struct SignalContext
	{
		SignalContext* outerContext;
		jmp_buf catchJump;
		std::function<bool(Platform::Signal,const Platform::CallStack&)> filter;
	};


	// Define a unique_ptr to a Platform::Event.
	struct SigAltStack
	{
		enum { numBytes = 65536 };

		U8* base = nullptr;

		~SigAltStack()
		{
			if(base)
			{
				// Disable the sig alt stack.
				// According to the docs, ss_size is ignored if SS_DISABLE is set, but MacOS returns
				// an ENOMEM error if ss_size is too small regardless of whether SS_DISABLE is set.
				stack_t disableAltStack;
				memset(&disableAltStack,0,sizeof(stack_t));
				disableAltStack.ss_flags = SS_DISABLE;
				disableAltStack.ss_size = SigAltStack::numBytes;
				errorUnless(!sigaltstack(&disableAltStack,nullptr));

				// Free the alt stack's memory.
				errorUnless(!munmap(base,SigAltStack::numBytes));
				base = nullptr;
			}
		}

		void init()
		{
			if(!base)
			{
				// Allocate a stack to use when handling signals, so stack overflow can be handled safely.
				base = (U8*)mmap(
					nullptr,SigAltStack::numBytes,
					PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS,
					-1,0);
				errorUnless(base != MAP_FAILED);
				stack_t sigAltStackInfo;
				sigAltStackInfo.ss_size = SigAltStack::numBytes;
				sigAltStackInfo.ss_sp = base;
				sigAltStackInfo.ss_flags = 0;
				errorUnless(!sigaltstack(&sigAltStackInfo,nullptr));
			}
		}
	};

	struct PlatformException
	{
		void* data;
		CallStack callStack;
	};

	thread_local SigAltStack sigAltStack;
	thread_local SignalContext* innermostSignalContext = nullptr;
	static std::atomic<SignalHandler> portableSignalHandler;

	static void deliverSignal(Signal signal,const CallStack& callStack)
	{
		// Call the signal handlers, from innermost to outermost, until one returns true.
		for(SignalContext* signalContext = innermostSignalContext;
			signalContext;
			signalContext = signalContext->outerContext)
		{
			if(signalContext->filter(signal,callStack))
			{
				// Jump back to the execution context that was saved in catchSignals.
				siglongjmp(signalContext->catchJump,1);
			}
		}

		// If the signal wasn't handled by a catchSignals call, call the portable signal handler.
		SignalHandler portableSignalHandlerSnapshot = portableSignalHandler.load();
		if(portableSignalHandlerSnapshot)
		{
			portableSignalHandlerSnapshot(signal,callStack);
		}
	}

	[[noreturn]] static void signalHandler(int signalNumber,siginfo_t* signalInfo,void*)
	{
		Signal signal;

		// Derive the exception cause the from signal that was received.
		switch(signalNumber)
		{
		case SIGFPE:
			if(signalInfo->si_code != FPE_INTDIV && signalInfo->si_code != FPE_INTOVF) { Errors::fatal("unknown SIGFPE code"); }
			signal.type = Signal::Type::intDivideByZeroOrOverflow;
			break;
		case SIGSEGV:
		case SIGBUS:
		{
			// Determine whether the faulting address was an address reserved by the stack.
			U8* stackMinAddr;
			U8* stackMaxAddr;
			getCurrentThreadStack(stackMinAddr,stackMaxAddr);
			stackMinAddr -= sysconf(_SC_PAGESIZE);
			signal.type
				= signalInfo->si_addr >= stackMinAddr && signalInfo->si_addr < stackMaxAddr
					? Signal::Type::stackOverflow
					: Signal::Type::accessViolation;
			signal.accessViolation.address
				= reinterpret_cast<Uptr>(signalInfo->si_addr);
			break;
		}
		default:
			Errors::fatalf("unknown signal number: %i",signalNumber);
			break;
		};

		// Capture the execution context, omitting this function and the function that called it,
		// so the top of the callstack is the function that triggered the signal.
		CallStack callStack = captureCallStack(2);

		deliverSignal(signal,callStack);

		switch(signalNumber)
		{
		case SIGFPE: Errors::fatalf("unhandled SIGFPE\n");
		case SIGSEGV: Errors::fatalf("unhandled SIGSEGV\n");
		case SIGBUS: Errors::fatalf("unhandled SIGBUS\n");
		default: Errors::unreachable();
		};
	}

	void initSignals()
	{
		static bool hasInitializedSignalHandlers = false;
		if(!hasInitializedSignalHandlers)
		{
			hasInitializedSignalHandlers = true;

			// Set up a signal mask for the signals we handle that will disable them inside the handler.
			struct sigaction signalAction;
			sigemptyset(&signalAction.sa_mask);
			sigaddset(&signalAction.sa_mask,SIGFPE);
			sigaddset(&signalAction.sa_mask,SIGSEGV);
			sigaddset(&signalAction.sa_mask,SIGBUS);

			// Set the signal handler for the signals we want to intercept.
			signalAction.sa_sigaction = signalHandler;
			signalAction.sa_flags = SA_SIGINFO | SA_ONSTACK;
			sigaction(SIGSEGV,&signalAction,nullptr);
			sigaction(SIGBUS,&signalAction,nullptr);
			sigaction(SIGFPE,&signalAction,nullptr);
		}
	}

	bool catchSignals(
		const std::function<void()>& thunk,
		const std::function<bool(Signal,const CallStack&)>& filter
		)
	{
		initSignals();
		sigAltStack.init();

		SignalContext signalContext;
		signalContext.outerContext = innermostSignalContext;
		signalContext.filter = filter;

		// Use saveExecutionState to capture the execution state into the signal context. If a signal is raised,
		// the signal handler will jump back to here.
		bool isReturningFromSignalHandler = sigsetjmp(signalContext.catchJump,1) != 0;
		if(!isReturningFromSignalHandler)
		{
			innermostSignalContext = &signalContext;

			// Call the thunk.
			thunk();
		}
		innermostSignalContext = signalContext.outerContext;

		return isReturningFromSignalHandler;
	}

	void terminateHandler()
	{
		try
		{
			std::rethrow_exception(std::current_exception());
		}
		catch(PlatformException exception)
		{
			Signal signal;
			signal.type = Signal::Type::unhandledException;
			signal.unhandledException.data = exception.data;
			deliverSignal(signal,exception.callStack);
			Errors::fatal("Unhandled runtime exception");
		}
		catch(...)
		{
			Errors::fatal("Unhandled C++ exception");
		}
	}

	void setSignalHandler(SignalHandler handler)
	{
		initSignals();
		sigAltStack.init();

		std::set_terminate(terminateHandler);

		portableSignalHandler.store(handler);
	}

	CallStack captureCallStack(Uptr numOmittedFramesFromTop)
	{
		return CallStack();
	}

	bool catchPlatformExceptions(
		const std::function<void()>& thunk,
		const std::function<void(void*,const CallStack&)>& handler
		)
	{
		try
		{
			thunk();
			return false;
		}
		catch(PlatformException exception)
		{
			handler(exception.data,exception.callStack);
			if(exception.data) { delete [] (U8*)exception.data; }
			return true;
		}
	}
	
	std::type_info* getUserExceptionTypeInfo()
	{
		static std::type_info* typeInfo = nullptr;
		if(!typeInfo)
		{
			try
			{
				throw PlatformException {nullptr};
			}
			catch(PlatformException)
			{
				typeInfo = __cxa_current_exception_type();
			}
		}
		assert(typeInfo);
		return typeInfo;
	}

	[[noreturn]] void raisePlatformException(void* data)
	{
		throw PlatformException {data,captureCallStack(1)};
		printf("unhandled PlatformException\n");
		Errors::unreachable();
	}

	struct Thread
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

	static thread_local U8* threadEntryFramePointer = nullptr;

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

	Thread* createThread(Uptr numStackBytes,I64 (*threadEntry)(void*),void* argument)
	{
		auto thread = new Thread;
		auto createArgs = new CreateThreadArgs;
		createArgs->entry = threadEntry;
		createArgs->entryArgument = argument;

		pthread_attr_t threadAttr;
		errorUnless(!pthread_attr_init(&threadAttr));
		errorUnless(!pthread_attr_setstacksize(&threadAttr,numStackBytes));

		// Create a new pthread.
		errorUnless(!pthread_create(&thread->id,&threadAttr,createThreadEntry,createArgs));
		errorUnless(!pthread_attr_destroy(&threadAttr));

		return thread;
	}

	void detachThread(Thread* thread)
	{
		errorUnless(!pthread_detach(thread->id));
		delete thread;
	}

	I64 joinThread(Thread* thread)
	{
		void* returnValue = nullptr;
		errorUnless(!pthread_join(thread->id,&returnValue));
		delete thread;
		return reinterpret_cast<I64>(returnValue);
	}

	void exitThread(I64 argument)
	{
		throw ExitThreadException {argument};
		Errors::unreachable();
	}

	NO_ASAN static void* forkThreadEntry(void* argsVoid)
	{
		std::unique_ptr<ForkThreadArgs> args((ForkThreadArgs*)argsVoid);
		I64 result = 0;
		try
		{
			threadEntryFramePointer = args->threadEntryFramePointer;

			result = switchToForkedStackContext(
				&args->forkContext,
				args->threadEntryFramePointer);
		}
		catch(ExitThreadException exception)
		{
			result = exception.exitCode;
		}

		return reinterpret_cast<void*>(result);
	}

	NO_ASAN Thread* forkCurrentThread()
	{
		auto forkThreadArgs = new ForkThreadArgs;

		if(!threadEntryFramePointer)
		{
			Errors::fatal("Cannot fork a thread that wasn't created by Platform::createThread");
		}
		if(innermostSignalContext)
		{
			Errors::fatal("Cannot fork a thread with catchSignals on the stack");
		}

		// Capture the current execution state in forkThreadArgs->forkContext.
		// The forked thread will load this execution context, and "return" from this function on the forked stack.
		const I64 isExecutingInFork = saveExecutionState(&forkThreadArgs->forkContext,0);
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
			getCurrentThreadStack(minStackAddr,maxStackAddr);
			const Uptr numStackBytes = maxStackAddr - minStackAddr;

			// Use the current stack pointer derive a conservative bounds on the area of the stack that is active.
			const U8* minActiveStackAddr = getStackPointer() - 128;
			const U8* maxActiveStackAddr = threadEntryFramePointer;
			const Uptr numActiveStackBytes = maxActiveStackAddr - minActiveStackAddr;

			if(numActiveStackBytes + PTHREAD_STACK_MIN > numStackBytes)
			{
				Errors::fatal("not enough stack space to fork thread");
			}

			// Allocate a stack for the forked thread, and copy this thread's stack to it.
			U8* forkedMinStackAddr = (U8*)mmap(
				nullptr, numStackBytes,
				PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK_FLAGS,
				-1, 0);
			errorUnless(forkedMinStackAddr != MAP_FAILED);
			U8* forkedMaxStackAddr = forkedMinStackAddr + numStackBytes - PTHREAD_STACK_MIN;
			memcpy(forkedMaxStackAddr - numActiveStackBytes,minActiveStackAddr,numActiveStackBytes);

			// Compute the offset to add to stack pointers to translate them to the forked thread's stack.
			const Iptr forkedStackOffset = forkedMaxStackAddr - maxActiveStackAddr;

			// Translate this thread's saved stack pointer to the forked stack.
			forkThreadArgs->forkContext.rsp += forkedStackOffset;

			// Translate this thread's entry stack pointer to the forked stack.
			forkThreadArgs->threadEntryFramePointer = threadEntryFramePointer + forkedStackOffset;

			// Create a pthread with a small temp stack that will just load forkThreadArgs->forkContext.m
			// Allocate the pthread_attr_t on the heap try to avoid the stack cookie check.
			pthread_attr_t* threadAttr = new pthread_attr_t;
			errorUnless(!pthread_attr_init(threadAttr));
			errorUnless(!pthread_attr_setstack(threadAttr,forkedMinStackAddr,numStackBytes));

			auto thread = new Thread;
			errorUnless(!pthread_create(&thread->id,threadAttr,(void*(*)(void*))forkThreadEntry,forkThreadArgs));

			errorUnless(!pthread_attr_destroy(threadAttr));
			delete threadAttr;

			return thread;
		}
	}

	U64 getMonotonicClock()
	{
		#ifdef __APPLE__
			timeval timeVal;
			gettimeofday(&timeVal, nullptr);
			return U64(timeVal.tv_sec) * 1000000 + U64(timeVal.tv_usec);
		#else
			timespec monotonicClock;
			clock_gettime(CLOCK_MONOTONIC,&monotonicClock);
			return U64(monotonicClock.tv_sec) * 1000000 + U64(monotonicClock.tv_nsec) / 1000;
		#endif
	}

	struct Mutex
	{
		pthread_mutex_t pthreadMutex;
	};

	Mutex* createMutex()
	{
		auto mutex = new Mutex();
		errorUnless(!pthread_mutex_init(&mutex->pthreadMutex,nullptr));
		return mutex;
	}

	void destroyMutex(Mutex* mutex)
	{
		errorUnless(!pthread_mutex_destroy(&mutex->pthreadMutex));
		delete mutex;
	}

	Lock::Lock(Mutex* inMutex)
		: mutex(inMutex)
	{
		errorUnless(!pthread_mutex_lock(&mutex->pthreadMutex));
	}

	void Lock::unlock()
	{
		if(mutex)
		{
			errorUnless(!pthread_mutex_unlock(&mutex->pthreadMutex));
			mutex = nullptr;
		}
	}

	struct Event
	{
		pthread_cond_t conditionVariable;
		pthread_mutex_t mutex;
	};

	Event* createEvent()
	{
		auto event = new Event();

		pthread_condattr_t conditionVariableAttr;
		errorUnless(!pthread_condattr_init(&conditionVariableAttr));

		// Set the condition variable to use the monotonic clock for wait timeouts.
		#ifndef __APPLE__
			errorUnless(!pthread_condattr_setclock(&conditionVariableAttr,CLOCK_MONOTONIC));
		#endif

		errorUnless(!pthread_cond_init(&event->conditionVariable,nullptr));
		errorUnless(!pthread_mutex_init(&event->mutex,nullptr));

		errorUnless(!pthread_condattr_destroy(&conditionVariableAttr));

		return event;
	}

	void destroyEvent(Event* event)
	{
		pthread_cond_destroy(&event->conditionVariable);
		errorUnless(!pthread_mutex_destroy(&event->mutex));
		delete event;
	}

	bool waitForEvent(Event* event,U64 untilTime)
	{
		errorUnless(!pthread_mutex_lock(&event->mutex));

		int result;
		if(untilTime == UINT64_MAX)
		{
			result = pthread_cond_wait(&event->conditionVariable,&event->mutex);
		}
		else
		{
			timespec untilTimeSpec;
			untilTimeSpec.tv_sec = untilTime / 1000000;
			untilTimeSpec.tv_nsec = (untilTime % 1000000) * 1000;

			result = pthread_cond_timedwait(&event->conditionVariable,&event->mutex,&untilTimeSpec);
		}

		errorUnless(!pthread_mutex_unlock(&event->mutex));

		if(result == ETIMEDOUT)
		{
			return false;
		}
		else
		{
			errorUnless(!result);
			return true;
		}
	}

	void signalEvent(Event* event)
	{
		errorUnless(!pthread_cond_signal(&event->conditionVariable));
	}
	
	// Instead of just reinterpreting the file descriptor as a pointer, use
	// -fd - 1, which maps fd=0 to a non-null value, and fd=-1 to null.
	static I32 filePtrToIndex(File* ptr)
	{
		return I32(-reinterpret_cast<Iptr>(ptr) - 1);
	}

	static File* fileIndexToPtr(int index)
	{
		return reinterpret_cast<File*>(-Iptr(index) - 1);
	}

	File* openFile(const std::string& pathName, FileAccessMode accessMode, FileCreateMode createMode)
	{
		U32 flags = 0;
		mode_t mode = 0;
		switch(accessMode)
		{
		case FileAccessMode::readOnly: flags = O_RDONLY; break;
		case FileAccessMode::writeOnly: flags = O_WRONLY; break;
		case FileAccessMode::readWrite: flags = O_RDWR; break;
		default: Errors::unreachable();
		};

		switch(createMode)
		{
		case FileCreateMode::createAlways: flags |= O_CREAT | O_TRUNC; break;
		case FileCreateMode::createNew: flags |= O_CREAT | O_EXCL; break;
		case FileCreateMode::openAlways: flags |= O_CREAT; break;
		case FileCreateMode::openExisting: break;
		case FileCreateMode::truncateExisting: flags |= O_TRUNC; break;
		default: Errors::unreachable();
		};

		switch(createMode)
		{
		case FileCreateMode::createAlways:
		case FileCreateMode::createNew:
		case FileCreateMode::openAlways: mode = S_IRWXU; break;
		default: break;
		};

		const I32 result = open(pathName.c_str(), flags, mode);
		return fileIndexToPtr(result);
	}

	bool closeFile(File* file)
	{
		return close(filePtrToIndex(file)) == 0;
	}

	File* getStdFile(StdDevice device)
	{
		switch(device)
		{
		case StdDevice::in: return fileIndexToPtr(0);
		case StdDevice::out: return fileIndexToPtr(1);
		case StdDevice::err: return fileIndexToPtr(2);
		default: Errors::unreachable();
		};
	}

	bool seekFile(File* file,I64 offset,FileSeekOrigin origin,U64& outAbsoluteOffset)
	{
		I32 whence = 0;
		switch(origin)
		{
		case FileSeekOrigin::begin: whence = SEEK_SET; break;
		case FileSeekOrigin::cur: whence = SEEK_CUR; break;
		case FileSeekOrigin::end: whence = SEEK_END; break;
		default: Errors::unreachable();
		};

		#ifdef __linux__
			const I64 result = lseek64(filePtrToIndex(file), offset, whence);
		#else
			const I64 result = lseek(filePtrToIndex(file), reinterpret_cast<off_t>(offset), whence);
		#endif
		outAbsoluteOffset = U64(result);
		return result != -1;
	}

	bool readFile(File* file, U8* outData,Uptr numBytes,Uptr& outNumBytesRead)
	{
		ssize_t result = read(filePtrToIndex(file),outData,numBytes);
		outNumBytesRead = result;
		return result == Iptr(numBytes);
	}

	bool writeFile(File* file,const U8* data,Uptr numBytes,Uptr& outNumBytesWritten)
	{
		ssize_t result = write(filePtrToIndex(file),data,numBytes);
		outNumBytesWritten = result;
		return result == Iptr(numBytes);
	}

	bool flushFileWrites(File* file)
	{
		return fsync(filePtrToIndex(file)) == 0;
	}

	std::string getCurrentWorkingDirectory()
	{
		const Uptr maxPathBytes = pathconf(".", _PC_PATH_MAX);
		char* buffer = (char*)alloca(maxPathBytes);
		errorUnless(getcwd(buffer,maxPathBytes) == buffer);
		return std::string(buffer);
	}
}

#endif
