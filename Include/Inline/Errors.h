#pragma once

#include "Platform/Platform.h"

#include <cstdarg>

namespace Errors
{
	// Fatal error handling.
	[[noreturn]] inline void fatalf(const char* messageFormat,...)
	{
		va_list varArgs;
		va_start(varArgs,messageFormat);
		Platform::handleFatalError(messageFormat,varArgs);
	}
	[[noreturn]] inline void fatal(const char* message) { fatalf("%s\n",message); }
	[[noreturn]] inline void unreachable() { fatalf("reached unreachable code\n"); }
	[[noreturn]] inline void unimplemented(const char* context) { fatalf("unimplemented: %s\n",context); }
}

