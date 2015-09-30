#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"

#ifndef RUNTIME_API
	#define RUNTIME_API DLL_IMPORT
#endif

namespace AST { struct Module; }

namespace Runtime
{	
	struct Exception
	{
		enum class Cause
		{
			AccessViolation,
			StackOverflow,
			IntegerDivideByZero,
			IntegerOverflow
		};

		Cause cause;
		std::vector<std::string> callStack;		
	};

	// Initializes the runtime.
	RUNTIME_API bool init();

	// Adds a module to the instance.
	RUNTIME_API bool loadModule(const AST::Module* module);

	// Gets a pointer to the native code for the given function of a module.
	// If the module hasn't yet been passed to jitCompileModule, will return nullptr.
	RUNTIME_API void* getFunctionPointer(const AST::Module* module,uintptr functionIndex);
}
