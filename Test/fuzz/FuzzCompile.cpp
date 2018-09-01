#include "IR/Module.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Linker.h"
#include "Runtime/Runtime.h"
#include "WASM/WASM.h"
#include "WAST/TestScript.h"
#include "WAST/WAST.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

using namespace IR;
using namespace Runtime;
using namespace WAST;

namespace LLVMJIT
{
	RUNTIME_API void deinit();
}

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	IR::Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals = 1024;
	if(!loadBinaryModule(data, numBytes, module, Log::debug)) { return 0; }

	compileModule(module);
	collectGarbage();

	// De-initialize LLVM to avoid the accumulation of de-duped debug metadata in the LLVMContext.
	LLVMJIT::deinit();

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
