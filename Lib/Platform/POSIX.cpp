#ifndef _WIN32

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Event.h"
#include "WAVM/Platform/Exception.h"
#include "WAVM/Platform/File.h"
#include "WAVM/Platform/Intrinsic.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Platform/Thread.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#if WAVM_ENABLE_RUNTIME
#define UNW_LOCAL_ONLY
#include "libunwind.h"
#endif

#ifdef __APPLE__
#define MAP_ANONYMOUS MAP_ANON
#define UC_RESET_ALT_STACK 0x80000000
#endif

#ifdef __linux__
#define MAP_STACK_FLAGS (MAP_STACK)
#else
#define MAP_STACK_FLAGS 0
#endif

using namespace WAVM;
using namespace WAVM::Platform;

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
static_assert(offsetof(ExecutionContext, rbx) == 0, "unexpected offset");
static_assert(offsetof(ExecutionContext, rsp) == 8, "unexpected offset");
static_assert(offsetof(ExecutionContext, rbp) == 16, "unexpected offset");
static_assert(offsetof(ExecutionContext, r12) == 24, "unexpected offset");
static_assert(offsetof(ExecutionContext, r13) == 32, "unexpected offset");
static_assert(offsetof(ExecutionContext, r14) == 40, "unexpected offset");
static_assert(offsetof(ExecutionContext, r15) == 48, "unexpected offset");
static_assert(offsetof(ExecutionContext, rip) == 56, "unexpected offset");
static_assert(sizeof(ExecutionContext) == 64, "unexpected size");

extern "C" {

#ifdef __WAVIX__
extern I64 saveExecutionState(ExecutionContext* outContext, I64 returnCode) noexcept(false)
{
	Errors::fatal("saveExecutionState is unimplemented on Wavix");
}

[[noreturn]] extern void loadExecutionState(ExecutionContext* context, I64 returnCode)
{
	Errors::fatal("loadExecutionState is unimplemented on Wavix");
}

extern I64 switchToForkedStackContext(ExecutionContext* forkedContext,
									  U8* trampolineFramePointer) noexcept(false)
{
	Errors::fatal("switchToForkedStackContext is unimplemented on Wavix");
}

extern U8* getStackPointer() { Errors::fatal("getStackPointer is unimplemented on Wavix"); }

// libunwind dynamic frame registration
void __register_frame(const void* fde)
{
	Errors::fatal("__register_frame is unimplemented on Wavix");
}
void __deregister_frame(const void* fde)
{
	Errors::fatal("__deregister_frame is unimplemented on Wavix");
}

#else
// Defined in POSIX.S
extern I64 saveExecutionState(ExecutionContext* outContext, I64 returnCode) noexcept(false);
[[noreturn]] extern void loadExecutionState(ExecutionContext* context, I64 returnCode);
extern I64 switchToForkedStackContext(ExecutionContext* forkedContext,
									  U8* trampolineFramePointer) noexcept(false);
extern U8* getStackPointer();

// libunwind dynamic frame registration
void __register_frame(const void* fde);
void __deregister_frame(const void* fde);
#endif

const char* __asan_default_options()
{
	return "handle_segv=false:handle_sigbus=false:handle_sigfpe=false:replace_intrin=false";
}
}

static Uptr internalGetPreferredVirtualPageSizeLog2()
{
	U32 preferredVirtualPageSize = sysconf(_SC_PAGESIZE);
	// Verify our assumption that the virtual page size is a power of two.
	wavmAssert(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
	return floorLogTwo(preferredVirtualPageSize);
}
Uptr Platform::getPageSizeLog2()
{
	static Uptr preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
	return preferredVirtualPageSizeLog2;
}

static U32 memoryAccessAsPOSIXFlag(MemoryAccess access)
{
	switch(access)
	{
	default:
	case MemoryAccess::none: return PROT_NONE;
	case MemoryAccess::readOnly: return PROT_READ;
	case MemoryAccess::readWrite: return PROT_READ | PROT_WRITE;
	case MemoryAccess::execute: return PROT_EXEC;
	case MemoryAccess::readWriteExecute: return PROT_EXEC | PROT_READ | PROT_WRITE;
	}
}

static bool isPageAligned(U8* address)
{
	const Uptr addressBits = reinterpret_cast<Uptr>(address);
	return (addressBits & ((1ull << getPageSizeLog2()) - 1)) == 0;
}

static Mutex& getErrorReportingMutex()
{
	static Platform::Mutex mutex;
	return mutex;
}

static void dumpErrorCallStack(Uptr numOmittedFramesFromTop)
{
	std::fprintf(stderr, "Call stack:\n");
	CallStack callStack = captureCallStack(numOmittedFramesFromTop);
	for(auto frame : callStack.stackFrames)
	{
		std::string frameDescription;
		if(!Platform::describeInstructionPointer(frame.ip, frameDescription))
		{ frameDescription = "<unknown function>"; }
		std::fprintf(stderr, "  %s\n", frameDescription.c_str());
	}
	std::fflush(stderr);
}

void Platform::handleFatalError(const char* messageFormat, va_list varArgs)
{
	Lock<Platform::Mutex> lock(getErrorReportingMutex());
	std::vfprintf(stderr, messageFormat, varArgs);
	std::fflush(stderr);
	std::abort();
}

void Platform::handleAssertionFailure(const AssertMetadata& metadata)
{
	Lock<Platform::Mutex> lock(getErrorReportingMutex());
	std::fprintf(stderr,
				 "Assertion failed at %s(%u): %s\n",
				 metadata.file,
				 metadata.line,
				 metadata.condition);
	dumpErrorCallStack(2);
	std::fflush(stderr);
}

U8* Platform::allocateVirtualPages(Uptr numPages)
{
	Uptr numBytes = numPages << getPageSizeLog2();
	void* result = mmap(nullptr, numBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(result == MAP_FAILED)
	{
		fprintf(stderr,
				"mmap(0, %" PRIuPTR
				", PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) failed! errno=%s\n",
				numBytes,
				strerror(errno));
		dumpErrorCallStack(0);
		return nullptr;
	}
	return (U8*)result;
}

U8* Platform::allocateAlignedVirtualPages(Uptr numPages,
										  Uptr alignmentLog2,
										  U8*& outUnalignedBaseAddress)
{
	const Uptr pageSizeLog2 = getPageSizeLog2();
	const Uptr numBytes = numPages << pageSizeLog2;
	if(alignmentLog2 > pageSizeLog2)
	{
		// Call mmap with enough padding added to the size to align the allocation within the
		// unaligned mapping.
		const Uptr alignmentBytes = 1ull << alignmentLog2;
		U8* unalignedBaseAddress = (U8*)mmap(
			nullptr, numBytes + alignmentBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if(unalignedBaseAddress == MAP_FAILED) { return nullptr; }

		const Uptr address = reinterpret_cast<Uptr>(unalignedBaseAddress);
		const Uptr alignedAddress = (address + alignmentBytes - 1) & ~(alignmentBytes - 1);
		U8* result = reinterpret_cast<U8*>(alignedAddress);

		// Unmap the start and end of the unaligned mapping, leaving the aligned mapping in the
		// middle.
		const Uptr numHeadPaddingBytes = alignedAddress - address;
		if(numHeadPaddingBytes > 0)
		{ errorUnless(!munmap(unalignedBaseAddress, numHeadPaddingBytes)); }

		const Uptr numTailPaddingBytes = alignmentBytes - (alignedAddress - address);
		if(numTailPaddingBytes > 0)
		{ errorUnless(!munmap(result + (numPages << pageSizeLog2), numTailPaddingBytes)); }

		outUnalignedBaseAddress = result;
		return result;
	}
	else
	{
		outUnalignedBaseAddress = allocateVirtualPages(numPages);
		return outUnalignedBaseAddress;
	}
}

bool Platform::commitVirtualPages(U8* baseVirtualAddress, Uptr numPages, MemoryAccess access)
{
	errorUnless(isPageAligned(baseVirtualAddress));
	int result = mprotect(
		baseVirtualAddress, numPages << getPageSizeLog2(), memoryAccessAsPOSIXFlag(access));
	if(result != 0)
	{
		fprintf(stderr,
				"mprotect(0x%" PRIxPTR ", %" PRIuPTR ", %u) failed! errno=%s\n",
				reinterpret_cast<Uptr>(baseVirtualAddress),
				numPages << getPageSizeLog2(),
				memoryAccessAsPOSIXFlag(access),
				strerror(errno));
		dumpErrorCallStack(0);
	}
	return result == 0;
}

bool Platform::setVirtualPageAccess(U8* baseVirtualAddress, Uptr numPages, MemoryAccess access)
{
	errorUnless(isPageAligned(baseVirtualAddress));
	int result = mprotect(
		baseVirtualAddress, numPages << getPageSizeLog2(), memoryAccessAsPOSIXFlag(access));
	if(result != 0)
	{
		fprintf(stderr,
				"mprotect(0x%" PRIxPTR ", %" PRIuPTR ", %u) failed! errno=%s\n",
				reinterpret_cast<Uptr>(baseVirtualAddress),
				numPages << getPageSizeLog2(),
				memoryAccessAsPOSIXFlag(access),
				strerror(errno));
		dumpErrorCallStack(0);
	}
	return result == 0;
}

void Platform::decommitVirtualPages(U8* baseVirtualAddress, Uptr numPages)
{
	errorUnless(isPageAligned(baseVirtualAddress));
	auto numBytes = numPages << getPageSizeLog2();
	if(mprotect(baseVirtualAddress, numBytes, PROT_NONE))
	{
		Errors::fatalf("mprotect(0x%" PRIxPTR ", %u, PROT_NONE) failed! errno=%s",
					   reinterpret_cast<Uptr>(baseVirtualAddress),
					   numBytes,
					   strerror(errno));
	}
	if(madvise(baseVirtualAddress, numBytes, MADV_DONTNEED))
	{
		Errors::fatalf("madvise(0x%" PRIxPTR ", %u, MADV_DONTNEED) failed! errno=%s",
					   reinterpret_cast<Uptr>(baseVirtualAddress),
					   numBytes,
					   strerror(errno));
	}
}

void Platform::freeVirtualPages(U8* baseVirtualAddress, Uptr numPages)
{
	errorUnless(isPageAligned(baseVirtualAddress));
	if(munmap(baseVirtualAddress, numPages << getPageSizeLog2()))
	{
		Errors::fatalf("munmap(0x%" PRIxPTR ", %u) failed! errno=%s",
					   reinterpret_cast<Uptr>(baseVirtualAddress),
					   numPages << getPageSizeLog2(),
					   strerror(errno));
	}
}

void Platform::freeAlignedVirtualPages(U8* unalignedBaseAddress, Uptr numPages, Uptr alignmentLog2)
{
	errorUnless(isPageAligned(unalignedBaseAddress));
	if(munmap(unalignedBaseAddress, numPages << getPageSizeLog2()))
	{
		Errors::fatalf("munmap(0x%" PRIxPTR ", %u) failed! errno=%s",
					   reinterpret_cast<Uptr>(unalignedBaseAddress),
					   numPages << getPageSizeLog2(),
					   strerror(errno));
	}
}

bool Platform::describeInstructionPointer(Uptr ip, std::string& outDescription)
{
#if WAVM_ENABLE_RUNTIME
	// Look up static symbol information for the address.
	Dl_info symbolInfo;
	if(dladdr((void*)(ip - 1), &symbolInfo))
	{
		wavmAssert(symbolInfo.dli_fname);
		outDescription = "host!";
		outDescription += symbolInfo.dli_fname;
		outDescription += '!';
		if(!symbolInfo.dli_sname) { outDescription += "<unknown>"; }
		else
		{
			char demangledBuffer[1024];
			const char* demangledSymbolName = symbolInfo.dli_sname;
			if(symbolInfo.dli_sname[0] == '_')
			{
				Uptr numDemangledChars = sizeof(demangledBuffer);
				I32 demangleStatus = 0;
				if(abi::__cxa_demangle(symbolInfo.dli_sname,
									   demangledBuffer,
									   (size_t*)&numDemangledChars,
									   &demangleStatus))
				{ demangledSymbolName = demangledBuffer; }
			}
			outDescription += demangledSymbolName;
			outDescription += '+';
			outDescription += std::to_string(ip - reinterpret_cast<Uptr>(symbolInfo.dli_saddr));
		}
		return true;
	}
#endif
	return false;
}

static void getCurrentThreadStack(U8*& outMinAddr, U8*& outMaxAddr)
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

struct SignalContext
{
	SignalContext* outerContext;
	jmp_buf catchJump;
	std::function<bool(Platform::Signal, const Platform::CallStack&)> filter;
};

// Define a unique_ptr to a Platform::Event.
struct SigAltStack
{
	enum
	{
		numBytes = 65536
	};

	U8* base = nullptr;

	~SigAltStack()
	{
		if(base)
		{
			// Disable the sig alt stack.
			// According to the docs, ss_size is ignored if SS_DISABLE is set, but MacOS returns an
			// ENOMEM error if ss_size is too small regardless of whether SS_DISABLE is set.
			stack_t disableAltStack;
			memset(&disableAltStack, 0, sizeof(stack_t));
			disableAltStack.ss_flags = SS_DISABLE;
			disableAltStack.ss_size = SigAltStack::numBytes;
			errorUnless(!sigaltstack(&disableAltStack, nullptr));

			// Free the alt stack's memory.
			errorUnless(!munmap(base, SigAltStack::numBytes));
			base = nullptr;
		}
	}

	void init()
	{
		if(!base)
		{
			// Allocate a stack to use when handling signals, so stack overflow can be handled
			// safely.
			base = (U8*)mmap(nullptr,
							 SigAltStack::numBytes,
							 PROT_READ | PROT_WRITE,
							 MAP_PRIVATE | MAP_ANONYMOUS,
							 -1,
							 0);
			errorUnless(base != MAP_FAILED);
			stack_t sigAltStackInfo;
			sigAltStackInfo.ss_size = SigAltStack::numBytes;
			sigAltStackInfo.ss_sp = base;
			sigAltStackInfo.ss_flags = 0;
			errorUnless(!sigaltstack(&sigAltStackInfo, nullptr));
		}
	}
};

struct PlatformException
{
	void* data;
	CallStack callStack;
};

static thread_local SigAltStack sigAltStack;
static thread_local SignalContext* innermostSignalContext = nullptr;
static std::atomic<SignalHandler> portableSignalHandler;

static void deliverSignal(Signal signal, const CallStack& callStack)
{
	// Call the signal handlers, from innermost to outermost, until one returns true.
	for(SignalContext* signalContext = innermostSignalContext; signalContext;
		signalContext = signalContext->outerContext)
	{
		if(signalContext->filter(signal, callStack))
		{
			// Jump back to the execution context that was saved in catchSignals.
			siglongjmp(signalContext->catchJump, 1);
		}
	}

	// If the signal wasn't handled by a catchSignals call, call the portable signal handler.
	SignalHandler portableSignalHandlerSnapshot = portableSignalHandler.load();
	if(portableSignalHandlerSnapshot) { portableSignalHandlerSnapshot(signal, callStack); }
}

[[noreturn]] static void signalHandler(int signalNumber, siginfo_t* signalInfo, void*)
{
	Signal signal;

	// Derive the exception cause the from signal that was received.
	switch(signalNumber)
	{
	case SIGFPE:
		if(signalInfo->si_code != FPE_INTDIV && signalInfo->si_code != FPE_INTOVF)
		{ Errors::fatal("unknown SIGFPE code"); }
		signal.type = Signal::Type::intDivideByZeroOrOverflow;
		break;
	case SIGSEGV:
	case SIGBUS:
	{
		// Determine whether the faulting address was an address reserved by the stack.
		U8* stackMinAddr;
		U8* stackMaxAddr;
		getCurrentThreadStack(stackMinAddr, stackMaxAddr);
		stackMinAddr -= sysconf(_SC_PAGESIZE);
		signal.type = signalInfo->si_addr >= stackMinAddr && signalInfo->si_addr < stackMaxAddr
						  ? Signal::Type::stackOverflow
						  : Signal::Type::accessViolation;
		signal.accessViolation.address = reinterpret_cast<Uptr>(signalInfo->si_addr);
		break;
	}
	default: Errors::fatalf("unknown signal number: %i", signalNumber); break;
	};

	// Capture the execution context, omitting this function and the function that called it, so the
	// top of the callstack is the function that triggered the signal.
	CallStack callStack = captureCallStack(2);

	deliverSignal(signal, callStack);

	switch(signalNumber)
	{
	case SIGFPE: Errors::fatalf("unhandled SIGFPE\n");
	case SIGSEGV: Errors::fatalf("unhandled SIGSEGV\n");
	case SIGBUS: Errors::fatalf("unhandled SIGBUS\n");
	default: Errors::unreachable();
	};
}

static void initSignals()
{
	static bool hasInitializedSignalHandlers = false;
	if(!hasInitializedSignalHandlers)
	{
		hasInitializedSignalHandlers = true;

		// Set up a signal mask for the signals we handle that will disable them inside the handler.
		struct sigaction signalAction;
		sigemptyset(&signalAction.sa_mask);
		sigaddset(&signalAction.sa_mask, SIGFPE);
		sigaddset(&signalAction.sa_mask, SIGSEGV);
		sigaddset(&signalAction.sa_mask, SIGBUS);

		// Set the signal handler for the signals we want to intercept.
		signalAction.sa_sigaction = signalHandler;
		signalAction.sa_flags = SA_SIGINFO | SA_ONSTACK;
		sigaction(SIGSEGV, &signalAction, nullptr);
		sigaction(SIGBUS, &signalAction, nullptr);
		sigaction(SIGFPE, &signalAction, nullptr);
	}
}

bool Platform::catchSignals(const std::function<void()>& thunk,
							const std::function<bool(Signal, const CallStack&)>& filter)
{
	initSignals();
	sigAltStack.init();

	SignalContext signalContext;
	signalContext.outerContext = innermostSignalContext;
	signalContext.filter = filter;

#ifdef __WAVIX__
	Errors::fatal("catchSignals is unimplemented on Wavix");
#else
	// Use sigsetjmp to capture the execution state into the signal context. If a signal is raised,
	// the signal handler will jump back to here.
	bool isReturningFromSignalHandler = sigsetjmp(signalContext.catchJump, 1) != 0;
	if(!isReturningFromSignalHandler)
	{
		innermostSignalContext = &signalContext;

		// Call the thunk.
		thunk();
	}
	innermostSignalContext = signalContext.outerContext;

	return isReturningFromSignalHandler;
#endif
}

static void terminateHandler()
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
		deliverSignal(signal, exception.callStack);
		Errors::fatal("Unhandled runtime exception");
	}
	catch(...)
	{
		Errors::fatal("Unhandled C++ exception");
	}
}

void Platform::setSignalHandler(SignalHandler handler)
{
	initSignals();
	sigAltStack.init();

	std::set_terminate(terminateHandler);

	portableSignalHandler.store(handler);
}

CallStack Platform::captureCallStack(Uptr numOmittedFramesFromTop)
{
	CallStack result;

#if WAVM_ENABLE_RUNTIME
	unw_context_t context;
	errorUnless(!unw_getcontext(&context));

	unw_cursor_t cursor;

	errorUnless(!unw_init_local(&cursor, &context));
	while(unw_step(&cursor) > 0)
	{
		if(numOmittedFramesFromTop) { --numOmittedFramesFromTop; }
		else
		{
			unw_word_t ip;
			errorUnless(!unw_get_reg(&cursor, UNW_REG_IP, &ip));

			result.stackFrames.push_back(CallStack::Frame{ip});
		}
	}
#endif

	return result;
}

static void visitFDEs(const U8* ehFrames, Uptr numBytes, void (*visitFDE)(const void*))
{
	// The LLVM project libunwind implementation that WAVM uses expects __register_frame and
	// __deregister_frame to be called for each FDE in the .eh_frame section.
	const U8* next = ehFrames;
	const U8* end = ehFrames + numBytes;
	do
	{
		const U8* cfi = next;
		Uptr numCFIBytes = *((const U32*)next);
		next += 4;
		if(numBytes == 0xffffffff)
		{
			const U64 numCFIBytes64 = *((const U64*)next);
			errorUnless(numCFIBytes64 <= UINTPTR_MAX);
			numCFIBytes = Uptr(numCFIBytes64);
			next += 8;
		}
		const U32 cieOffset = *((const U32*)next);
		if(cieOffset != 0) { visitFDE(cfi); }

		next += numCFIBytes;
	} while(next < end);
}

void Platform::registerEHFrames(const U8* imageBase, const U8* ehFrames, Uptr numBytes)
{
	visitFDEs(ehFrames, numBytes, __register_frame);
}

void Platform::deregisterEHFrames(const U8* imageBase, const U8* ehFrames, Uptr numBytes)
{
	visitFDEs(ehFrames, numBytes, __deregister_frame);
}

bool Platform::catchPlatformExceptions(const std::function<void()>& thunk,
									   const std::function<void(void*, const CallStack&)>& handler)
{
	try
	{
		thunk();
		return false;
	}
	catch(PlatformException exception)
	{
		handler(exception.data, exception.callStack);
		if(exception.data) { free(exception.data); }
		return true;
	}
}

std::type_info* Platform::getUserExceptionTypeInfo()
{
	static std::type_info* typeInfo = nullptr;
	if(!typeInfo)
	{
		try
		{
			throw PlatformException{nullptr};
		}
		catch(PlatformException)
		{
			typeInfo = __cxxabiv1::__cxa_current_exception_type();
		}
	}
	wavmAssert(typeInfo);
	return typeInfo;
}

[[noreturn]] void Platform::raisePlatformException(void* data)
{
	throw PlatformException{data, captureCallStack(1)};
	printf("unhandled PlatformException\n");
	Errors::unreachable();
}

namespace WAVM { namespace Platform {
	struct Thread
	{
		pthread_t id;
	};
}}

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

U64 Platform::getMonotonicClock()
{
#ifdef __APPLE__
	timeval timeVal;
	gettimeofday(&timeVal, nullptr);
	return U64(timeVal.tv_sec) * 1000000 + U64(timeVal.tv_usec);
#else
	timespec monotonicClock;
	clock_gettime(CLOCK_MONOTONIC, &monotonicClock);
	return U64(monotonicClock.tv_sec) * 1000000 + U64(monotonicClock.tv_nsec) / 1000;
#endif
}

Platform::Mutex::Mutex()
{
	static_assert(sizeof(pthreadMutex) == sizeof(pthread_mutex_t), "");
	static_assert(alignof(PthreadMutex) >= alignof(pthread_mutex_t), "");
	errorUnless(!pthread_mutex_init((pthread_mutex_t*)&pthreadMutex, nullptr));
}

Platform::Mutex::~Mutex() { errorUnless(!pthread_mutex_destroy((pthread_mutex_t*)&pthreadMutex)); }

void Platform::Mutex::lock() { errorUnless(!pthread_mutex_lock((pthread_mutex_t*)&pthreadMutex)); }

void Platform::Mutex::unlock()
{
	errorUnless(!pthread_mutex_unlock((pthread_mutex_t*)&pthreadMutex));
}

Platform::Event::Event()
{
	static_assert(sizeof(pthreadMutex) == sizeof(pthread_mutex_t), "");
	static_assert(alignof(PthreadMutex) >= alignof(pthread_mutex_t), "");

	static_assert(sizeof(pthreadCond) == sizeof(pthread_cond_t), "");
	static_assert(alignof(PthreadCond) >= alignof(pthread_cond_t), "");

	pthread_condattr_t conditionVariableAttr;
	errorUnless(!pthread_condattr_init(&conditionVariableAttr));

// Set the condition variable to use the monotonic clock for wait timeouts.
#ifndef __APPLE__
	errorUnless(!pthread_condattr_setclock(&conditionVariableAttr, CLOCK_MONOTONIC));
#endif

	errorUnless(!pthread_cond_init((pthread_cond_t*)&pthreadCond, nullptr));
	errorUnless(!pthread_mutex_init((pthread_mutex_t*)&pthreadMutex, nullptr));

	errorUnless(!pthread_condattr_destroy(&conditionVariableAttr));
}

Platform::Event::~Event()
{
	pthread_cond_destroy((pthread_cond_t*)&pthreadCond);
	errorUnless(!pthread_mutex_destroy((pthread_mutex_t*)&pthreadMutex));
}

bool Platform::Event::wait(U64 untilTime)
{
	errorUnless(!pthread_mutex_lock((pthread_mutex_t*)&pthreadMutex));

	int result;
	if(untilTime == UINT64_MAX)
	{
		result = pthread_cond_wait((pthread_cond_t*)&pthreadCond, (pthread_mutex_t*)&pthreadMutex);
	}
	else
	{
		timespec untilTimeSpec;
		untilTimeSpec.tv_sec = untilTime / 1000000;
		untilTimeSpec.tv_nsec = (untilTime % 1000000) * 1000;

		result = pthread_cond_timedwait(
			(pthread_cond_t*)&pthreadCond, (pthread_mutex_t*)&pthreadMutex, &untilTimeSpec);
	}

	errorUnless(!pthread_mutex_unlock((pthread_mutex_t*)&pthreadMutex));

	if(result == ETIMEDOUT) { return false; }
	else
	{
		errorUnless(!result);
		return true;
	}
}

void Platform::Event::signal() { errorUnless(!pthread_cond_signal((pthread_cond_t*)&pthreadCond)); }

// Instead of just reinterpreting the file descriptor as a pointer, use -fd - 1, which maps fd=0 to
// a non-null value, and fd=-1 to null.
static I32 filePtrToIndex(File* ptr) { return I32(-reinterpret_cast<Iptr>(ptr) - 1); }

static File* fileIndexToPtr(int index) { return reinterpret_cast<File*>(-Iptr(index) - 1); }

File* Platform::openFile(const std::string& pathName,
						 FileAccessMode accessMode,
						 FileCreateMode createMode)
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

bool Platform::closeFile(File* file) { return close(filePtrToIndex(file)) == 0; }

File* Platform::getStdFile(StdDevice device)
{
	switch(device)
	{
	case StdDevice::in: return fileIndexToPtr(0);
	case StdDevice::out: return fileIndexToPtr(1);
	case StdDevice::err: return fileIndexToPtr(2);
	default: Errors::unreachable();
	};
}

bool Platform::seekFile(File* file, I64 offset, FileSeekOrigin origin, U64* outAbsoluteOffset)
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
	if(outAbsoluteOffset) { *outAbsoluteOffset = U64(result); }
	return result != -1;
}

bool Platform::readFile(File* file, void* outData, Uptr numBytes, Uptr* outNumBytesRead)
{
	ssize_t result = read(filePtrToIndex(file), outData, numBytes);
	if(outNumBytesRead) { *outNumBytesRead = result; }
	return result >= 0;
}

bool Platform::writeFile(File* file, const void* data, Uptr numBytes, Uptr* outNumBytesWritten)
{
	ssize_t result = write(filePtrToIndex(file), data, numBytes);
	if(outNumBytesWritten) { *outNumBytesWritten = result; }
	return result >= 0;
}

bool Platform::flushFileWrites(File* file) { return fsync(filePtrToIndex(file)) == 0; }

std::string Platform::getCurrentWorkingDirectory()
{
	const Uptr maxPathBytes = pathconf(".", _PC_PATH_MAX);
	char* buffer = (char*)alloca(maxPathBytes);
	errorUnless(getcwd(buffer, maxPathBytes) == buffer);
	return std::string(buffer);
}

#endif
