#include <stdlib.h>

#include "IR/IR.h"
#include "IR/Module.h"
#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
#include "Logging/Logging.h"

using namespace IR;
using namespace WAST;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals = 1024;
	loadBinaryModule(data, numBytes, module, Log::debug);
	return 0;
}

#if !ENABLE_LIBFUZZER
I32 main()
{
	LLVMFuzzerTestOneInput((const U8*)"", 0);
	return EXIT_SUCCESS;
}
#endif
