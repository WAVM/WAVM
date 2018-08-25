#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
#include "Platform/Platform.h"
#include "WASM/WASM.h"
#include "WAST/WAST.h"

int main(int argc, char** argv)
{
	if(argc < 3)
	{
		Log::printf(Log::error,
					"Usage: Assemble in.wast out.wasm [switches]\n"
					"  -n|--omit-names           Omits WAST names from the output\n"
					"     --omit-extended-names  Omits only the non-standard WAVM extended\n"
					"                              names from the output\n");
		return EXIT_FAILURE;
	}
	const char* inputFilename  = argv[1];
	const char* outputFilename = argv[2];
	bool omitNames             = false;
	bool omitExtendedNames     = false;
	if(argc > 3)
	{
		for(Iptr argumentIndex = 3; argumentIndex < argc; ++argumentIndex)
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
	IR::Module module;
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
	std::vector<U8> wasmBytes;
	try
	{
		Timing::Timer saveTimer;

		Serialization::ArrayOutputStream stream;
		WASM::serialize(stream, module);
		wasmBytes = stream.getBytes();

		Timing::logRatePerSecond(
			"Serialized WASM", saveTimer, wasmBytes.size() / 1024.0 / 1024.0, "MB");
	}
	catch(Serialization::FatalSerializationException exception)
	{
		Log::printf(Log::error,
					"Error serializing WebAssembly binary file:\n%s\n",
					exception.message.c_str());
		return false;
	}

	// Write the serialized data to the output file.
	return saveFile(outputFilename, wasmBytes.data(), wasmBytes.size()) ? EXIT_SUCCESS
																		: EXIT_FAILURE;
}
