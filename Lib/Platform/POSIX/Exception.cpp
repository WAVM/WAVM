#include <cxxabi.h>
#include <signal.h>
#include <unistd.h>
#include <atomic>

#include "POSIXPrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Diagnostics.h"

using namespace WAVM;
using namespace WAVM::Platform;

struct PlatformException
{
	void* data;
	CallStack callStack;
};

thread_local SignalContext* Platform::innermostSignalContext = nullptr;
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
	case SIGFPE: Errors::fatalf("unhandled SIGFPE");
	case SIGSEGV: Errors::fatalf("unhandled SIGSEGV");
	case SIGBUS: Errors::fatalf("unhandled SIGBUS");
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
