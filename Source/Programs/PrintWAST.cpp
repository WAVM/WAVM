#include "Core/Core.h"
#include "CLI.h"
#include "AST/AST.h"
#include "WebAssembly/WebAssembly.h"

int main(int argc,char** argv)
{
	AST::Module* module;
	const char* outputFilename;
	if(argc == 4 && !strcmp(argv[1],"-text"))
	{
		WebAssemblyText::File file;
		if(loadTextModule(argv[2],file)) { module = file.modules[0]; }
		else { return EXIT_FAILURE; }
		outputFilename = argv[3];
	}
	else if(argc == 5 && !strcmp(argv[1],"-binary"))
	{
		module = loadBinaryModule(argv[2],argv[3]);
		outputFilename = argv[4];
	}
	else
	{
		std::cerr <<  "Usage: Print -binary in.wasm in.js.mem out.wast" << std::endl;
		std::cerr <<  "       Print -text in.wast out.wast" << std::endl;
		return EXIT_FAILURE;
	}
	
	if(!module) { return EXIT_FAILURE; }
	
	Core::Timer printTimer;
	std::ofstream outputStream(outputFilename);
	if(!outputStream.is_open())
	{
		std::cerr << "Failed to open " << outputFilename << std::endl;
		return EXIT_FAILURE;
	}
	outputStream << WebAssemblyText::print(module);
	outputStream.close();
	std::cout << "Printed WAST code in " << printTimer.getMilliseconds() << "ms" << std::endl;

	return EXIT_SUCCESS;
}
