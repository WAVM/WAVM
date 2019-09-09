#include <stdlib.h>
#include <vector>
#include "FuzzTargetCommonMain.h"
#include "ModuleMatcher.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	// Copy the data to a std::vector and append a null terminator.
	std::vector<U8> wastBytes(data, data + numBytes);
	wastBytes.push_back(0);

	Module wastModule(FeatureSpec(true));
	wastModule.featureSpec.sharedTables = true;
	std::vector<WAST::Error> parseErrors;
	if(WAST::parseModule((const char*)wastBytes.data(), wastBytes.size(), wastModule, parseErrors))
	{
		std::vector<U8> wasmBytes;
		{
			Serialization::ArrayOutputStream outputStream;
			WASM::saveBinaryModule(outputStream, wastModule);
			wasmBytes = outputStream.getBytes();
		}

		Module wasmModule(FeatureSpec(true));
		{
			Serialization::MemoryInputStream inputStream(wasmBytes.data(), wasmBytes.size());
			WASM::LoadError loadError;
			if(!WASM::loadBinaryModule(inputStream, wasmModule, &loadError))
			{
				Errors::fatalf("Failed to load the generated WASM binary: %s",
							   loadError.message.c_str());
			}
		}

		ModuleMatcher moduleMatcher(wastModule, wasmModule);
		moduleMatcher.verify();
	}

	return 0;
}
