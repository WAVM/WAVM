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
	Module module(FeatureSpec(true));
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals = 1024;
	module.featureSpec.maxDataSegments = 65536;
	module.featureSpec.setWAVMFeatures(true);
	if(WASM::loadBinaryModule(data, numBytes, module))
	{
		const std::string wastString = WAST::print(module);

#if !WAVM_ENABLE_LIBFUZZER
		// Log::printf(Log::debug, "%s\n", wastString.c_str());
#endif

		Module wastModule(FeatureSpec(true));
		wastModule.featureSpec.setWAVMFeatures(true);
		std::vector<WAST::Error> parseErrors;
		if(!WAST::parseModule(
			   (const char*)wastString.c_str(), wastString.size() + 1, wastModule, parseErrors))
		{
			WAST::reportParseErrors("disassembly", wastString.c_str(), parseErrors);
			Errors::fatal("Disassembled module contained syntax errors");
		}

		// Strip any name section from both WAST and WASM module, since disassembling and
		// re-assembling the module doesn't produce exactly the same name section.
		for(auto sectionIt = module.customSections.begin();
			sectionIt != module.customSections.end();)
		{
			if(sectionIt->name != "name") { ++sectionIt; }
			else
			{
				sectionIt = module.customSections.erase(sectionIt);
			}
		}
		for(auto sectionIt = wastModule.customSections.begin();
			sectionIt != wastModule.customSections.end();)
		{
			if(sectionIt->name != "name") { ++sectionIt; }
			else
			{
				sectionIt = wastModule.customSections.erase(sectionIt);
			}
		}

		ModuleMatcher moduleMatcher(module, wastModule);
		moduleMatcher.verify();
	}

	return 0;
}
