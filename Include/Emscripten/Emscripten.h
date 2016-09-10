#pragma once

#ifndef EMSCRIPTEN_API
	#define EMSCRIPTEN_API DLL_IMPORT
#endif

namespace WebAssembly { struct Module; }
namespace Runtime { struct ModuleInstance; }

namespace Emscripten
{
	EMSCRIPTEN_API void initInstance(const WebAssembly::Module& module,Runtime::ModuleInstance* moduleInstance);
	EMSCRIPTEN_API void injectCommandArgs(const std::vector<const char*>& argStrings,std::vector<Runtime::Value>& outInvokeArgs);
}