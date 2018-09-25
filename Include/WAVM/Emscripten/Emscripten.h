#pragma once

#include <vector>

#include "WAVM/IR/Value.h"
#include "WAVM/Runtime/Runtime.h"

namespace WAVM { namespace IR {
	struct Module;
}}

namespace WAVM { namespace Emscripten {
	struct Instance
	{
		Runtime::GCPointer<Runtime::ModuleInstance> env;
		Runtime::GCPointer<Runtime::ModuleInstance> asm2wasm;
		Runtime::GCPointer<Runtime::ModuleInstance> global;

		Runtime::GCPointer<Runtime::Memory> emscriptenMemory;
	};

	EMSCRIPTEN_API Instance* instantiate(Runtime::Compartment* compartment,
										 const IR::Module& module);
	EMSCRIPTEN_API void initializeGlobals(Runtime::Context* context,
										  const IR::Module& module,
										  Runtime::ModuleInstance* moduleInstance);
	EMSCRIPTEN_API void injectCommandArgs(Emscripten::Instance* instance,
										  const std::vector<const char*>& argStrings,
										  std::vector<IR::Value>& outInvokeArgs);
}}
