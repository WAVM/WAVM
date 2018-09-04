#include <stdlib.h>
#include <vector>

#include "IR/IR.h"
#include "IR/Module.h"
#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
#include "Logging/Logging.h"
#include "Runtime/Runtime.h"

using namespace IR;
using namespace Runtime;
using namespace WAST;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	IR::Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals = 1024;
	if(!loadBinaryModule(data, numBytes, module, Log::debug)) { return 0; }

	compileModule(module);
	collectGarbage();

	return 0;
}

#if !ENABLE_LIBFUZZER
I32 main(int argc, char** argv)
{
	if(argc != 2)
	{
		Log::printf(Log::error, "Usage: FuzzCompile in.wasm\n");
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];

	std::vector<U8> wasmBytes;
	if(!loadFile(inputFilename, wasmBytes)) { return EXIT_FAILURE; }

	LLVMFuzzerTestOneInput(wasmBytes.data(), wasmBytes.size());
	return EXIT_SUCCESS;
}
#endif
