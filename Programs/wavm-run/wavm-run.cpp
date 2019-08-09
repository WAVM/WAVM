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
#include "WAVM/VFS/SandboxFS.h"
#include "WAVM/WASI/WASI.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

struct RootResolver : Resolver
{
	StubResolver stubResolver;
	HashMap<std::string, ModuleInstance*> moduleNameToInstanceMap;

	RootResolver(Compartment* compartment) : stubResolver(compartment) {}

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

		return stubResolver.resolve(moduleName, exportName, type, outObject);
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

static bool compileModule(const IR::Module& irModule, ModuleRef& outModule, bool precompiled)
{
	if(!precompiled)
	{
		outModule = Runtime::compileModule(irModule);
		return true;
	}
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
			return false;
		}
		else
		{
			outModule = Runtime::loadPrecompiledModule(irModule, precompiledObjectSection->data);
			return true;
		}
	}
}

static void reportLinkErrors(const LinkResult& linkResult)
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
}

static bool isWASIModule(const IR::Module& irModule)
{
	for(const auto& import : irModule.functions.imports)
	{
		if(import.moduleName == "wasi_unstable") { return true; }
	}

	return false;
}

static bool isEmscriptenModule(const IR::Module& irModule)
{
	if(!irModule.memories.imports.size() || irModule.memories.imports[0].moduleName != "env"
	   || irModule.memories.imports[0].exportName != "memory")
	{ return false; }

	for(const auto& import : irModule.functions.imports)
	{
		if(import.moduleName == "env") { return true; }
	}

	return false;
}

static void showHelp()
{
	Log::printf(Log::error,
				"Usage: wavm-run [options] <program file> [program arguments]\n"
				"  <program file>        The WebAssembly module (.wast/.wasm) to run\n"
				"  [program arguments]   The arguments to pass to the WebAssembly function\n"
				"\n"
				"  [options]:\n"
				"  -h|--help             Display this message\n"
				"  -d|--debug            Write additional debug information to stdout\n"
				"  -f|--function name    Specify function name to run in module (default:main)\n"
				"  --enable-thread-test  Enable ThreadTest intrinsics\n"
				"  --precompiled         Use precompiled object code in programfile\n"
				"  --metrics             Write benchmarking information to stdout\n"
				"  --sys=<system>        Specifies the system to host the module:\n"
				"                        bare, emscripten, or wasi. The default is to detect\n"
				"                        the system based on the module imports/exports.\n"
				"  --mount-root=<dir>    Mounts <dir> as the WASI root directory\n"
				"  --wasi-trace=<level>  Sets the level of WASI tracing:\n"
				"                        syscalls or syscalls-with-callstacks\n");
}

template<Uptr numPrefixChars>
static bool stringStartsWith(const char* string, const char (&prefix)[numPrefixChars])
{
	return !strncmp(string, prefix, numPrefixChars - 1);
}

enum class System
{
	detect,
	bare,
	emscripten,
	wasi
};

struct State
{
	// Command-line options.
	const char* filename = nullptr;
	const char* functionName = nullptr;
	const char* rootMountPath = nullptr;
	std::vector<std::string> runArgs;
	System system = System::detect;
	bool enableThreadTest = false;
	bool precompiled = false;
	WASI::SyscallTraceLevel wasiTraceLavel = WASI::SyscallTraceLevel::none;

	// Objects that need to be cleaned up before exiting.
	GCPointer<Compartment> compartment = createCompartment();
	Emscripten::Instance* emscriptenInstance = nullptr;
	std::shared_ptr<WASI::Process> wasiProcess;
	std::shared_ptr<VFS::FileSystem> sandboxFS;

	~State()
	{
		if(emscriptenInstance) { delete emscriptenInstance; }
		wasiProcess.reset();

		errorUnless(tryCollectCompartment(std::move(compartment)));
	}

	bool parseCommandLine(char** argv)
	{
		char** nextArg = argv;
		while(*++nextArg)
		{
			if(!strcmp(*nextArg, "--help") || !strcmp(*nextArg, "-h"))
			{
				showHelp();
				return false;
			}
			else if(!strcmp(*nextArg, "--debug") || !strcmp(*nextArg, "-d"))
			{
				Log::setCategoryEnabled(Log::debug, true);
			}
			else if(!strcmp(*nextArg, "--function") || !strcmp(*nextArg, "-f"))
			{
				if(!*++nextArg)
				{
					showHelp();
					return false;
				}
				functionName = *nextArg;
			}
			else if(!strcmp(*nextArg, "--metrics"))
			{
				Log::setCategoryEnabled(Log::metrics, true);
			}
			else if(stringStartsWith(*nextArg, "--sys="))
			{
				if(system != System::detect)
				{
					Log::printf(Log::error, "'--sys=' may only occur once on the command line.\n");
					return false;
				}

				const char* systemString = *nextArg + strlen("--sys=");
				if(!strcmp(systemString, "bare")) { system = System::bare; }
				else if(!strcmp(systemString, "emscripten"))
				{
					system = System::emscripten;
				}
				else if(!strcmp(systemString, "wasi"))
				{
					system = System::wasi;
				}
				else
				{
					Log::printf(Log::error,
								"Invalid system '%s': must be 'bare', 'emscripten', or 'wasi'.\n",
								systemString);
					return false;
				}
			}
			else if(!strcmp(*nextArg, "--enable-thread-test"))
			{
				enableThreadTest = true;
			}
			else if(!strcmp(*nextArg, "--precompiled"))
			{
				precompiled = true;
			}
			else if(stringStartsWith(*nextArg, "--mount-root="))
			{
				if(rootMountPath)
				{
					Log::printf(Log::error,
								"--mount-root=' may only occur once on the command line.\n");
					return false;
				}
				rootMountPath = *nextArg + strlen("--mount-root=");
			}
			else if(stringStartsWith(*nextArg, "--wasi-trace="))
			{
				if(wasiTraceLavel != WASI::SyscallTraceLevel::none)
				{
					Log::printf(Log::error,
								"--wasi-trace=' may only occur once on the command line.\n");
					return false;
				}

				const char* levelString = *nextArg + strlen("--mount-root=");

				if(!strcmp(levelString, "syscalls"))
				{ wasiTraceLavel = WASI::SyscallTraceLevel::syscalls; }
				else if(!strcmp(levelString, "syscalls-with-callstacks"))
				{
					wasiTraceLavel = WASI::SyscallTraceLevel::syscallsWithCallstacks;
				}
				else
				{
					Log::printf(Log::error, "Invalid WASI trace level: %s\n", levelString);
					return false;
				}
			}
			else
			{
				filename = *nextArg;
				++nextArg;
				break;
			}
		}

		if(!filename)
		{
			showHelp();
			return false;
		}

		while(*nextArg) { runArgs.push_back(*nextArg++); };

		return true;
	}

	bool initSystem(const IR::Module& irModule)
	{
		// If the user didn't specify a system on the command-line, try to figure it out from the
		// module's imports.
		if(system == System::detect)
		{
			if(isWASIModule(irModule))
			{
				Log::printf(Log::debug, "Module appears to be a WASI module.\n");
				system = System::wasi;
			}
			else if(isEmscriptenModule(irModule))
			{
				Log::printf(Log::debug, "Module appears to be an Emscripten module.\n");
				system = System::emscripten;
			}
		}

		// If a directory to mount as the root filesystem was passed on the command-line, create a
		// SandboxFS for it.
		if(rootMountPath)
		{
			if(system != System::wasi)
			{
				Log::printf(Log::error, "--mount-root may only be used with the WASI system.\n");
				return false;
			}

			std::string absoluteRootMountPath;
			if(rootMountPath[0] == '/' || rootMountPath[0] == '\\' || rootMountPath[0] == '~'
			   || rootMountPath[1] == ':')
			{ absoluteRootMountPath = rootMountPath; }
			else
			{
				absoluteRootMountPath
					= Platform::getCurrentWorkingDirectory() + '/' + rootMountPath;
			}
			sandboxFS = VFS::makeSandboxFS(&Platform::getHostFS(), absoluteRootMountPath);
		}

		if(system == System::emscripten)
		{
			// Instantiate the Emscripten environment.
			emscriptenInstance = Emscripten::instantiate(compartment, irModule);
			if(emscriptenInstance)
			{
				emscriptenInstance->stdIn = Platform::getStdFD(Platform::StdDevice::in);
				emscriptenInstance->stdOut = Platform::getStdFD(Platform::StdDevice::out);
				emscriptenInstance->stdErr = Platform::getStdFD(Platform::StdDevice::err);
			}
		}
		else if(system == System::wasi)
		{
			std::vector<std::string> args = runArgs;
			args.insert(args.begin(), "/proc/1/exe");

			// Create the WASI process.
			wasiProcess = WASI::createProcess(compartment,
											  std::move(args),
											  {},
											  sandboxFS.get(),
											  Platform::getStdFD(Platform::StdDevice::in),
											  Platform::getStdFD(Platform::StdDevice::out),
											  Platform::getStdFD(Platform::StdDevice::err));
		}

		if(wasiTraceLavel != WASI::SyscallTraceLevel::none)
		{
			if(system != System::wasi)
			{
				Log::printf(Log::error, "--wasi-trace may only be used with the WASI system.\n");
				return false;
			}

			WASI::setSyscallTraceLevel(wasiTraceLavel);
		}

		return true;
	}

	int run(char** argv)
	{
		// Parse the command line.
		if(!parseCommandLine(argv)) { return EXIT_FAILURE; }

		// Load the module.
		IR::Module irModule;
		if(!loadModule(filename, irModule)) { return EXIT_FAILURE; }

		// Compile the module.
		Runtime::ModuleRef module = nullptr;
		if(!compileModule(irModule, module, precompiled)) { return EXIT_FAILURE; }

		// Initialize the system environment.
		if(!initSystem(irModule)) { return EXIT_FAILURE; }

		// Link the module with the intrinsic modules.
		LinkResult linkResult;
		if(system == System::emscripten || system == System::bare)
		{
			RootResolver rootResolver(compartment);

			if(emscriptenInstance)
			{
				rootResolver.moduleNameToInstanceMap.set("env", emscriptenInstance->env);
				rootResolver.moduleNameToInstanceMap.set("asm2wasm", emscriptenInstance->asm2wasm);
				rootResolver.moduleNameToInstanceMap.set("global", emscriptenInstance->global);
			}

			if(enableThreadTest)
			{
				ModuleInstance* threadTestInstance = ThreadTest::instantiate(compartment);
				rootResolver.moduleNameToInstanceMap.set("threadTest", threadTestInstance);
			}

			linkResult = linkModule(irModule, rootResolver);
		}
		else if(system == System::wasi)
		{
			Resolver* resolver = WASI::getProcessResolver(wasiProcess);
			linkResult = linkModule(irModule, *resolver);
		}

		if(!linkResult.success)
		{
			reportLinkErrors(linkResult);
			return EXIT_FAILURE;
		}

		// Instantiate the module.
		ModuleInstance* moduleInstance = instantiateModule(
			compartment, module, std::move(linkResult.resolvedImports), filename);
		if(!moduleInstance) { return EXIT_FAILURE; }

		// Take the module's memory as the WASI process memory.
		if(system == System::wasi)
		{
			Memory* memory = asMemoryNullable(getInstanceExport(moduleInstance, "memory"));
			if(!memory)
			{
				Log::printf(Log::error, "WASM module doesn't export WASI memory.\n");
				return EXIT_FAILURE;
			}
			WASI::setProcessMemory(wasiProcess, memory);
		}

		// Create a WASM execution context.
		Context* context = Runtime::createContext(compartment);

		// Look up the function export to call, validate its type, and set up the invoke arguments.
		Function* function = nullptr;
		std::vector<Value> invokeArgs;
		if(functionName)
		{
			function = asFunctionNullable(getInstanceExport(moduleInstance, functionName));
			if(!function)
			{
				Log::printf(Log::error, "Module does not export '%s'\n", functionName);
				return EXIT_FAILURE;
			}

			const FunctionType functionType = getFunctionType(function);

			if(functionType.params().size() != runArgs.size())
			{
				Log::printf(Log::error,
							"'%s' expects %" PRIuPTR " argument(s), but command line had %" PRIuPTR
							".\n",
							functionName,
							functionType.params().size(),
							runArgs.size());
				return EXIT_FAILURE;
			}

			for(Uptr argIndex = 0; argIndex < runArgs.size(); ++argIndex)
			{
				const std::string& argString = runArgs[argIndex];
				Value value;
				switch(functionType.params()[argIndex])
				{
				case ValueType::i32: value = (U32)atoi(argString.c_str()); break;
				case ValueType::i64: value = (U64)atol(argString.c_str()); break;
				case ValueType::f32: value = (F32)atof(argString.c_str()); break;
				case ValueType::f64: value = atof(argString.c_str()); break;
				case ValueType::v128:
				case ValueType::anyref:
				case ValueType::funcref:
					Errors::fatalf("Cannot parse command-line argument for %s function parameter",
								   asString(functionType.params()[argIndex]));

				case ValueType::none:
				case ValueType::any:
				case ValueType::nullref:
				default: WAVM_UNREACHABLE();
				}
				invokeArgs.push_back(value);
			}
		}
		else if(system == System::wasi)
		{
			// WASI just calls a _start function with the signature ()->().
			function = asFunctionNullable(getInstanceExport(moduleInstance, "_start"));
			if(!function)
			{
				Log::printf(Log::error, "WASM module doesn't export WASI _start function.\n");
				return EXIT_FAILURE;
			}
			if(getFunctionType(function) != FunctionType())
			{
				Log::printf(Log::error,
							"WASI module exported _start : %s but expected _start : %s.\n",
							asString(getFunctionType(function)).c_str(),
							asString(FunctionType()).c_str());
				return EXIT_FAILURE;
			}
		}
		else
		{
			// Emscripten calls main or _main with a signature (i32, i32)|() -> i32?
			function = asFunctionNullable(getInstanceExport(moduleInstance, "main"));
			if(!function)
			{ function = asFunctionNullable(getInstanceExport(moduleInstance, "_main")); }
			if(!function)
			{
				Log::printf(Log::error, "Module does not export main function\n");
				return EXIT_FAILURE;
			}
			const FunctionType functionType = getFunctionType(function);

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
					std::vector<std::string> args = runArgs;
					args.insert(args.begin(), filename);

					wavmAssert(emscriptenInstance);
					Emscripten::injectCommandArgs(emscriptenInstance, args, invokeArgs);
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

		int result = EXIT_SUCCESS;
		try
		{
			// Call the module start function, if it has one.
			Function* startFunction = getStartFunction(moduleInstance);
			if(startFunction) { invokeFunctionChecked(context, startFunction, {}); }

			if(emscriptenInstance)
			{
				// Call the Emscripten global initalizers.
				Emscripten::initializeGlobals(
					emscriptenInstance, context, irModule, moduleInstance);
			}

			// Invoke the function.
			Timing::Timer executionTimer;
			IR::ValueTuple functionResults = invokeFunctionChecked(context, function, invokeArgs);
			Timing::logTimer("Invoked function", executionTimer);

			if(functionName)
			{
				Log::printf(Log::debug,
							"%s returned: %s\n",
							functionName,
							asString(functionResults).c_str());
			}
			else if(functionResults.size() == 1 && functionResults[0].type == ValueType::i32)
			{
				result = functionResults[0].i32;
			}
		}
		catch(const WASI::ExitException& exitException)
		{
			// If either the WASM or WASI start functions call the WASI exit API, they will throw a
			// WASI::ExitException. Catch it here, and return the exit code.
			result = int(exitException.exitCode);
		}

		// Log the peak memory usage.
		Uptr peakMemoryUsage = Platform::getPeakMemoryUsageBytes();
		Log::printf(Log::metrics, "Peak memory usage: %" PRIuPTR "KiB\n", peakMemoryUsage / 1024);

		return result;
	}

	int runAndCatchRuntimeExceptions(char** argv)
	{
		int result = EXIT_FAILURE;
		Runtime::catchRuntimeExceptions([&result, argv, this]() { result = run(argv); },
										[](Runtime::Exception* exception) {
											// Treat any unhandled exception as a fatal error.
											Errors::fatalf("Runtime exception: %s",
														   describeException(exception).c_str());
										});
		return result;
	}
};

int main(int argc, char** argv)
{
	State state;
	return state.runAndCatchRuntimeExceptions(argv);
}
