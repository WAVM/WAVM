#include <stdlib.h>
#include <string>
#include <vector>

#include "IR/IR.h"
#include "IR/Module.h"
#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
#include "Inline/Errors.h"
#include "Logging/Logging.h"
#include "ModuleMatcher.h"
#include "WASTParse/WASTParse.h"
#include "WASTPrint/WASTPrint.h"

using namespace WAVM;
using namespace WAVM::IR;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals = 1024;
	if(loadBinaryModule(data, numBytes, module, Log::debug))
	{
		const std::string wastString = WAST::print(module);

		Module wastModule;
		std::vector<WAST::Error> parseErrors;
		if(!WAST::parseModule(
			   (const char*)wastString.c_str(), wastString.size() + 1, wastModule, parseErrors))
		{
			reportParseErrors("disassembly", parseErrors);
			Errors::unreachable();
		}

		ModuleMatcher moduleMatcher(module, wastModule);
		moduleMatcher.verify();
	}

	return 0;
}

#if !ENABLE_LIBFUZZER

I32 main(int argc, char** argv)
{
	if(argc != 2)
	{
		Log::printf(Log::error, "Usage: FuzzDisassemble in.wasm\n");
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];

	std::vector<U8> wasmBytes;
	if(!loadFile(inputFilename, wasmBytes)) { return EXIT_FAILURE; }

	LLVMFuzzerTestOneInput(wasmBytes.data(), wasmBytes.size());
	return EXIT_SUCCESS;
}
#endif
