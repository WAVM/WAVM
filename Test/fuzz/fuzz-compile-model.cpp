#include <string>
#include <vector>
#include "FuzzTargetCommonMain.h"
#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/RandomModule.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/RandomStream.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"

#if !WAVM_ENABLE_LIBFUZZER
#include "WAVM/WASTPrint/WASTPrint.h"
#endif

namespace WAVM { namespace IR {
	struct Module;
}}

using namespace WAVM;
using namespace WAVM::IR;

static void compileModule(const IR::Module& module, RandomStream& random)
{
	static const LLVMJIT::TargetSpec possibleTargetSpecs[]
		= {LLVMJIT::TargetSpec{"x86_64-pc-windows-msvc", "skylake-avx512"},
		   LLVMJIT::TargetSpec{"x86_64-apple-darwin18.6.0", "skylake-avx512"},
		   LLVMJIT::TargetSpec{"x86_64-unknown-linux-gnu", "skylake-avx512"},
		   LLVMJIT::TargetSpec{"x86_64-unknown-linux-gnu", "skylake"},
		   LLVMJIT::TargetSpec{"x86_64-unknown-linux-gnu", "penryn"},
		   LLVMJIT::TargetSpec{"aarch64-unknown-linux-gnu", "cortex-a72"}};
	static const Uptr numPossibleTargets
		= sizeof(possibleTargetSpecs) / sizeof(LLVMJIT::TargetSpec);

	const LLVMJIT::TargetSpec& targetSpec = possibleTargetSpecs[random.get(numPossibleTargets - 1)];

	WAVM_ERROR_UNLESS(LLVMJIT::validateTarget(targetSpec, FeatureLevel::proposed)
					  == LLVMJIT::TargetValidationResult::valid);

	std::vector<U8> objectCode = LLVMJIT::compileModule(module, targetSpec);
}

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	RandomStream random(data, numBytes);

	IR::Module module(FeatureLevel::wavm);
	IR::generateValidModule(module, random);

#if !WAVM_ENABLE_LIBFUZZER
	std::string wastString = WAST::print(module);
	Log::printf(Log::Category::debug, "Generated module WAST:\n%s\n", wastString.c_str());
#endif

	compileModule(module, random);

	return 0;
}
