#include "Core/Core.h"
#include "CLI.h"
#include "WAST/WAST.h"
#include "WebAssembly/WebAssembly.h"

int commandMain(int argc,char** argv)
{
	if(argc != 3)
	{
		std::cerr <<  "Usage: Assemble in.wast out.wasm" << std::endl;
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];
	const char* outputFilename = argv[2];	
	
	// Load the WAST module.
	WebAssembly::Module module;
	if(!loadTextModule(inputFilename,module)) { return EXIT_FAILURE; }

	// Write the binary module.
	if(!saveBinaryModule(outputFilename,module)) { return EXIT_FAILURE; }

	return EXIT_SUCCESS;
}
