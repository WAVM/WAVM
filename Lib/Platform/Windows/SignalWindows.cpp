#if WAVM_PLATFORM_WINDOWS

#include <utility>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Alloca.h"
#include "WAVM/Platform/Signal.h"
#include "WAVM/Platform/Unwind.h"
#include "WindowsPrivate.h"

#include <excpt.h>

using namespace WAVM;
using namespace WAVM::Platform;

static bool translateSEHToSignal(EXCEPTION_POINTERS* exceptionPointers, Signal& outSignal)
{
	// Decide how to handle this exception code.
	switch(exceptionPointers->ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION: {
		outSignal.type = Signal::Type::accessViolation;
		outSignal.accessViolation.address
			= exceptionPointers->ExceptionRecord->ExceptionInformation[1];
		return true;
	}
	case EXCEPTION_STACK_OVERFLOW: outSignal.type = Signal::Type::stackOverflow; return true;
	case STATUS_INTEGER_DIVIDE_BY_ZERO:
		outSignal.type = Signal::Type::intDivideByZeroOrOverflow;
		return true;
	case STATUS_INTEGER_OVERFLOW:
		outSignal.type = Signal::Type::intDivideByZeroOrOverflow;
		return true;
	default: return false;
	}
}

// __try/__except doesn't support locals with destructors in the same function, so this is just
// the body of the sehSignalFilterFunction __try pulled out into a function.
static LONG CALLBACK sehSignalFilterFunctionNonReentrant(EXCEPTION_POINTERS* exceptionPointers,
														 bool (*filter)(void*,
																		Signal,
																		UnwindState&&),
														 void* context)
{
	Signal signal;
	if(!translateSEHToSignal(exceptionPointers, signal)) { return EXCEPTION_CONTINUE_SEARCH; }
	else
	{
		// Create an unwind state from the exception context.
		UnwindState state = makeUnwindStateFromSignalContext(exceptionPointers->ContextRecord);

		if((*filter)(context, signal, std::move(state))) { return EXCEPTION_EXECUTE_HANDLER; }
		else
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}
}

static LONG CALLBACK sehSignalFilterFunction(EXCEPTION_POINTERS* exceptionPointers,
											 bool (*filter)(void*, Signal, UnwindState&&),
											 void* context)
{
	__try
	{
		return sehSignalFilterFunctionNonReentrant(exceptionPointers, filter, context);
	}
	__except(Errors::fatal("reentrant exception"), true)
	{
		WAVM_UNREACHABLE();
	}
}

bool Platform::catchSignals(void (*thunk)(void*),
							bool (*filter)(void*, Signal, UnwindState&&),
							void* context)
{
	initThread();

	__try
	{
		(*thunk)(context);
		return false;
	}
	__except(sehSignalFilterFunction(GetExceptionInformation(), filter, context))
	{
		// After a stack overflow, the stack will be left in a damaged state. Let the CRT repair it.
		WAVM_ERROR_UNLESS(_resetstkoflw());

		return true;
	}
}

#endif // WAVM_PLATFORM_WINDOWS
