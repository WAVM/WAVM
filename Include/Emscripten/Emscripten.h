#pragma once

#include "Runtime/Runtime.h"

#ifndef EMSCRIPTEN_API
#define EMSCRIPTEN_API DLL_IMPORT
#endif

#include <vector>

namespace IR
{
	struct Module;
}

namespace Emscripten
{
	struct Instance
	{
		Runtime::GCPointer<Runtime::ModuleInstance> env;
		Runtime::GCPointer<Runtime::ModuleInstance> asm2wasm;
		Runtime::GCPointer<Runtime::ModuleInstance> global;

		Runtime::GCPointer<Runtime::MemoryInstance> emscriptenMemory;
	};

	EMSCRIPTEN_API Instance* instantiate(
		Runtime::Compartment* compartment,
		const IR::Module& module);
	EMSCRIPTEN_API void initializeGlobals(
		Runtime::Context* context,
		const IR::Module& module,
		Runtime::ModuleInstance* moduleInstance);
	EMSCRIPTEN_API void injectCommandArgs(
		Emscripten::Instance* instance,
		const std::vector<const char*>& argStrings,
		std::vector<IR::Value>& outInvokeArgs);
}