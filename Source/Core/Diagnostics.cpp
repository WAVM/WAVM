#include "Core.h"
#include "Platform.h"

#include <cstdio>
#include <cstdlib>

namespace Core
{
	[[noreturn]] void fatalError(const char* message)
	{
		std::printf("fatal error: %s\n",message);
		std::fflush(stdout);
		std::abort();
	}

	[[noreturn]] void unreachable()
	{
		fatalError("reached unreachable code");
	}
}
