#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "Core/Platform.h"
#include "AST/AST.h"
#include "Runtime/Runtime.h"

#include "CLI.h"

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

	Core::Timer executionTime;
	auto functionExport = module->exportNameToFunctionIndexMap.find(functionName);
	if(functionExport != module->exportNameToFunctionIndexMap.end())
	{
		auto function = module->functions[functionExport->second];
		if(function->type.parameters.size())
		{
			std::cerr << "Module exports '" << functionName << "' but it isn't the right type?!" << std::endl;
		}
		else
		{
			auto functionResult = Runtime::invokeFunction(module,functionExport->second,nullptr);
			if(functionResult.type == Runtime::TypeId::Exception)
			{
				std::cerr << functionName << " threw exception: " << Runtime::describeExceptionCause(functionResult.exception->cause) << std::endl;
				for(auto function : functionResult.exception->callStack) { std::cerr << "  " << function << std::endl; }
			}
		}
	}
	else
	{
		std::cerr << "Module does not export '" << functionName << "'" << std::endl;
		return -1;
	}
	executionTime.stop();

	std::cout << "Execution time: " << executionTime.getMilliseconds() << "ms" << std::endl;

	return 0;
}
