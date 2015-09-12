#pragma once

#include "Core/Core.h"
#include "AST/AST.h"

#include <vector>

namespace WebAssemblyText
{
	struct File
	{
		std::vector<AST::Module*> modules;
		std::vector<AST::ErrorRecord*> errors;
	};

	bool parse(const char* string,File& outFile);
	std::string print(const AST::Module* module);
}

namespace WebAssemblyBinary
{
	bool decode(const uint8_t* data,size_t numBytes,AST::Module*& outModule,std::vector<AST::ErrorRecord*>& outErrors);
}
