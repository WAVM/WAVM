#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

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
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/WASI/WASI.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

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
	std::vector<std::string> args;
	bool onlyCheck = false;
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

	I32 exitCode = 0;
	WASI::RunResult result
		= WASI::run(module, std::vector<std::string>(options.args), {}, exitCode);
	switch(result)
	{
	case WASI::RunResult::success: return exitCode;
	case WASI::RunResult::linkError:
		Log::printf(Log::error, "WASM module is unlinkable.\n");
		return EXIT_FAILURE;
	case WASI::RunResult::noStartFunction:
		Log::printf(Log::error, "WASM module doesn't export WASI _start function.\n");
		return EXIT_FAILURE;
	case WASI::RunResult::mistypedStartFunction:
		Log::printf(Log::error,
					"WASM module exports a _start function, but it is not the correct type.\n.");
		return EXIT_FAILURE;
	default: Errors::unreachable();
	}
}

static void showHelp()
{
	Log::printf(Log::error,
				"Usage: wavm-run-wasi [switches] [programfile] [--] [arguments]\n"
				"  in.wast|in.wasm       Specify program file (.wast/.wasm)\n"
				"  -c|--check            Exit after checking that the program is valid\n"
				"  -d|--debug            Write additional debug information to stdout\n"
				"  -h|--help             Display this message\n"
				"  --precompiled         Use precompiled object code in programfile\n"
				"  --metrics             Write benchmarking information to stdout\n"
				"  --trace-syscalls      Trace WASI syscalls to stdout\n"
				"  --                    Stop parsing arguments\n");
}

int main(int argc, char** argv)
{
	CommandLineOptions options;
	char** nextArg = argv;
	while(*++nextArg)
	{
		if(!strcmp(*nextArg, "--check") || !strcmp(*nextArg, "-c")) { options.onlyCheck = true; }
		else if(!strcmp(*nextArg, "--debug") || !strcmp(*nextArg, "-d"))
		{
			Log::setCategoryEnabled(Log::debug, true);
		}
		else if(!strcmp(*nextArg, "--metrics"))
		{
			Log::setCategoryEnabled(Log::metrics, true);
		}
		else if(!strcmp(*nextArg, "--precompiled"))
		{
			options.precompiled = true;
		}
		else if (!strcmp(*nextArg, "--trace-syscalls"))
		{
			WASI::setTraceSyscalls(true);
		}
		else if(!strcmp(*nextArg, "--"))
		{
			++nextArg;
			break;
		}
		else if(!strcmp(*nextArg, "--help") || !strcmp(*nextArg, "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else if(!options.filename)
		{
			options.filename = *nextArg;
		}
		else
		{
			break;
		}
	}

	while(*nextArg) { options.args.push_back(*nextArg++); };

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
	return result;
}
