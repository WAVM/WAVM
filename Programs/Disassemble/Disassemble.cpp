#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
#include "Inline/Timing.h"
#include "Platform/Platform.h"
#include "WASM/WASM.h"
#include "WAST/WAST.h"

int main(int argc, char** argv)
{
	if(argc != 3)
	{
		Log::printf(Log::error, "Usage: Disassemble in.wasm out.wast\n");
		return EXIT_FAILURE;
	}
	const char* inputFilename  = argv[1];
	const char* outputFilename = argv[2];

	// Load the WASM file.
	IR::Module module;
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
