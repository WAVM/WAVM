#include "IR/Module.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Linker.h"
#include "Runtime/Runtime.h"
#include "StubResolver.h"
#include "WASM/WASM.h"
#include "WAST/TestScript.h"
#include "WAST/WAST.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

using namespace WAST;
using namespace IR;
using namespace Runtime;

namespace LLVMJIT
{
	RUNTIME_API void deinit();
}

inline bool loadBinaryModule(const std::string& wasmBytes, Module& outModule)
{
	// Load the module from a binary WebAssembly file.
	try
	{
		Serialization::MemoryInputStream stream((const U8*)wasmBytes.data(), wasmBytes.size());
		WASM::serialize(stream, outModule);
		return true;
	}
	catch(Serialization::FatalSerializationException exception)
	{
		return false;
	}
	catch(ValidationException exception)
	{
		return false;
	}
	catch(std::bad_alloc)
	{
		return false;
	}
}

inline bool loadTextModule(const std::string& wastString, IR::Module& outModule)
{
	try
	{
		std::vector<WAST::Error> parseErrors;
		WAST::parseModule(wastString.c_str(), wastString.size(), outModule, parseErrors);
		return !parseErrors.size();
	}
	catch(...)
	{
		Log::printf(Log::error, "unknown exception!\n");
		return false;
	}
}

inline bool loadModule(const std::string& dataString, IR::Module& outModule)
{
	return loadBinaryModule(dataString, outModule);
}

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals            = 1024;
	if(!loadBinaryModule(std::string((const char*)data, numBytes), module)) { return 0; }

	Compartment* compartment = createCompartment();
	StubResolver stubResolver(compartment);
	LinkResult linkResult = linkModule(module, stubResolver);
	if(linkResult.success)
	{
		catchRuntimeExceptions(
			[&] {
				instantiateModule(
					compartment, module, std::move(linkResult.resolvedImports), "fuzz");
			},
			[&](Exception&& exception) {});

		collectGarbage();
	}

	// De-initialize LLVM to avoid the accumulation of de-duped debug metadata in the LLVMContext.
	LLVMJIT::deinit();

	return 0;
}

#if !ENABLE_LIBFUZZER
I32 main()
{
	LLVMFuzzerTestOneInput((const U8*)"", 0);
	return EXIT_SUCCESS;
}
#endif
