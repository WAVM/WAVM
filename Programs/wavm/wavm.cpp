#include "Emscripten/Emscripten.h"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Validate.h"
#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
#include "Inline/HashMap.h"
#include "Inline/Timing.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Linker.h"
#include "Runtime/Runtime.h"
#include "ThreadTest/ThreadTest.h"
#include "WASTParse/WASTParse.h"

using namespace IR;
using namespace Runtime;

struct RootResolver : Resolver
{
	Compartment* compartment;
	HashMap<std::string, ModuleInstance*> moduleNameToInstanceMap;

	RootResolver(Compartment* inCompartment) : compartment(inCompartment) {}

	bool resolve(const std::string& moduleName,
				 const std::string& exportName,
				 ObjectType type,
				 Object*& outObject) override
	{
		auto namedInstance = moduleNameToInstanceMap.get(moduleName);
		if(namedInstance)
		{
			outObject = getInstanceExport(*namedInstance, exportName);
			if(outObject)
			{
				if(isA(outObject, type)) { return true; }
				else
				{
					Log::printf(Log::error,
								"Resolved import %s.%s to a %s, but was expecting %s\n",
								moduleName.c_str(),
								exportName.c_str(),
								asString(getObjectType(outObject)).c_str(),
								asString(type).c_str());
					return false;
				}
			}
		}

		Log::printf(Log::error,
					"Generated stub for missing import %s.%s : %s\n",
					moduleName.c_str(),
					exportName.c_str(),
					asString(type).c_str());
		outObject = getStubObject(exportName, type);
		return true;
	}

	Object* getStubObject(const std::string& exportName, ObjectType type) const
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
			IR::Module stubModule;
			DisassemblyNames stubModuleNames;
			stubModule.types.push_back(asFunctionType(type));
			stubModule.functions.defs.push_back({{0}, {}, std::move(codeStream.getBytes()), {}});
			stubModule.exports.push_back({"importStub", IR::ObjectKind::function, 0});
			stubModuleNames.functions.push_back({"importStub: " + exportName, {}, {}});
			IR::setDisassemblyNames(stubModule, stubModuleNames);
			IR::validateDefinitions(stubModule);

			// Instantiate the module and return the stub function instance.
			auto stubModuleInstance
				= instantiateModule(compartment, compileModule(stubModule), {}, "importStub");
			return getInstanceExport(stubModuleInstance, "importStub");
		}
		case IR::ObjectKind::memory:
		{
			return asObject(Runtime::createMemory(compartment, asMemoryType(type)));
		}
		case IR::ObjectKind::table:
		{
			return asObject(Runtime::createTable(compartment, asTableType(type)));
		}
		case IR::ObjectKind::global:
		{
			return asObject(Runtime::createGlobal(
				compartment,
				asGlobalType(type),
				IR::Value(asGlobalType(type).valueType, IR::UntaggedValue())));
		}
		case IR::ObjectKind::exceptionType:
		{
			return asObject(
				Runtime::createExceptionTypeInstance(asExceptionType(type), "importStub"));
		}
		default: Errors::unreachable();
		};
	}
};

static bool loadModule(const char* filename, IR::Module& outModule)
{
	// Read the specified file into an array.
	std::vector<U8> fileBytes;
	if(!loadFile(filename, fileBytes)) { return false; }

	// If the file starts with the WASM binary magic number, load it as a binary module.
	if(*(U32*)fileBytes.data() == 0x6d736100)
	{ return loadBinaryModule(fileBytes.data(), fileBytes.size(), outModule); }
	else
	{
		// Make sure the WAST file is null terminated.
		fileBytes.push_back(0);

		// Load it as a text module.
		std::vector<WAST::Error> parseErrors;
		if(!WAST::parseModule(
			   (const char*)fileBytes.data(), fileBytes.size(), outModule, parseErrors))
		{
			reportParseErrors(filename, parseErrors);
			return false;
		}

		return true;
	}
}

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
	IR::Module module;

	// Load the module.
	if(!loadModule(options.filename, module)) { return EXIT_FAILURE; }
	if(options.onlyCheck) { return EXIT_SUCCESS; }

	// Link the module with the intrinsic modules.
	Compartment* compartment = Runtime::createCompartment();
	Context* context = Runtime::createContext(compartment);
	RootResolver rootResolver(compartment);

	Emscripten::Instance* emscriptenInstance = nullptr;
	if(options.enableEmscripten)
	{
		emscriptenInstance = Emscripten::instantiate(compartment, module);
		if(emscriptenInstance)
		{
			rootResolver.moduleNameToInstanceMap.set("env", emscriptenInstance->env);
			rootResolver.moduleNameToInstanceMap.set("asm2wasm", emscriptenInstance->asm2wasm);
			rootResolver.moduleNameToInstanceMap.set("global", emscriptenInstance->global);
		}
	}

	if(options.enableThreadTest)
	{
		ModuleInstance* threadTestInstance = ThreadTest::instantiate(compartment);
		rootResolver.moduleNameToInstanceMap.set("threadTest", threadTestInstance);
	}

	LinkResult linkResult = linkModule(module, rootResolver);
	if(!linkResult.success)
	{
		Log::printf(Log::error, "Failed to link module:\n");
		for(auto& missingImport : linkResult.missingImports)
		{
			Log::printf(Log::error,
						"Missing import: module=\"%s\" export=\"%s\" type=\"%s\"\n",
						missingImport.moduleName.c_str(),
						missingImport.exportName.c_str(),
						asString(missingImport.type).c_str());
		}
		return EXIT_FAILURE;
	}

	// Instantiate the module.
	ModuleInstance* moduleInstance = instantiateModule(compartment,
													   compileModule(module),
													   std::move(linkResult.resolvedImports),
													   options.filename);
	if(!moduleInstance) { return EXIT_FAILURE; }

	// Call the module start function, if it has one.
	FunctionInstance* startFunction = getStartFunction(moduleInstance);
	if(startFunction) { invokeFunctionChecked(context, startFunction, {}); }

	if(options.enableEmscripten)
	{
		// Call the Emscripten global initalizers.
		Emscripten::initializeGlobals(context, module, moduleInstance);
	}

	// Look up the function export to call.
	FunctionInstance* functionInstance;
	if(!options.functionName)
	{
		functionInstance = asFunctionNullable(getInstanceExport(moduleInstance, "main"));
		if(!functionInstance)
		{ functionInstance = asFunctionNullable(getInstanceExport(moduleInstance, "_main")); }
		if(!functionInstance)
		{
			Log::printf(Log::error, "Module does not export main function\n");
			return EXIT_FAILURE;
		}
	}
	else
	{
		functionInstance
			= asFunctionNullable(getInstanceExport(moduleInstance, options.functionName));
		if(!functionInstance)
		{
			Log::printf(Log::error, "Module does not export '%s'\n", options.functionName);
			return EXIT_FAILURE;
		}
	}
	FunctionType functionType = getFunctionType(functionInstance);

	// Set up the arguments for the invoke.
	std::vector<Value> invokeArgs;
	if(!options.functionName)
	{
		if(functionType.params().size() == 2)
		{
			MemoryInstance* defaultMemory = Runtime::getDefaultMemory(moduleInstance);
			if(!defaultMemory)
			{
				Log::printf(
					Log::error,
					"Module does not declare a default memory object to put arguments in.\n");
				return EXIT_FAILURE;
			}

			std::vector<const char*> argStrings;
			argStrings.push_back(options.filename);
			char** args = options.args;
			while(*args) { argStrings.push_back(*args++); };

			Emscripten::injectCommandArgs(emscriptenInstance, argStrings, invokeArgs);
		}
		else if(functionType.params().size() > 0)
		{
			Log::printf(Log::error,
						"WebAssembly function requires %" PRIu64
						" argument(s), but only 0 or 2 can be passed!",
						functionType.params().size());
			return EXIT_FAILURE;
		}
	}
	else
	{
		for(U32 i = 0; options.args[i]; ++i)
		{
			Value value;
			switch(functionType.params()[i])
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
	IR::ValueTuple functionResults = invokeFunctionChecked(context, functionInstance, invokeArgs);
	Timing::logTimer("Invoked function", executionTimer);

	if(options.functionName)
	{
		Log::printf(Log::debug,
					"%s returned: %s\n",
					options.functionName,
					asString(functionResults).c_str());
		return EXIT_SUCCESS;
	}
	else if(functionResults.size() == 1 && functionResults[0].type == ValueType::i32)
	{
		return functionResults[0].i32;
	}
	else
	{
		return EXIT_SUCCESS;
	}
}

void showHelp()
{
	Log::printf(Log::error,
				"Usage: wavm [switches] [programfile] [--] [arguments]\n"
				"  in.wast|in.wasm\t\tSpecify program file (.wast/.wasm)\n"
				"  -f|--function name\t\tSpecify function name to run in module rather than main\n"
				"  -c|--check\t\t\tExit after checking that the program is valid\n"
				"  -d|--debug\t\t\tWrite additional debug information to stdout\n"
				"  --disable-emscripten\t\tDisable Emscripten intrinsics\n"
				"  --enable-thread-test\t\tEnable ThreadTest intrinsics\n"
				"  --\t\t\t\tStop parsing arguments\n");
}

int main(int argc, char** argv)
{
	CommandLineOptions options;
	options.args = argv;
	while(*++options.args)
	{
		if(!strcmp(*options.args, "--function") || !strcmp(*options.args, "-f"))
		{
			if(!*++options.args)
			{
				showHelp();
				return EXIT_FAILURE;
			}
			options.functionName = *options.args;
		}
		else if(!strcmp(*options.args, "--check") || !strcmp(*options.args, "-c"))
		{
			options.onlyCheck = true;
		}
		else if(!strcmp(*options.args, "--debug") || !strcmp(*options.args, "-d"))
		{
			Log::setCategoryEnabled(Log::debug, true);
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
		else
		{
			break;
		}
	}

	if(!options.filename)
	{
		showHelp();
		return EXIT_FAILURE;
	}

	// Treat any unhandled exception (e.g. in a thread) as a fatal error.
	Runtime::setUnhandledExceptionHandler([](Runtime::Exception&& exception) {
		Errors::fatalf("Unhandled runtime exception: %s\n", describeException(exception).c_str());
	});

	return run(options);
}
