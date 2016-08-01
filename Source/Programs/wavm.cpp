#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "Core/Platform.h"
#include "AST/AST.h"
#include "Runtime/Runtime.h"

#include "CLI.h"

bool endsWith(const char *str, const char *suffix)
{
	if(!str || !suffix) { return false; }
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if(lenstr < lensuffix) { return false; }
	return (strncmp(str+lenstr-lensuffix, suffix, lensuffix) == 0);
}

void showHelp()
{
	std::cerr << "Usage: wavm [switches] [programfile] [--] [arguments]" << std::endl;
	std::cerr << "  --text in.wast\t\tSpecify text program file (.wast)" << std::endl;
	std::cerr << "  --binary in.wasm in.js.mem\tSpecify binary program file (.wasm) and memory file (.js.mem)" << std::endl;
	std::cerr << "  -f|--function name\t\tSpecify function name to run in module rather than main" << std::endl;
	std::cerr << "  --\t\t\t\tStop parsing arguments" << std::endl;
}

int main(int argc,char** argv)
{
	const char* sourceFile = 0;
	const char* binaryFile = 0;
	const char* binaryMemFile = 0;
	const char* functionName = 0;

	auto args = argv;
	while(*++args)
	{
		if(!sourceFile && !strcmp(*args, "--text"))
		{
			if(!*++args) { showHelp(); return EXIT_FAILURE; }
			sourceFile = *args;
		}
		else if(!sourceFile && endsWith(*args, ".wast"))
		{
			sourceFile = *args;
		}
		else if(!binaryFile && !strcmp(*args, "--binary"))
		{
			if(!*++args) { showHelp(); return EXIT_FAILURE; }
			binaryFile = *args;
			if(!*++args) { showHelp(); return EXIT_FAILURE; }
			binaryMemFile = *args;
		}
		else if(!binaryFile && endsWith(*args, ".wasm"))
		{
			binaryFile = *args;
			if(!*++args) { showHelp(); return EXIT_FAILURE; }
			binaryMemFile = *args;
		}
		else if(!strcmp(*args, "--function") || !strcmp(*args, "-f"))
		{
			if(!*++args) { showHelp(); return EXIT_FAILURE; }
			functionName = *args;
		}
		else if(!strcmp(*args, "--"))
		{
			++args;
			break;
		}
		else if(!strcmp(*args, "--help") || !strcmp(*args, "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else { break; }
	}

	AST::Module* module = nullptr;
	const char* main_arg0 = 0;
	if(sourceFile)
	{
		WebAssemblyText::File wastFile;
		if(loadTextModule(sourceFile,wastFile)) { module = wastFile.modules[0]; }
		else { return EXIT_FAILURE; }
		main_arg0 = sourceFile;
	}
	else if(binaryFile)
	{
		module = loadBinaryModule(binaryFile,binaryMemFile);
		main_arg0 = binaryFile;
	}
	else
	{
		showHelp();
		return EXIT_FAILURE;
	}

	if(!module) { return EXIT_FAILURE; }
	if(!Runtime::init()) { return EXIT_FAILURE; }
	if(!Runtime::loadModule(module)) { return EXIT_FAILURE; }

	auto functionExport = module->exportNameToFunctionIndexMap.end();
	if(!functionName)
	{
		functionExport = module->exportNameToFunctionIndexMap.find("main");
		if(functionExport == module->exportNameToFunctionIndexMap.end()) { functionExport = module->exportNameToFunctionIndexMap.find("_main"); }
		if(functionExport == module->exportNameToFunctionIndexMap.end())
		{
			std::cerr << "Module does not export main" << std::endl;
			return EXIT_FAILURE;
		}
	}
	else
	{
		functionExport = module->exportNameToFunctionIndexMap.find(functionName);
		if(functionExport == module->exportNameToFunctionIndexMap.end())
		{
			std::cerr << "Module does not export '" << functionName << "'" << std::endl;
			return EXIT_FAILURE;
		}
	}

	std::vector<Runtime::Value> parameters;
	auto function = module->functions[functionExport->second];
	if(!functionName)
	{
		if(function->type.parameters.size() == 2)
		{
			uintptr main_argc_start = args-argv-1;
			auto main_argc = (uintptr)argc - main_argc_start;
			auto main_argv = (uintptr)Runtime::vmGrowMemory((main_argc+1) * sizeof(uint32));
			auto main_argvGlobal = &Runtime::instanceMemoryRef<uint32>(main_argv);
			for (auto i = main_argc_start; i < (uint32)argc; ++i)
			{
				const char* str = argv[i];
				if(i == main_argc_start) { str = main_arg0; }
				auto len = strlen(str)+1;
				auto vmAddress = (uint32)Runtime::vmGrowMemory(sizeof(uint8)*(len));
				memcpy(&Runtime::instanceMemoryRef<short>(vmAddress),str,len);
				main_argvGlobal[i-main_argc_start] = vmAddress;
			}
			main_argvGlobal[main_argc] = 0;
			std::vector<Runtime::Value> mainParameters = {(uint32)main_argc, (uint32)main_argv};
			parameters = mainParameters;
		}
		else if(function->type.parameters.size() > 0)
		{
			std::cerr << "'" << function->name << "' requires " << function->type.parameters.size() << " argument(s), but only 0 or 2 can be passed!" << std::endl;
			return EXIT_FAILURE;
		}
	}
	else
	{
		parameters.resize(function->type.parameters.size());
		uintptr main_argc_start = args-argv;
		auto end = (uintptr)std::min((uintptr)function->type.parameters.size(), (uintptr)(argc - main_argc_start));
		for(uint32 i = 0; i < end; ++i)
		{
			Runtime::Value value;
			switch((Runtime::TypeId)function->type.parameters[i])
			{
			case Runtime::TypeId::Void:
			case Runtime::TypeId::None: break;
			case Runtime::TypeId::I8:
			case Runtime::TypeId::I16:
			case Runtime::TypeId::I32: value = (uint32)atoi(argv[main_argc_start+i]); break;
			case Runtime::TypeId::I64: value = (uint64)atol(argv[main_argc_start+i]); break;
			case Runtime::TypeId::F32:
			case Runtime::TypeId::F64: value = atof(argv[main_argc_start+i]); break;
			default: throw;
			}
			parameters[i] = value;
		}
	}

	#if WAVM_TIMER_OUTPUT
	Core::Timer executionTime;
	#endif
	auto functionResult = Runtime::invokeFunction(module,functionExport->second,parameters.data());
	#if WAVM_TIMER_OUTPUT
	executionTime.stop();
	std::cout << "Execution time: " << executionTime.getMilliseconds() << "ms" << std::endl;
	#endif

	if(functionResult.type == Runtime::TypeId::Exception)
	{
		std::cerr << function->name << " threw exception: " << Runtime::describeExceptionCause(functionResult.exception->cause) << std::endl;
		for(auto calledFunction : functionResult.exception->callStack) { std::cerr << "  " << calledFunction << std::endl; }
		return EXIT_FAILURE;
	}

	if(!functionName)
	{
		if(functionResult.type != Runtime::TypeId::I32) { return EXIT_SUCCESS; }
		return functionResult.i32;
	}

	std::cout << functionName << " returned: " << describeRuntimeValue(functionResult) << std::endl;
	return EXIT_SUCCESS;
}
