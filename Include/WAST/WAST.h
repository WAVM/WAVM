#pragma once

#ifndef WAST_API
	#define WAST_API DLL_IMPORT
#endif

#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "Runtime/Runtime.h"
#include "WebAssembly/WebAssembly.h"

namespace SExp { struct SNodeIt; }

namespace WAST
{
	struct Error
	{
		Core::TextFileLocus locus;
		std::string message;

		Error(const Core::TextFileLocus& inLocus,std::string&& inMessage): locus(inLocus), message(inMessage) {}
	};

	// Parse a module from a string or S-expression subtree. Returns true if it succeeds, and writes the module to outModule.
	// If it fails, returns false and appends a list of errors to outErrors.
	WAST_API bool parseModule(const char* string,WebAssembly::Module& outModule,std::vector<Error>& outErrors);
	WAST_API bool parseModule(SExp::SNodeIt firstNonNameChildNodeIt,WebAssembly::Module& outModule,std::vector<Error>& outErrors);

	// Prints a module in WAST format.
	WAST_API std::string print(const WebAssembly::Module& module);
}