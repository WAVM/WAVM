#if WAVM_PLATFORM_POSIX

#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/signal.h>
#include <utility>
#include "POSIXPrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Signal.h"
#include "WAVM/Platform/Unwind.h"

using namespace WAVM;
using namespace WAVM::Platform;

#if defined(__APPLE__)
#define UC_RESET_ALT_STACK 0x80000000
extern "C" int __sigreturn(ucontext_t*, int);
#endif

thread_local SignalContext* Platform::innermostSignalContext = nullptr;

struct ScopedSignalContext : SignalContext
{
	bool isLinked = false;

	void link()
	{
		outerContext = innermostSignalContext;
		innermostSignalContext = this;
		isLinked = true;
	}

	~ScopedSignalContext()
	{
		if(isLinked)
		{
			innermostSignalContext = outerContext;
			isLinked = false;
		}
	}
};

static void maskSignals(int how)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGFPE);
	sigaddset(&set, SIGSEGV);
	sigaddset(&set, SIGBUS);
	pthread_sigmask(how, &set, nullptr);
}

[[noreturn]] static void signalHandler(int signalNumber, siginfo_t* signalInfo, void* contextPtr)
{
	maskSignals(SIG_BLOCK);

	Signal signal;

	// Derive the exception cause the from signal that was received.
	switch(signalNumber)
	{
	case SIGFPE:
		if(signalInfo->si_code != FPE_INTDIV && signalInfo->si_code != FPE_INTOVF)
		{
			Errors::fatalfWithCallStack("unknown SIGFPE code");
		}
		signal.type = Signal::Type::intDivideByZeroOrOverflow;
		break;
	case SIGSEGV:
	case SIGBUS: {
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

	// Call the signal filters, from innermost to outermost, until one returns true.
	{
		UnwindState state = makeUnwindStateFromSignalContext(contextPtr);
		for(SignalContext* signalContext = innermostSignalContext; signalContext;
			signalContext = signalContext->outerContext)
		{
			if(signalContext->filter(signalContext->filterArgument, signal, std::move(state)))
			{
				// siglongjmp won't unwind the stack, so manually call the UnwindState destructor.
				state.~UnwindState();

				// Jump back to the execution context that was saved in catchSignals.
				siglongjmp(signalContext->catchJump, 1);
			}
		}
	}

	switch(signalNumber)
	{
	case SIGFPE: Errors::fatalfWithCallStack("unhandled SIGFPE");
	case SIGSEGV: Errors::fatalfWithCallStack("unhandled SIGSEGV");
	case SIGBUS: Errors::fatalfWithCallStack("unhandled SIGBUS");
	default: WAVM_UNREACHABLE();
	};
}

bool Platform::initGlobalSignalsOnce()
{
	// Set the signal handler for the signals we want to intercept.
	struct sigaction signalAction;
	signalAction.sa_sigaction = signalHandler;
	signalAction.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
	sigemptyset(&signalAction.sa_mask);
	WAVM_ERROR_UNLESS(!sigaction(SIGSEGV, &signalAction, nullptr));
	WAVM_ERROR_UNLESS(!sigaction(SIGBUS, &signalAction, nullptr));
	WAVM_ERROR_UNLESS(!sigaction(SIGFPE, &signalAction, nullptr));

	return true;
}

bool Platform::catchSignals(void (*thunk)(void*),
							bool (*filter)(void*, Signal, UnwindState&&),
							void* argument)
{
	initThreadAndGlobalSignals();

	ScopedSignalContext signalContext;
	signalContext.filter = filter;
	signalContext.filterArgument = argument;

	// Use sigsetjmp to capture the execution state into the signal context. If a signal is raised,
	// the signal handler will jump back to here. Tell sigsetjmp not to save the signal mask, since
	// that's quite expensive (a syscall). Instead, just unblock the signals that our handler blocks
	// after handling those signals.
	bool isReturningFromSignalHandler = sigsetjmp(signalContext.catchJump, 0) != 0;
	if(!isReturningFromSignalHandler)
	{
		signalContext.link();

		// Call the thunk.
		thunk(argument);
	}
	else
	{
#if defined(__APPLE__)
		// On MacOS, it's necessary to call __sigreturn to restore the sigaltstack state after
		// exiting the signal handler.
		__sigreturn(nullptr, UC_RESET_ALT_STACK);
#endif

		// Unblock the signals that are blocked by the signal handler.
		maskSignals(SIG_UNBLOCK);
	}

	return isReturningFromSignalHandler;
}

#endif // WAVM_PLATFORM_POSIX
