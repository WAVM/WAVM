#pragma once

#include "Core/Core.h"
#include "AST/AST.h"
#include "Runtime/Runtime.h"

#include <vector>

#ifndef WEBASSEMBLY_API
	#define WEBASSEMBLY_API DLL_IMPORT
#endif

namespace WebAssemblyText
{
	enum class TestOp
	{
		Invoke,
		Assert,
		AssertNaN,
	};

	struct TestStatement
	{
		TestOp op;
		Core::TextFileLocus locus;
	};

	struct Invoke : TestStatement
	{
		uintptr functionIndex;
		std::vector<Runtime::Value> parameters;
		Invoke() : TestStatement({TestOp::Invoke}) {}
	};

	struct Assert : TestStatement
	{
		Invoke* invoke;
		Runtime::Value value;
		Assert() : TestStatement({TestOp::Assert}) {}
	};

	struct AssertNaN : TestStatement
	{
		Invoke* invoke;
		AssertNaN() : TestStatement({TestOp::AssertNaN}) {}
	};
	
	struct File
	{
		std::vector<AST::Module*> modules;
		std::vector<AST::ErrorRecord*> errors;

		std::vector<std::vector<TestStatement*>> moduleTests;
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
