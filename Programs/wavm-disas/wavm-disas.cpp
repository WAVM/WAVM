#include <stdlib.h>
#include <string>

#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTPrint/WASTPrint.h"

using namespace WAVM;

static bool loadBinaryModuleFromFile(const char* filename,
									 IR::Module& outModule,
									 Log::Category errorCategory = Log::error)
{
	std::vector<U8> wasmBytes;
	if(!loadFile(filename, wasmBytes)) { return false; }
	return WASM::loadBinaryModule(wasmBytes.data(), wasmBytes.size(), outModule);
}

int main(int argc, char** argv)
{
	const char* inputFilename = nullptr;
	const char* outputFilename = nullptr;

	bool showHelp = false;
	bool enableQuotedNames = false;
	if(argc < 3 || argc > 4) { showHelp = true; }
	else
	{
		inputFilename = argv[1];
		outputFilename = argv[2];
		if(!strcmp(argv[1], "--help")) { showHelp = true; }
		else if(argc == 4)
		{
			if(!strcmp(argv[3], "--enable-quoted-names")) { enableQuotedNames = true; }
			else
			{
				showHelp = true;
			}
		}
	}

	if(showHelp)
	{
		Log::printf(Log::error, "Usage: wavm-disas in.wasm out.wast [--enable-quoted-names]\n");
		return EXIT_FAILURE;
	}

	// Load the WASM file.
	IR::Module module;
	module.featureSpec.quotedNamesInTextFormat = enableQuotedNames;
	if(!loadBinaryModuleFromFile(inputFilename, module)) { return EXIT_FAILURE; }

	// Print the module to WAST.
	Timing::Timer printTimer;
	const std::string wastString = WAST::print(module);
	Timing::logRatePerSecond(
		"Printed WAST", printTimer, F64(wastString.size()) / 1024.0 / 1024.0, "MB");

	// Write the serialized data to the output file.
	return saveFile(outputFilename, wastString.data(), wastString.size()) ? EXIT_SUCCESS
																		  : EXIT_FAILURE;
}
