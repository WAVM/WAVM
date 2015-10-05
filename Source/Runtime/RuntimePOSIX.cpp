#ifndef _WIN32

#include "Core/Core.h"
#include "RuntimePrivate.h"

#include <signal.h>
#include <setjmp.h>

namespace RuntimePlatform
{
	using namespace Runtime;

	THREAD_LOCAL jmp_buf setjmpEnv;
	THREAD_LOCAL Exception::Cause exceptionCause = Exception::Cause::Unknown;

	void signalHandler(int signalNumber,siginfo_t* signalInfo,void*)
	{
		// Derive the exception cause the from signal that was received.
		exceptionCause = Exception::Cause::Unknown;
		switch(signalNumber)
		{
		case SIGFPE:
			switch(signalInfo->si_code)
			{
				case FPE_INTDIV: exceptionCause = Exception::Cause::IntegerDivideByZero; break;
			};
			break;
		case SIGSEGV: exceptionCause = Exception::Cause::AccessViolation; break;
		};

		// Jump back to the setjmp in catchRuntimeExceptions.
		longjmp(setjmpEnv,1);
	}

	Value catchRuntimeExceptions(const std::function<Value()>& thunk)
	{
		Runtime::Value result;

		struct sigaction oldSignalActionSEGV;
		struct sigaction oldSignalActionFPE;

		// Use setjmp to allow signals to jump
		if(!setjmp(setjmpEnv))
		{
			// Set a signal handler for the signals we want to intercept.
			struct sigaction signalAction;
			signalAction.sa_sigaction = signalHandler;
			sigemptyset(&signalAction.sa_mask);
			// SA_NODEFER is needed to keep the signal enabled after its first use, since the
			// signal handler uses longjmp instead of returning normally.
			signalAction.sa_flags = SA_SIGINFO | SA_NODEFER;
			sigaction(SIGSEGV,&signalAction,&oldSignalActionSEGV);
			sigaction(SIGFPE,&signalAction,&oldSignalActionFPE);

			// Call the thunk.
			result = thunk();
		}
		else
		{
			result = Value(new Exception {exceptionCause});
		}

		// Reset the signals.
		sigaction(SIGSEGV,&oldSignalActionSEGV,nullptr);
		sigaction(SIGFPE,&oldSignalActionFPE,nullptr);

		return result;
	}

	bool describeInstructionPointer(uintptr_t ip,std::string& outDescription)
	{
		return false;
	}

	Runtime::ExecutionContext captureExecutionContext()
	{
		return Runtime::ExecutionContext();
	}
}

#endif