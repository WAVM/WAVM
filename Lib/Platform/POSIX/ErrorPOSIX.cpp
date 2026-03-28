#if WAVM_PLATFORM_POSIX

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include "POSIXPrivate.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/StringBuilder.h"
#include "WAVM/Platform/Error.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Platform/Unwind.h"

using namespace WAVM;
using namespace WAVM::Platform;

static Mutex& getErrorReportingMutex()
{
	static Platform::Mutex mutex;
	return mutex;
}

static std::atomic<ErrorHandler> currentErrorHandler{nullptr};

void Platform::setErrorHandler(ErrorHandler handler)
{
	currentErrorHandler.store(handler, std::memory_order_release);
	if(handler) { initGlobalSignals(); }
}

// Dispatches to the error handler or falls back to printing the message to stderr.
// numOmittedFramesFromTop counts frames above the caller to skip when capturing the UnwindState.
static void reportError(const char* message, bool printCallStack, Uptr numOmittedFramesFromTop)
{
	ErrorHandler handler = currentErrorHandler.load(std::memory_order_acquire);
	if(handler)
	{
		// +1 to skip reportError itself.
		UnwindState state = UnwindState::capture(numOmittedFramesFromTop + 1);
		handler(message, printCallStack, std::move(state));
	}
	else
	{
		std::fprintf(stderr, "%s\n", message);
		std::fflush(stderr);
	}
}

void Platform::handleFatalError(const char* messageFormat, bool printCallStack, va_list varArgs)
{
	Platform::Mutex::Lock lock(getErrorReportingMutex());
	TruncatingFixedStringBuilder<4096> message;
	message.vappendf(messageFormat, varArgs);
	reportError(message.c_str(), printCallStack, 1);
	std::abort();
}

void Platform::handleAssertionFailure(const AssertMetadata& metadata)
{
	Platform::Mutex::Lock lock(getErrorReportingMutex());
	TruncatingFixedStringBuilder<4096> message;
	message.appendf(
		"Assertion failed at %s(%u): %s", metadata.file, metadata.line, metadata.condition);
	reportError(message.c_str(), true, 1);
}

void Platform::reportErrorWithCallStack(Uptr numOmittedFramesFromTop,
										const char* messageFormat,
										...)
{
	Platform::Mutex::Lock lock(getErrorReportingMutex());
	TruncatingFixedStringBuilder<4096> message;
	va_list varArgs;
	va_start(varArgs, messageFormat);
	message.vappendf(messageFormat, varArgs);
	va_end(varArgs);
	reportError(message.c_str(), true, numOmittedFramesFromTop + 1);
}

#endif // WAVM_PLATFORM_POSIX
