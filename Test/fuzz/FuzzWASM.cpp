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

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals            = 1024;
	loadBinaryModule(std::string((const char*)data, numBytes), module);
	return 0;
}

#if !ENABLE_LIBFUZZER
I32 main()
{
	LLVMFuzzerTestOneInput((const U8*)"", 0);
	return EXIT_SUCCESS;
}
#endif
