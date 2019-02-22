#include <stdlib.h>

#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASM/WASM.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::WASM;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals = 1024;
	module.featureSpec.maxDataSegments = 65536;
	loadBinaryModule(data, numBytes, module, Log::debug);
	return 0;
}

#if !WAVM_ENABLE_LIBFUZZER
I32 main()
{
	LLVMFuzzerTestOneInput((const U8*)"", 0);
	return EXIT_SUCCESS;
}
#endif
