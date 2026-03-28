#pragma once

#include <cstdarg>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"

namespace WAVM { namespace Platform {
	struct AssertMetadata
	{
		const char* condition;
		const char* file;
		U32 line;
	};

	WAVM_API void handleAssertionFailure(const AssertMetadata& metadata);
	[[noreturn]] WAVM_API void handleFatalError(const char* messageFormat,
												bool printCallStack,
												va_list varArgs);

	// Reports an error with a call stack dump to stderr. Non-fatal: returns to caller.
	WAVM_API void reportErrorWithCallStack(Uptr numOmittedFramesFromTop,
										   const char* messageFormat,
										   ...);

	struct UnwindState; // Forward declaration; defined in Unwind.h.

	// Callback for error reporting. Receives the pre-formatted message, whether to print
	// a call stack, and an UnwindState positioned past internal error-handling frames.
	// The handler is responsible for all output (message and call stack).
	typedef void (*ErrorHandler)(const char* message, bool printCallStack, UnwindState&& state);

	// Sets a process-wide error handler used by handleFatalError, handleAssertionFailure, and
	// reportErrorWithCallStack. If no handler is set, errors are printed to stderr.
	WAVM_API void setErrorHandler(ErrorHandler handler);
}}
