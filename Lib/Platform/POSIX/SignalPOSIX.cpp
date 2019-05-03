#include <signal.h>
#include <unistd.h>
#include <atomic>

#include "POSIXPrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Diagnostics.h"

using namespace WAVM;
using namespace WAVM::Platform;

thread_local SignalContext* Platform::innermostSignalContext = nullptr;

[[noreturn]] static void signalHandler(int signalNumber, siginfo_t* signalInfo, void*)
{
	Signal signal;

	// Derive the exception cause the from signal that was received.
	switch(signalNumber)
	{
	case SIGFPE:
		if(signalInfo->si_code != FPE_INTDIV && signalInfo->si_code != FPE_INTOVF)
		{ Errors::fatalfWithCallStack("unknown SIGFPE code"); }
		signal.type = Signal::Type::intDivideByZeroOrOverflow;
		break;
	case SIGSEGV:
	case SIGBUS:
	{
		// Determine whether the faulting address was an address reserved by the stack.
		U8* stackMinGuardAddr;
		U8* stackMinAddr;
		U8* stackMaxAddr;
		sigAltStack.getNonSignalStack(stackMinGuardAddr, stackMinAddr, stackMaxAddr);
		signal.type = signalInfo->si_addr >= stackMinGuardAddr && signalInfo->si_addr < stackMaxAddr
						  ? Signal::Type::stackOverflow
						  : Signal::Type::accessViolation;
		signal.accessViolation.address = reinterpret_cast<Uptr>(signalInfo->si_addr);
		break;
	}
	default: Errors::fatalfWithCallStack("unknown signal number: %i", signalNumber); break;
	};

	// Capture the execution context, omitting this function and the function that called it, so the
	// top of the callstack is the function that triggered the signal.
	CallStack callStack = captureCallStack(2);

	// Call the signal handlers, from innermost to outermost, until one returns true.
	for(SignalContext* signalContext = innermostSignalContext; signalContext;
		signalContext = signalContext->outerContext)
	{
		if(signalContext->filter(signalContext->filterArgument, signal, std::move(callStack)))
		{
			// siglongjmp won't unwind the stack, so manually call the CallStack destructor.
			callStack.~CallStack();

			// Jump back to the execution context that was saved in catchSignals.
			siglongjmp(signalContext->catchJump, 1);
		}
	}

	switch(signalNumber)
	{
	case SIGFPE: Errors::fatalfWithCallStack("unhandled SIGFPE");
	case SIGSEGV: Errors::fatalfWithCallStack("unhandled SIGSEGV");
	case SIGBUS: Errors::fatalfWithCallStack("unhandled SIGBUS");
	default: Errors::unreachable();
	};
}

static bool initSignals()
{
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

	return true;
}

bool Platform::catchSignals(void (*thunk)(void*),
							bool (*filter)(void*, Signal, CallStack&&),
							void* argument)
{
	static bool initedSignals = initSignals();
	wavmAssert(initedSignals);

	sigAltStack.init();

	SignalContext signalContext;
	signalContext.outerContext = innermostSignalContext;
	signalContext.filter = filter;
	signalContext.filterArgument = argument;

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
		thunk(argument);
	}
	innermostSignalContext = signalContext.outerContext;

	return isReturningFromSignalHandler;
#endif
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
