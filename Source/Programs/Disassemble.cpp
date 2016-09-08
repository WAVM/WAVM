#include "Core/Core.h"
#include "CLI.h"
#include "WAST/WAST.h"
#include "WebAssembly/WebAssembly.h"

int commandMain(int argc,char** argv)
{
	if(argc != 3)
	{
		std::cerr <<  "Usage: Disassemble in.wasm out.wast" << std::endl;
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];
	//const char* outputFilename = argv[2];	
	
	// Load the WASM file.
	WebAssembly::Module module;
	if(!loadBinaryModule(inputFilename,module)) { return EXIT_FAILURE; }

	std::cerr << "Disassembly not yet implemented!" << std::endl;
	return EXIT_FAILURE;
}
