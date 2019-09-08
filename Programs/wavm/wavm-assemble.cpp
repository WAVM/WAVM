#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"
#include "wavm.h"

using namespace WAVM;

static bool loadTextModuleFromFile(const char* filename, IR::Module& outModule)
{
	std::vector<U8> wastBytes;
	if(!loadFile(filename, wastBytes)) { return false; }

	// Make sure the WAST is null terminated.
	wastBytes.push_back(0);

	std::vector<WAST::Error> parseErrors;
	if(WAST::parseModule((const char*)wastBytes.data(), wastBytes.size(), outModule, parseErrors))
	{ return true; }
	else
	{
		Log::printf(Log::error, "Error parsing WebAssembly text file:\n");
		reportParseErrors(filename, (const char*)wastBytes.data(), parseErrors);
		return false;
	}
}

void showAssembleHelp(Log::Category outputCategory)
{
	Log::printf(outputCategory,
				"Usage: wavm assemble in.wast out.wasm [switches]\n"
				"  -n|--omit-names           Omits WAST names from the output\n"
				"     --omit-extended-names  Omits only the non-standard WAVM extended\n"
				"                              names from the output\n");
}

int execAssembleCommand(int argc, char** argv)
{
	if(argc < 2)
	{
		showAssembleHelp(Log::error);
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[0];
	const char* outputFilename = argv[1];
	bool omitNames = false;
	bool omitExtendedNames = false;
	if(argc > 2)
	{
		for(Iptr argumentIndex = 2; argumentIndex < argc; ++argumentIndex)
		{
			if(!strcmp(argv[argumentIndex], "-n") || !strcmp(argv[argumentIndex], "--omit-names"))
			{ omitNames = true; }
			else if(!strcmp(argv[argumentIndex], "--omit-extended-names"))
			{
				omitExtendedNames = true;
			}
			else
			{
				Log::printf(Log::error, "Unrecognized argument: %s\n", argv[argumentIndex]);
				return EXIT_FAILURE;
			}
		}
	}

	// Load the WAST module.
	IR::Module module(IR::FeatureSpec(true));
	module.featureSpec.extendedNamesSection = !omitExtendedNames;
	if(!loadTextModuleFromFile(inputFilename, module)) { return EXIT_FAILURE; }

	// If the command-line switch to omit names was specified, strip the name section.
	if(omitNames)
	{
		for(auto sectionIt = module.userSections.begin(); sectionIt != module.userSections.end();
			++sectionIt)
		{
			if(sectionIt->name == "name")
			{
				module.userSections.erase(sectionIt);
				break;
			}
		}
	}

	// Serialize the WASM module.

	Timing::Timer saveTimer;

	Serialization::ArrayOutputStream stream;
	WASM::saveBinaryModule(stream, module);
	std::vector<U8> wasmBytes = stream.getBytes();

	Timing::logRatePerSecond(
		"Serialized WASM", saveTimer, wasmBytes.size() / 1024.0 / 1024.0, "MiB");

	// Write the serialized data to the output file.
	return saveFile(outputFilename, wasmBytes.data(), wasmBytes.size()) ? EXIT_SUCCESS
																		: EXIT_FAILURE;
}
