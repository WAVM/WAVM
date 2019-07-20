#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

#include "WAVM/Emscripten/Emscripten.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/File.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/ThreadTest/ThreadTest.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

struct RootResolver : Resolver
{
	HashMap<std::string, ModuleInstance*> moduleNameToInstanceMap;

	bool resolve(const std::string& moduleName,
				 const std::string& exportName,
				 ExternType type,
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
								asString(getExternType(outObject)).c_str(),
								asString(type).c_str());
				}
			}
		}

		return false;
	}
};

static bool loadModule(const char* filename, IR::Module& outModule)
{
	// Read the specified file into an array.
	std::vector<U8> fileBytes;
	if(!loadFile(filename, fileBytes)) { return false; }

	// If the file starts with the WASM binary magic number, load it as a binary irModule.
	static const U8 wasmMagicNumber[4] = {0x00, 0x61, 0x73, 0x6d};
	if(fileBytes.size() >= 4 && !memcmp(fileBytes.data(), wasmMagicNumber, 4))
	{ return WASM::loadBinaryModule(fileBytes.data(), fileBytes.size(), outModule); }
	else
	{
		// Make sure the WAST file is null terminated.
		fileBytes.push_back(0);

		// Load it as a text irModule.
		std::vector<WAST::Error> parseErrors;
		if(!WAST::parseModule(
			   (const char*)fileBytes.data(), fileBytes.size(), outModule, parseErrors))
		{
			Log::printf(Log::error, "Error parsing WebAssembly text file:\n");
			WAST::reportParseErrors(filename, parseErrors);
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
	bool precompiled = false;
};

static int run(const CommandLineOptions& options)
{
	IR::Module irModule;

	// Load the module.
	if(!loadModule(options.filename, irModule)) { return EXIT_FAILURE; }
	if(options.onlyCheck) { return EXIT_SUCCESS; }

	// Compile the module.
	Runtime::ModuleRef module = nullptr;
	if(!options.precompiled) { module = Runtime::compileModule(irModule); }
	else
	{
		const UserSection* precompiledObjectSection = nullptr;
		for(const UserSection& userSection : irModule.userSections)
		{
			if(userSection.name == "wavm.precompiled_object")
			{
				precompiledObjectSection = &userSection;
				break;
			}
		}

		if(!precompiledObjectSection)
		{
			Log::printf(Log::error,
						"Input file did not contain 'wavm.precompiled_object' section.\n");
			return EXIT_FAILURE;
		}
		else
		{
			module = Runtime::loadPrecompiledModule(irModule, precompiledObjectSection->data);
		}
	}

	// Link the module with the intrinsic modules.
	Compartment* compartment = Runtime::createCompartment();
	Context* context = Runtime::createContext(compartment);
	RootResolver rootResolver;
	StubResolver stubResolver(compartment, rootResolver);

	Emscripten::Instance* emscriptenInstance = nullptr;
	if(options.enableEmscripten)
	{
		emscriptenInstance = Emscripten::instantiate(compartment, irModule);
		if(emscriptenInstance)
		{
			rootResolver.moduleNameToInstanceMap.set("env", emscriptenInstance->env);
			rootResolver.moduleNameToInstanceMap.set("asm2wasm", emscriptenInstance->asm2wasm);
			rootResolver.moduleNameToInstanceMap.set("global", emscriptenInstance->global);

			emscriptenInstance->stdIn = Platform::getStdFD(Platform::StdDevice::in);
			emscriptenInstance->stdOut = Platform::getStdFD(Platform::StdDevice::out);
			emscriptenInstance->stdErr = Platform::getStdFD(Platform::StdDevice::err);
		}
	}

	if(options.enableThreadTest)
	{
		ModuleInstance* threadTestInstance = ThreadTest::instantiate(compartment);
		rootResolver.moduleNameToInstanceMap.set("threadTest", threadTestInstance);
	}

	LinkResult linkResult = linkModule(irModule, stubResolver);
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
	ModuleInstance* moduleInstance = instantiateModule(
		compartment, module, std::move(linkResult.resolvedImports), options.filename);
	if(!moduleInstance) { return EXIT_FAILURE; }

	// Call the module start function, if it has one.
	Function* startFunction = getStartFunction(moduleInstance);
	if(startFunction) { invokeFunctionChecked(context, startFunction, {}); }

	if(emscriptenInstance)
	{
		// Call the Emscripten global initalizers.
		Emscripten::initializeGlobals(emscriptenInstance, context, irModule, moduleInstance);
	}

	// Look up the function export to call.
	Function* function;
	if(!options.functionName)
	{
		function = asFunctionNullable(getInstanceExport(moduleInstance, "main"));
		if(!function) { function = asFunctionNullable(getInstanceExport(moduleInstance, "_main")); }
		if(!function)
		{
			Log::printf(Log::error, "Module does not export main function\n");
			return EXIT_FAILURE;
		}
	}
	else
	{
		function = asFunctionNullable(getInstanceExport(moduleInstance, options.functionName));
		if(!function)
		{
			Log::printf(Log::error, "Module does not export '%s'\n", options.functionName);
			return EXIT_FAILURE;
		}
	}
	FunctionType functionType = getFunctionType(function);

	// Set up the arguments for the invoke.
	std::vector<Value> invokeArgs;
	if(!options.functionName)
	{
		if(functionType.params().size() == 2)
		{
			if(!emscriptenInstance)
			{
				Log::printf(
					Log::error,
					"Module does not declare a default memory object to put arguments in.\n");
				return EXIT_FAILURE;
			}
			else
			{
				std::vector<const char*> argStrings;
				argStrings.push_back(options.filename);
				char** args = options.args;
				while(*args) { argStrings.push_back(*args++); };

				wavmAssert(emscriptenInstance);
				Emscripten::injectCommandArgs(emscriptenInstance, argStrings, invokeArgs);
			}
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
			case ValueType::v128:
			case ValueType::anyref:
			case ValueType::funcref:
				Errors::fatalf("Cannot parse command-line argument for %s function parameter",
							   asString(functionType.params()[i]));

			case ValueType::none:
			case ValueType::any:
			case ValueType::nullref:
			default: WAVM_UNREACHABLE();
			}
			invokeArgs.push_back(value);
		}
	}

	// Invoke the function.
	Timing::Timer executionTimer;
	IR::ValueTuple functionResults = invokeFunctionChecked(context, function, invokeArgs);
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

static void showHelp()
{
	Log::printf(Log::error,
				"Usage: wavm-run [options] <program file> [program arguments]\n"
				"  -h|--help             Display this message\n"
				"  -c|--check            Exit after checking that the program is valid\n"
				"  -d|--debug            Write additional debug information to stdout\n"
				"  -f|--function name    Specify function name to run in module (default:main)\n"
				"  --disable-emscripten  Disable Emscripten intrinsics\n"
				"  --enable-thread-test  Enable ThreadTest intrinsics\n"
				"  --precompiled         Use precompiled object code in programfile\n"
				"  --metrics             Write benchmarking information to stdout\n"
				"  <program file>        The WebAssembly module (.wast/.wasm) to run\n"
				"  [program arguments]   The arguments to pass to the WebAssembly function\n");
}

int main(int argc, char** argv)
{
	CommandLineOptions options;
	options.args = argv;
	while(*++options.args)
	{
		if(!strcmp(*options.args, "--help") || !strcmp(*options.args, "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else if(!strcmp(*options.args, "--check") || !strcmp(*options.args, "-c"))
		{
			options.onlyCheck = true;
		}
		else if(!strcmp(*options.args, "--debug") || !strcmp(*options.args, "-d"))
		{
			Log::setCategoryEnabled(Log::debug, true);
		}
		else if(!strcmp(*options.args, "--function") || !strcmp(*options.args, "-f"))
		{
			if(!*++options.args)
			{
				showHelp();
				return EXIT_FAILURE;
			}
			options.functionName = *options.args;
		}
		else if(!strcmp(*options.args, "--metrics"))
		{
			Log::setCategoryEnabled(Log::metrics, true);
		}
		else if(!strcmp(*options.args, "--disable-emscripten"))
		{
			options.enableEmscripten = false;
		}
		else if(!strcmp(*options.args, "--enable-thread-test"))
		{
			options.enableThreadTest = true;
		}
		else if(!strcmp(*options.args, "--precompiled"))
		{
			options.precompiled = true;
		}
		else
		{
			options.filename = *options.args;
			++options.args;
			break;
		}
	}

	if(!options.filename)
	{
		showHelp();
		return EXIT_FAILURE;
	}

	int result = EXIT_FAILURE;
	Runtime::catchRuntimeExceptions([&result, options]() { result = run(options); },
									[](Runtime::Exception* exception) {
										// Treat any unhandled exception as a fatal error.
										Errors::fatalf("Runtime exception: %s",
													   describeException(exception).c_str());
									});

	// Log the peak memory usage.
	Uptr peakMemoryUsage = Platform::getPeakMemoryUsageBytes();
	Log::printf(Log::metrics, "Peak memory usage: %" PRIuPTR "KiB\n", peakMemoryUsage / 1024);

	return result;
}
