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
		else { return EXIT_FAILURE; }
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
		return EXIT_FAILURE;
	}
	
	if(!module) { return EXIT_FAILURE; }
	
	// Initialize the runtime.
	if(!Runtime::init())
	{
		std::cerr << "Couldn't initialize runtime" << std::endl;
		return EXIT_FAILURE;
	}

	if(!Runtime::loadModule(module)) { return EXIT_FAILURE; }

	#if WAVM_TIMER_OUTPUT
	Core::Timer executionTime;
	#endif
	auto functionExport = module->exportNameToFunctionIndexMap.find(functionName);
	if(functionExport == module->exportNameToFunctionIndexMap.end())
	{
		std::cerr << "Module does not export '" << functionName << "'" << std::endl;
		return EXIT_FAILURE;
	}
	auto function = module->functions[functionExport->second];
	if(function->type.parameters.size())
	{
		std::cerr << "Module exports '" << functionName << "' but it isn't the right type?!" << std::endl;
	}
	else
	{
		auto functionResult = Runtime::invokeFunction(module,functionExport->second,nullptr);
		switch(functionResult.type) {
		case Runtime::TypeId::Exception:
			std::cerr << functionName << " threw exception: " << Runtime::describeExceptionCause(functionResult.exception->cause) << std::endl;
			for(auto calledFunction : functionResult.exception->callStack) { std::cerr << "  " << calledFunction << std::endl; }
			break;
		case Runtime::TypeId::I8:
			std::cout << "Function result: " << functionResult.i8 << std::endl;
			break;
		case Runtime::TypeId::I16:
			std::cout << "Function result: " << functionResult.i16 << std::endl;
			break;
		case Runtime::TypeId::I32:
			std::cout << "Function result: " << functionResult.i32 << std::endl;
			break;
		case Runtime::TypeId::I64:
			std::cout << "Function result: " << functionResult.i64 << std::endl;
			break;
		case Runtime::TypeId::F32:
			std::cout << "Function result: " << functionResult.f32 << std::endl;
			break;
		case Runtime::TypeId::F64:
			std::cout << "Function result: " << functionResult.f64 << std::endl;
			break;
		default:
			break;
		}
	}
	#if WAVM_TIMER_OUTPUT
	executionTime.stop();
	std::cout << "Execution time: " << executionTime.getMilliseconds() << "ms" << std::endl;
	#endif

	return EXIT_SUCCESS;
}
