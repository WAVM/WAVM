#pragma once

#include "Core/Core.h"
#include "AST/AST.h"

#include <vector>

namespace WebAssemblyText
{
	struct AssertEq
	{
		AST::Module* invokeDummyModule;
		AST::Module* invokeFunctionModule;
		uintptr_t invokeFunctionIndex;
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

	bool parse(const char* string,File& outFile);
	std::string print(const AST::Module* module);
}

namespace WebAssemblyBinary
{
	bool decode(const uint8* data,size_t numBytes,AST::Module*& outModule,std::vector<AST::ErrorRecord*>& outErrors);
}
