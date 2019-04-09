#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Runtime/Runtime.h"

namespace WAVM { namespace WASI {
	enum class RunResult
	{
		success = 0,
		linkError,
		noStartFunction,
		mistypedStartFunction
	};
	WASI_API RunResult run(Runtime::ModuleConstRefParam module,
						   std::vector<std::string>&& inArgs,
						   std::vector<std::string>&& inEnvs,
						   I32& outExitCode);

	WASI_API void setTraceSyscalls(bool newTraceSyscalls);
}}
