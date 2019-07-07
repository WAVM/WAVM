#include <string>
#include <vector>
#include "FuzzTargetCommonMain.h"
#include "RandomModule.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
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
	static const LLVMJIT::TargetSpec possibleTargetSpecs[] = {
		LLVMJIT::TargetSpec{"x86_64-pc-windows-msvc", "skylake-avx512"},
		LLVMJIT::TargetSpec{"x86_64-apple-darwin18.6.0", "skylake-avx512"},
		LLVMJIT::TargetSpec{"x86_64-unknown-linux-gnu", "skylake-avx512"},
		LLVMJIT::TargetSpec{"x86_64-unknown-linux-gnu", "skylake"},
		LLVMJIT::TargetSpec{"x86_64-unknown-linux-gnu", "core2"},
	};
	static const Uptr numPossibleTargets
		= sizeof(possibleTargetSpecs) / sizeof(LLVMJIT::TargetSpec);

	const LLVMJIT::TargetSpec& targetSpec = possibleTargetSpecs[random.get(numPossibleTargets - 1)];

	std::vector<U8> objectCode;
	errorUnless(LLVMJIT::compileModule(module, targetSpec, objectCode)
				== LLVMJIT::CompileResult::success);
}

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	RandomStream random(data, numBytes);

	IR::Module module;
	generateValidModule(module, random);

#if !WAVM_ENABLE_FUZZER
	std::string wastString = WAST::print(module);
	Log::printf(Log::Category::debug, "Generated module WAST:\n%s\n", wastString.c_str());
#endif

	compileModule(module, random);

	return 0;
}
