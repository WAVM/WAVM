#include <string>
#include <vector>

#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
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

static void showHelp()
{
	LLVMJIT::TargetSpec hostTargetSpec = LLVMJIT::getHostTargetSpec();

	Log::printf(Log::error,
				"Usage: wavm-compile [options] <in.wast|wasm> <out.wasm>\n"
				"  -h|--help                 Display this message\n"
				"  --target-triple <triple>  Set the target triple (default: %s)\n"
				"  --target-cpu <cpu>        Set the target CPU (default: %s)\n"
				"  --enable <feature>        Enable the specified feature. See the list of\n"
				"                            supported features below.\n"
				"\n"
				"Features:\n"
				"%s\n",
				hostTargetSpec.triple.c_str(),
				hostTargetSpec.cpu.c_str(),
				getFeatureListHelpText());
}

int main(int argc, char** argv)
{
	if(argc < 3)
	{
		showHelp();
		return EXIT_FAILURE;
	}
	const char* inputFilename = nullptr;
	const char* outputFilename = nullptr;
	LLVMJIT::TargetSpec targetSpec = LLVMJIT::getHostTargetSpec();
	IR::FeatureSpec featureSpec;
	for(int argIndex = 1; argIndex < argc; ++argIndex)
	{
		if(!strcmp(argv[argIndex], "-h") || !strcmp(argv[argIndex], "--help"))
		{
			showHelp();
			return EXIT_FAILURE;
		}
		else if(!strcmp(argv[argIndex], "--target-triple"))
		{
			if(argIndex + 1 == argc)
			{
				showHelp();
				return EXIT_FAILURE;
			}
			++argIndex;
			targetSpec.triple = argv[argIndex];
		}
		else if(!strcmp(argv[argIndex], "--target-cpu"))
		{
			if(argIndex + 1 == argc)
			{
				showHelp();
				return EXIT_FAILURE;
			}
			++argIndex;
			targetSpec.cpu = argv[argIndex];
		}
		else if(!strcmp(argv[argIndex], "--enable"))
		{
			++argIndex;
			if(argIndex == argc)
			{
				Log::printf(Log::error, "Expected feature name following '--enable'.\n");
				return false;
			}

			if(!parseAndSetFeature(argv[argIndex], featureSpec, true))
			{
				Log::printf(Log::error, "Unknown feature '%s'.\n", argv[argIndex]);
				return false;
			}
		}
		else if(!inputFilename)
		{
			inputFilename = argv[argIndex];
		}
		else if(!outputFilename)
		{
			outputFilename = argv[argIndex];
		}
		else
		{
			showHelp();
			return EXIT_FAILURE;
		}
	}

	if(!inputFilename || !outputFilename)
	{
		showHelp();
		return EXIT_FAILURE;
	}

	// Load the module IR.
	IR::Module irModule(featureSpec);
	if (!loadModule(inputFilename, irModule)) { return EXIT_FAILURE; }

	// Compile the module's IR.
	std::vector<U8> objectCode;
	switch(LLVMJIT::compileModule(irModule, targetSpec, objectCode))
	{
	case LLVMJIT::CompileResult::success: break;
	case LLVMJIT::CompileResult::invalidTargetSpec:
		Log::printf(Log::error,
					"Target triple (%s) or CPU (%s) is invalid.\n",
					targetSpec.triple.c_str(),
					targetSpec.cpu.c_str());
		return EXIT_FAILURE;

	default: WAVM_UNREACHABLE();
	};

	// Extract the compiled object code and add it to the IR module as a user section.
	irModule.userSections.push_back({"wavm.precompiled_object", objectCode});

	// Serialize the WASM module.
	std::vector<U8> wasmBytes;
	try
	{
		Timing::Timer saveTimer;

		Serialization::ArrayOutputStream stream;
		WASM::serialize(stream, irModule);
		wasmBytes = stream.getBytes();

		Timing::logRatePerSecond(
			"Serialized WASM", saveTimer, wasmBytes.size() / 1024.0 / 1024.0, "MiB");
	}
	catch(Serialization::FatalSerializationException const& exception)
	{
		Log::printf(Log::error,
					"Error serializing WebAssembly binary file:\n%s\n",
					exception.message.c_str());
		return EXIT_FAILURE;
	}

	// Write the serialized data to the output file.
	return saveFile(outputFilename, wasmBytes.data(), wasmBytes.size()) ? EXIT_SUCCESS
																		: EXIT_FAILURE;
}
