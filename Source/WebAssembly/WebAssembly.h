#pragma once

#include "Core/Core.h"
#include "AST/AST.h"

#include <vector>

#ifndef WEBASSEMBLY_API
	#define WEBASSEMBLY_API DLL_IMPORT
#endif

namespace WebAssemblyText
{
	struct AssertEq
	{
		AST::Module* invokeDummyModule;
		AST::Module* invokeFunctionModule;
		uintptr invokeFunctionIndex;
		std::vector<AST::TypedExpression> parameters;
		AST::TypedExpression value;
		Core::TextFileLocus locus;
	};

	struct File
	{
		std::vector<AST::Module*> modules;
		std::vector<AST::ErrorRecord*> errors;

		std::vector<AssertEq> assertEqs;
	};

	WEBASSEMBLY_API bool parse(const char* string,File& outFile);
	WEBASSEMBLY_API std::string print(const AST::Module* module);
}

namespace WebAssemblyBinary
{
	WEBASSEMBLY_API bool decode(
		const uint8* code,
		size_t numCodeBytes,
		const uint8* data,
		size_t numDataBytes,
		AST::Module*& outModule,
		std::vector<AST::ErrorRecord*>& outErrors
		);
}
