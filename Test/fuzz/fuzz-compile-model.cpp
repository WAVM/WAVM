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

static LLVMJIT::TargetSpec makeTargetSpec(LLVMJIT::TargetArch arch,
										  LLVMJIT::TargetOS os,
										  const char* cpu)
{
	LLVMJIT::TargetSpec spec;
	spec.arch = arch;
	spec.os = os;
	spec.cpu = cpu;
	return spec;
}

static void compileModule(const IR::Module& module, RandomStream& random)
{
	using Arch = LLVMJIT::TargetArch;
	using OS = LLVMJIT::TargetOS;

	const LLVMJIT::TargetSpec possibleTargetSpecs[] = {
		makeTargetSpec(Arch::x86_64, OS::windows, "skylake-avx512"),
		makeTargetSpec(Arch::x86_64, OS::macos, "skylake-avx512"),
		makeTargetSpec(Arch::x86_64, OS::linux_, "skylake-avx512"),
		makeTargetSpec(Arch::x86_64, OS::linux_, "skylake"),
		makeTargetSpec(Arch::x86_64, OS::linux_, "penryn"),
		makeTargetSpec(Arch::aarch64, OS::linux_, "cortex-a72"),
	};
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
