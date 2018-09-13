#include <string>
#include <vector>

#include "IR/Module.h"
#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
#include "Inline/Errors.h"
#include "Inline/Serialization.h"
#include "Inline/Timing.h"
#include "Logging/Logging.h"
#include "Runtime/Runtime.h"

using namespace IR;
using namespace Runtime;

int main(int argc, char** argv)
{
	if(argc != 3)
	{
		Log::printf(Log::error, "Usage: wavm-compile (in.wast|in.wasm) out.wasm\n");
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];
	const char* outputFilename = argv[2];

	// Treat any unhandled exception (e.g. in a thread) as a fatal error.
	Runtime::setUnhandledExceptionHandler([](Runtime::Exception&& exception) {
		Errors::fatalf("Unhandled runtime exception: %s\n", describeException(exception).c_str());
	});

	IR::Module irModule;

	// Load the module IR.
	if(!loadModule(inputFilename, irModule)) { return EXIT_FAILURE; }

	// Compile the module's IR.
	Runtime::Module* module = Runtime::compileModule(irModule);

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
	catch(Serialization::FatalSerializationException exception)
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
