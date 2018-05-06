#pragma once

#ifndef EMSCRIPTEN_API
	#define EMSCRIPTEN_API DLL_IMPORT
#endif

#include <vector>

namespace IR { struct Module; }
namespace Runtime { struct ModuleInstance; struct Context; struct Compartment; }

namespace Emscripten
{
	using namespace Runtime;

	struct Instance
	{
		GCPointer<ModuleInstance> env;
		GCPointer<ModuleInstance> asm2wasm;
		GCPointer<ModuleInstance> global;

		GCPointer<MemoryInstance> emscriptenMemory;
	};

	EMSCRIPTEN_API Instance* instantiate(Compartment* compartment, const IR::Module& module);
	EMSCRIPTEN_API void initializeGlobals(Context* context,const IR::Module& module,ModuleInstance* moduleInstance);
	EMSCRIPTEN_API void injectCommandArgs(Emscripten::Instance* instance,const std::vector<const char*>& argStrings,std::vector<Value>& outInvokeArgs);
}