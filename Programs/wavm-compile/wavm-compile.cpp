#include <string>
#include <vector>

#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Runtime/Runtime.h"
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

int main(int argc, char** argv)
{
	if(argc != 3)
	{
		Log::printf(Log::error, "Usage: wavm-compile (in.wast|in.wasm) out.wasm\n");
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];
	const char* outputFilename = argv[2];

	IR::Module irModule;

	// Load the module IR.
	if(!loadModule(inputFilename, irModule)) { return EXIT_FAILURE; }

	// Compile the module's IR.
	Runtime::ModuleRef module = Runtime::compileModule(irModule);

	// Extract the compiled object code and add it to the IR module as a user section.
	irModule.userSections.push_back({"wavm.precompiled_object", Runtime::getObjectCode(module)});

	// Serialize the WASM module.
	std::vector<U8> wasmBytes;
	try
	{
		Timing::Timer saveTimer;

		Serialization::ArrayOutputStream stream;
		WASM::serialize(stream, irModule);
		wasmBytes = stream.getBytes();

		Timing::logRatePerSecond(
			"Serialized WASM", saveTimer, wasmBytes.size() / 1024.0 / 1024.0, "MB");
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
