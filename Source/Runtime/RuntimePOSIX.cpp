#ifndef _WIN32

#include "Core/Core.h"
#include "RuntimePrivate.h"

namespace RuntimePlatform
{
	using namespace Runtime;

	Value catchRuntimeExceptions(const std::function<Value()>& thunk)
	{
		return thunk();
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