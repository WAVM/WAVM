#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>
#include "FuzzTargetCommonMain.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/WASM/WASM.h"

#if !WAVM_ENABLE_LIBFUZZER
#include "WAVM/WASTPrint/WASTPrint.h"
#endif

using namespace WAVM;
using namespace WAVM::Runtime;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	IR::Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals = 1024;
	module.featureSpec.maxDataSegments = 65536;
	if(!WASM::loadBinaryModule(data, numBytes, module, Log::debug)) { return 0; }

#if !WAVM_ENABLE_FUZZER
	std::string wastString = WAST::print(module);
	Log::printf(Log::Category::debug, "Disassembly:\n%s\n", wastString.c_str());
#endif

	GCPointer<Compartment> compartment = createCompartment();
	{
		NullResolver nullResolver;
		StubResolver stubResolver(
			compartment, nullResolver, StubResolver::FunctionBehavior::zero, false);
		LinkResult linkResult = linkModule(module, stubResolver);
		if(linkResult.success)
		{
			catchRuntimeExceptions(
				[&] {
					instantiateModule(compartment,
									  compileModule(module),
									  std::move(linkResult.resolvedImports),
									  "fuzz");
				},
				[&](Exception* exception) { destroyException(exception); });
		}
	}
	errorUnless(tryCollectCompartment(std::move(compartment)));

	return 0;
}
