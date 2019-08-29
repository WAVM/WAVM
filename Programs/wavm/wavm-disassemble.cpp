#include <stdlib.h>
#include <string>
#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTPrint/WASTPrint.h"
#include "wavm.h"

using namespace WAVM;

static bool loadBinaryModuleFromFile(const char* filename,
									 IR::Module& outModule,
									 Log::Category errorCategory = Log::error)
{
	std::vector<U8> wasmBytes;
	if(!loadFile(filename, wasmBytes)) { return false; }
	Serialization::MemoryInputStream inputStream(wasmBytes.data(), wasmBytes.size());
	WASM::LoadError loadError;
	if(WASM::loadBinaryModule(inputStream, outModule, &loadError)) { return true; }
	else
	{
		Log::printf(Log::error, "%s\n", loadError.message.c_str());
		return false;
	}
}

void showDisassembleHelp(Log::Category outputCategory)
{
	Log::printf(Log::error, "Usage: wavm disassemble in.wasm [out.wast] [--enable-quoted-names]\n");
}

int execDisassembleCommand(int argc, char** argv)
{
	const char* inputFilename = nullptr;
	const char* outputFilename = nullptr;

	bool showHelp = false;
	bool enableQuotedNames = false;
	if(argc < 1) { showHelp = true; }
	else
	{
		for(int argIndex = 0; argIndex < argc; ++argIndex)
		{
			if(!strcmp(argv[argIndex], "--help")) { showHelp = true; }
			else if(!strcmp(argv[argIndex], "--enable-quoted-names"))
			{
				enableQuotedNames = true;
			}
			else if(!inputFilename)
			{
				inputFilename = argv[argIndex];
			}
			else if(!outputFilename)
			{
				outputFilename = argv[argIndex];
			}
			else
			{
				showHelp = true;
				break;
			}
		}
	}

	if(showHelp)
	{
		showDisassembleHelp(Log::error);
		return EXIT_FAILURE;
	}

	// Load the WASM file.
	IR::Module module(IR::FeatureSpec(true));
	module.featureSpec.quotedNamesInTextFormat = enableQuotedNames;
	if(!loadBinaryModuleFromFile(inputFilename, module)) { return EXIT_FAILURE; }

	// Print the module to WAST.
	Timing::Timer printTimer;
	const std::string wastString = WAST::print(module);
	Timing::logRatePerSecond(
		"Printed WAST", printTimer, F64(wastString.size()) / 1024.0 / 1024.0, "MiB");

	if(!outputFilename) { Log::printf(Log::output, "%s", wastString.c_str()); }
	else
	{
		// Write the serialized data to the output file.
		if(!saveFile(outputFilename, wastString.data(), wastString.size())) { return EXIT_FAILURE; }
	}

	return EXIT_SUCCESS;
}
