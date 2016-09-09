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
	struct ParseError
	{
		Core::TextFileLocus locus;
		std::string message;

		ParseError(const Core::TextFileLocus& inLocus,std::string&& inMessage): locus(inLocus), message(inMessage) {}
	};

	WAST_API bool parseModule(const char* string,WebAssembly::Module& outModule,std::vector<ParseError>& outErrors);
	WAST_API bool parseModule(SExp::SNodeIt firstNonNameChildNodeIt,WebAssembly::Module& outModule,std::vector<ParseError>& outErrors);
	WAST_API std::string print(const WebAssembly::Module& module);
}