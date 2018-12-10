#include <stdlib.h>
#include <vector>

#include "ModuleMatcher.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;

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
		catch(Serialization::FatalSerializationException const&)
		{
			return 0;
		}

		Module wasmModule;
		if(!WASM::loadBinaryModule(wasmBytes.data(), wasmBytes.size(), wasmModule))
		{ Errors::fatal("Failed to deserialize the generated WASM file"); }

		ModuleMatcher moduleMatcher(wastModule, wasmModule);
		moduleMatcher.verify();
	}

	return 0;
}

#if !WAVM_ENABLE_LIBFUZZER

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
