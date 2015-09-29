#ifndef _WIN32

#include "Core/Core.h"
#include "RuntimePrivate.h"

namespace RuntimePlatform
{
	void initGlobalExceptionHandlers()
	{
		// todo
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