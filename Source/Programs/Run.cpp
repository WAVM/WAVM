#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "Core/Platform.h"
#include "AST/AST.h"
#include "Runtime/Runtime.h"

#include "CLI.h"
#include "RuntimeCLI.h"

int main(int argc,char** argv)
{
	AST::Module* module = nullptr;
	const char* functionName;
	if(argc == 4 && !strcmp(argv[1],"-text"))
	{
		WebAssemblyText::File wastFile;
		if(loadTextModule(argv[2],wastFile)) { module = wastFile.modules[0]; }
		else { return -1; }
		functionName = argv[3];
	}
	else if(argc == 5 && !strcmp(argv[1],"-binary"))
	{
		module = loadBinaryModule(argv[2],argv[3]);
		functionName = argv[4];
	}
	else
	{
		std::cerr <<  "Usage: Run -binary in.wasm in.js.mem functionname" << std::endl;
		std::cerr <<  "       Run -text in.wast functionname" << std::endl;
		return -1;
	}
	
	if(!module) { return -1; }
	
	// Initialize the runtime.
	if(!Runtime::init())
	{
		std::cerr << "Couldn't initialize runtime" << std::endl;
		return false;
	}

	if(!Runtime::loadModule(module)) { return -1; }
	
	// Initialize the Emscripten intrinsics.
	Void result;
	callModuleFunction(module,"__GLOBAL__sub_I_iostream_cpp",result);

	uint32 returnCode;
	Core::Timer executionTime;
	if(!callModuleFunction(module,functionName,returnCode)) { return -1; }
	executionTime.stop();

	std::cout << "Program returned: " << returnCode << std::endl;
	std::cout << "Execution time: " << executionTime.getMilliseconds() << "ms" << std::endl;

	return 0;
}
