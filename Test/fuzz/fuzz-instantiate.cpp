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

#if !WAVM_ENABLE_LIBFUZZER
#include "WAVM/WASTPrint/WASTPrint.h"
#endif

using namespace WAVM;
using namespace WAVM::Runtime;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	IR::FeatureSpec featureSpec(IR::FeatureLevel::wavm);
	featureSpec.maxLabelsPerFunction = 65536;
	featureSpec.maxLocals = 1024;
	featureSpec.maxDataSegments = 65536;
	Runtime::ModuleRef module;
	if(!Runtime::loadBinaryModule(data, numBytes, module, featureSpec)) { return 0; }
	const IR::Module& moduleIR = getModuleIR(module);

#if !WAVM_ENABLE_LIBFUZZER
	std::string wastString = WAST::print(moduleIR);
	Log::printf(Log::Category::debug, "Disassembly:\n%s\n", wastString.c_str());
#endif

	// Use a resource quota to limit the amount of memory a single instance can allocate.
	ResourceQuotaRef resourceQuota = createResourceQuota();
	setResourceQuotaMaxTableElems(resourceQuota, 65536);
	setResourceQuotaMaxMemoryPages(resourceQuota, 8192);

	GCPointer<Compartment> compartment = createCompartment();
	{
		StubResolver stubResolver(compartment, StubFunctionBehavior::zero, false, resourceQuota);
		LinkResult linkResult = linkModule(moduleIR, stubResolver);
		if(linkResult.success)
		{
			catchRuntimeExceptions(
				[&] {
					instantiateModule(compartment,
									  module,
									  std::move(linkResult.resolvedImports),
									  "fuzz",
									  resourceQuota);
				},
				[&](Exception* exception) { destroyException(exception); });
		}
	}
	WAVM_ERROR_UNLESS(tryCollectCompartment(std::move(compartment)));

	return 0;
}
