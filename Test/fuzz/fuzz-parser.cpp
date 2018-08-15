#include "IR/Module.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "WASM/WASM.h"
#include "WAST/TestScript.h"
#include "WAST/WAST.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

using namespace WAST;
using namespace IR;

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
		Log::printf(Log::Category::error, "unknown exception!\n");
		return false;
	}
}

inline bool loadModule(const std::string& dataString, IR::Module& outModule)
{
	// If the file starts with the WASM binary magic number, load it as a binary module.
	if(*(U32*)dataString.data() == 0x6d736100) { return loadBinaryModule(dataString, outModule); }
	else
	{
		// Otherwise, load it as a text module.
		return loadTextModule(dataString, outModule);
	}
}

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	Module module;
	loadModule(std::string((const char*)data, numBytes), module);
	return 0;
}

#if !ENABLE_LIBFUZZER
I32 main()
{
	LLVMFuzzerTestOneInput((const U8*)"", 0);
	return EXIT_SUCCESS;
}
#endif
