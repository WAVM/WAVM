#include "IR/Module.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "ModuleMatcher.h"
#include "WASM/WASM.h"
#include "WASTParse/TestScript.h"
#include "WASTParse/WASTParse.h"

#include <string>

using namespace IR;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	// Copy the data to a std::vector and append a null terminator.
	std::vector<U8> wastBytes(data, data + numBytes);
	wastBytes.push_back(0);

	Module wastModule;
	std::vector<WAST::Error> parseErrors;
	if(WAST::parseModule((const char*)wastBytes.data(), wastBytes.size(), wastModule, parseErrors))
	{
		std::vector<U8> wasmBytes;
		try
		{
			Serialization::ArrayOutputStream stream;
			WASM::serialize(stream, wastModule);
			wasmBytes = stream.getBytes();
		}
		catch(Serialization::FatalSerializationException exception)
		{
			return 0;
		}

		Module wasmModule;
		if(!loadBinaryModule(wasmBytes.data(), wasmBytes.size(), wasmModule))
		{ Errors::fatal("Failed to deserialize the generated WASM file"); }

		ModuleMatcher moduleMatcher(wastModule, wasmModule);
		moduleMatcher.verify();
	}

	return 0;
}

#if !ENABLE_LIBFUZZER

#include <cstring>

I32 main(int argc, char** argv)
{
	if(argc != 2)
	{
		Log::printf(Log::error, "Usage: FuzzAssemble in.wast\n");
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];

	std::vector<U8> wastBytes;
	if(!loadFile(inputFilename, wastBytes)) { return EXIT_FAILURE; }

	LLVMFuzzerTestOneInput(wastBytes.data(), wastBytes.size());
	return EXIT_SUCCESS;
}
#endif
