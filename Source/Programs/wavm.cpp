#include "CLI.h"
#include "Emscripten/Emscripten.h"
#include "Inline/BasicTypes.h"
#include "Inline/Timing.h"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Validate.h"
#include "Runtime/Runtime.h"
#include "Runtime/Linker.h"
#include "Runtime/Intrinsics.h"
#include "ThreadTest/ThreadTest.h"
#include "WAST/WAST.h"

#include <map>

using namespace IR;
using namespace Runtime;

struct RootResolver : Resolver
{
	Compartment* compartment;
	std::map<std::string,ModuleInstance*> moduleNameToInstanceMap;

	RootResolver(Compartment* inCompartment): compartment(inCompartment) {}

	bool resolve(const std::string& moduleName,const std::string& exportName,ObjectType type,Object*& outObject) override
	{
		auto namedInstanceIt = moduleNameToInstanceMap.find(moduleName);
		if(namedInstanceIt != moduleNameToInstanceMap.end())
		{
			outObject = getInstanceExport(namedInstanceIt->second,exportName);
			if(outObject)
			{
				if(isA(outObject,type)) { return true; }
				else
				{
					Log::printf(Log::Category::error,"Resolved import %s.%s to a %s, but was expecting %s",
						moduleName.c_str(),
						exportName.c_str(),
						asString(getObjectType(outObject)).c_str(),
						asString(type).c_str());
					return false;
				}
			}
		}

		Log::printf(Log::Category::error,"Generated stub for missing import %s.%s : %s\n",moduleName.c_str(),exportName.c_str(),asString(type).c_str());
		outObject = getStubObject(type);
		return true;
	}

	Object* getStubObject(ObjectType type) const
	{
		// If the import couldn't be resolved, stub it in.
		switch(type.kind)
		{
		case IR::ObjectKind::function:
		{
			// Generate a function body that just uses the unreachable op to fault if called.
			Serialization::ArrayOutputStream codeStream;
			OperatorEncoderStream encoder(codeStream);
			encoder.unreachable();
			encoder.end();

			// Generate a module for the stub function.
			Module stubModule;
			DisassemblyNames stubModuleNames;
			stubModule.types.push_back(asFunctionType(type));
			stubModule.functions.defs.push_back({{0},{},std::move(codeStream.getBytes()),{}});
			stubModule.exports.push_back({"importStub",IR::ObjectKind::function,0});
			stubModuleNames.functions.push_back({"importStub <" + asString(type) + ">",{},{}});
			IR::setDisassemblyNames(stubModule,stubModuleNames);
			IR::validateDefinitions(stubModule);

			// Instantiate the module and return the stub function instance.
			auto stubModuleInstance = instantiateModule(compartment,stubModule,{});
			return getInstanceExport(stubModuleInstance,"importStub");
		}
		case IR::ObjectKind::memory:
		{
			return asObject(Runtime::createMemory(compartment,asMemoryType(type)));
		}
		case IR::ObjectKind::table:
		{
			return asObject(Runtime::createTable(compartment,asTableType(type)));
		}
		case IR::ObjectKind::global:
		{
			return asObject(Runtime::createGlobal(
				compartment,
				asGlobalType(type),
				Runtime::Value(asGlobalType(type).valueType,Runtime::UntaggedValue())));
		}
		case IR::ObjectKind::exceptionType:
		{
			return asObject(Runtime::createExceptionTypeInstance(asExceptionTypeType(type)));
		}
		default: Errors::unreachable();
		};
	}
};

struct CommandLineOptions
{
	const char* filename = nullptr;
	const char* functionName = nullptr;
	char** args = nullptr;
	bool onlyCheck = false;
	bool enableEmscripten = true;
	bool enableThreadTest = false;
};

static int run(const CommandLineOptions& options)
{
	Module module;

	// Enable some additional "features" in WAVM that are disabled by default.
	module.featureSpec.importExportMutableGlobals = true;
	module.featureSpec.sharedTables = true;
	// Allow atomics on unshared memories to accomodate atomics on the Emscripten memory.
	module.featureSpec.requireSharedFlagForAtomicOperators = false;

	// Load the module.
	if(!loadModule(options.filename,module)) { return EXIT_FAILURE; }
	if(options.onlyCheck) { return EXIT_SUCCESS; }

	// Link the module with the intrinsic modules.
	Compartment* compartment = Runtime::createCompartment();
	Context* context = Runtime::createContext(compartment);
	RootResolver rootResolver(compartment);

	Emscripten::Instance* emscriptenInstance = nullptr;
	if(options.enableEmscripten)
	{
		emscriptenInstance = Emscripten::instantiate(compartment);
		rootResolver.moduleNameToInstanceMap["env"] = emscriptenInstance->env;
		rootResolver.moduleNameToInstanceMap["asm2wasm"] = emscriptenInstance->asm2wasm;
		rootResolver.moduleNameToInstanceMap["global"] = emscriptenInstance->global;
	}

	if(options.enableThreadTest)
	{
		ModuleInstance* threadTestInstance = ThreadTest::instantiate(compartment);
		rootResolver.moduleNameToInstanceMap["threadTest"] = threadTestInstance;
	}

	LinkResult linkResult = linkModule(module,rootResolver);
	if(!linkResult.success)
	{
		std::cerr << "Failed to link module:" << std::endl;
		for(auto& missingImport : linkResult.missingImports)
		{
			std::cerr << "Missing import: module=\"" << missingImport.moduleName
				<< "\" export=\"" << missingImport.exportName
				<< "\" type=\"" << asString(missingImport.type) << "\"" << std::endl;
		}
		return EXIT_FAILURE;
	}

	// Instantiate the module.
	ModuleInstance* moduleInstance = instantiateModule(compartment,module,std::move(linkResult.resolvedImports));
	if(!moduleInstance) { return EXIT_FAILURE; }

	// Call the module start function, if it has one.
	FunctionInstance* startFunction = getStartFunction(moduleInstance);
	if(startFunction)
	{
		invokeFunctionChecked(context,startFunction,{});
	}

	if(options.enableEmscripten)
	{
		// Call the Emscripten global initalizers.
		Emscripten::initializeGlobals(context,module,moduleInstance);
	}

	// Look up the function export to call.
	FunctionInstance* functionInstance;
	if(!options.functionName)
	{
		functionInstance = asFunctionNullable(getInstanceExport(moduleInstance,"main"));
		if(!functionInstance) { functionInstance = asFunctionNullable(getInstanceExport(moduleInstance,"_main")); }
		if(!functionInstance)
		{
			std::cerr << "Module does not export main function" << std::endl;
			return EXIT_FAILURE;
		}
	}
	else
	{
		functionInstance = asFunctionNullable(getInstanceExport(moduleInstance,options.functionName));
		if(!functionInstance)
		{
			std::cerr << "Module does not export '" << options.functionName << "'" << std::endl;
			return EXIT_FAILURE;
		}
	}
	const FunctionType* functionType = getFunctionType(functionInstance);

	// Set up the arguments for the invoke.
	std::vector<Value> invokeArgs;
	if(!options.functionName)
	{
		if(functionType->parameters.size() == 2)
		{
			MemoryInstance* defaultMemory = Runtime::getDefaultMemory(moduleInstance);
			if(!defaultMemory)
			{
				std::cerr << "Module does not declare a default memory object to put arguments in." << std::endl;
				return EXIT_FAILURE;
			}

			std::vector<const char*> argStrings;
			argStrings.push_back(options.filename);
			char** args = options.args;
			while(*args) { argStrings.push_back(*args++); };

			Emscripten::injectCommandArgs(emscriptenInstance,argStrings,invokeArgs);
		}
		else if(functionType->parameters.size() > 0)
		{
			std::cerr << "WebAssembly function requires " << functionType->parameters.size() << " argument(s), but only 0 or 2 can be passed!" << std::endl;
			return EXIT_FAILURE;
		}
	}
	else
	{
		for(U32 i = 0; options.args[i]; ++i)
		{
			Value value;
			switch(functionType->parameters[i])
			{
			case ValueType::i32: value = (U32)atoi(options.args[i]); break;
			case ValueType::i64: value = (U64)atol(options.args[i]); break;
			case ValueType::f32: value = (F32)atof(options.args[i]); break;
			case ValueType::f64: value = atof(options.args[i]); break;
			default: Errors::unreachable();
			}
			invokeArgs.push_back(value);
		}
	}

	// Invoke the function.
	Timing::Timer executionTimer;
	Result functionResult = invokeFunctionChecked(context,functionInstance,invokeArgs);
	Timing::logTimer("Invoked function",executionTimer);

	if(options.functionName)
	{
		Log::printf(
			Log::Category::debug,
			"%s returned: %s\n",
			options.functionName,
			asString(functionResult).c_str());
		return EXIT_SUCCESS;
	}
	else if(functionResult.type == ResultType::i32) { return functionResult.i32; }
	else { return EXIT_SUCCESS; }
}

void showHelp()
{
	std::cerr << "Usage: wavm [switches] [programfile] [--] [arguments]" << std::endl;
	std::cerr << "  in.wast|in.wasm\t\tSpecify program file (.wast/.wasm)" << std::endl;
	std::cerr << "  -f|--function name\t\tSpecify function name to run in module rather than main" << std::endl;
	std::cerr << "  -c|--check\t\t\tExit after checking that the program is valid" << std::endl;
	std::cerr << "  -d|--debug\t\t\tWrite additional debug information to stdout" << std::endl;
	std::cerr << "  --disable-emscripten\t\tDisable Emscripten intrinsics" << std::endl;
	std::cerr << "  --enable-thread-test\t\tEnable ThreadTest intrinsics" << std::endl;
	std::cerr << "  --\t\t\t\tStop parsing arguments" << std::endl;
}

int main(int argc,char** argv)
{
	CommandLineOptions options;
	options.args = argv;
	while(*++options.args)
	{
		if(!strcmp(*options.args, "--function") || !strcmp(*options.args, "-f"))
		{
			if(!*++options.args) { showHelp(); return EXIT_FAILURE; }
			options.functionName = *options.args;
		}
		else if(!strcmp(*options.args, "--check") || !strcmp(*options.args, "-c"))
		{
			options.onlyCheck = true;
		}
		else if(!strcmp(*options.args, "--debug") || !strcmp(*options.args, "-d"))
		{
			Log::setCategoryEnabled(Log::Category::debug,true);
		}
		else if(!strcmp(*options.args, "--disable-emscripten"))
		{
			options.enableEmscripten = false;
		}
		else if(!strcmp(*options.args, "--enable-thread-test"))
		{
			options.enableThreadTest = true;
		}
		else if(!strcmp(*options.args, "--"))
		{
			++options.args;
			break;
		}
		else if(!strcmp(*options.args, "--help") || !strcmp(*options.args, "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else if(!options.filename)
		{
			options.filename = *options.args;
		}
		else { break; }
	}

	if(!options.filename)
	{
		showHelp();
		return EXIT_FAILURE;
	}

	// Treat any unhandled exception (e.g. in a thread) as a fatal error.
	Runtime::setUnhandledExceptionHandler([](Runtime::Exception&& exception)
	{
		Errors::fatalf("Unhandled runtime exception: %s\n",describeException(exception).c_str());
	});

	int returnCode = EXIT_FAILURE;
	#ifdef __AFL_LOOP
	while(__AFL_LOOP(2000))
	#endif
	{
		returnCode = run(options);
		Runtime::collectGarbage();
	}
	return returnCode;
}
