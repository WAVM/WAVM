#include <stdlib.h>
#include <string>
#include <vector>
#include "FuzzTargetCommonMain.h"
#include "ModuleMatcher.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"
#include "WAVM/WASTPrint/WASTPrint.h"

using namespace WAVM;
using namespace WAVM::IR;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals = 1024;
	module.featureSpec.maxDataSegments = 65536;
	if(WASM::loadBinaryModule(data, numBytes, module, Log::debug))
	{
		const std::string wastString = WAST::print(module);

#if !WAVM_ENABLE_LIBFUZZER
		Log::printf(Log::debug, "%s\n", wastString.c_str());
#endif

		Module wastModule;
		std::vector<WAST::Error> parseErrors;
		if(!WAST::parseModule(
			   (const char*)wastString.c_str(), wastString.size() + 1, wastModule, parseErrors))
		{
			WAST::reportParseErrors("disassembly", parseErrors);
			Errors::fatal("Disassembled module contained syntax errors");
		}

		ModuleMatcher moduleMatcher(module, wastModule);
		moduleMatcher.verify();
	}

	return 0;
}
