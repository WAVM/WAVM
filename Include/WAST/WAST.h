#pragma once

#ifndef WAST_API
	#define WAST_API DLL_IMPORT
#endif

#include "Inline/BasicTypes.h"
#include "Runtime/Runtime.h"
#include "WASM/WASM.h"

namespace WAST
{
	// A location in a text file.
	struct TextFileLocus
	{
		std::string sourceLine;
		uint32 newlines;
		uint32 tabs;
		uint32 characters;

		TextFileLocus(): newlines(0), tabs(0), characters(0) {}

		uint32 lineNumber() const { return newlines + 1; }
		uint32 column(uint32 spacesPerTab = 4) const { return tabs * spacesPerTab + characters + 1; }

		std::string describe(uint32 spacesPerTab = 4) const
		{
			return std::to_string(lineNumber()) + ":" + std::to_string(column(spacesPerTab));
		}
	};

	// A WAST parse error.
	struct Error
	{
		TextFileLocus locus;
		std::string message;
	};

	// Parse a module from a string. Returns true if it succeeds, and writes the module to outModule.
	// If it fails, returns false and appends a list of errors to outErrors.
	WAST_API bool parseModule(const char* string,size_t stringLength,IR::Module& outModule,std::vector<Error>& outErrors);

	// Prints a module in WAST format.
	WAST_API std::string print(const IR::Module& module);
}