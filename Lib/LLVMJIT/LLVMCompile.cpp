#include <string.h>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include "LLVMJITPrivate.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Defines.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Triple.h>
#include <llvm/ADT/ilist_iterator.h>
#include <llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Pass.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Scalar.h>
#if LLVM_VERSION_MAJOR >= 7
#include <llvm/Transforms/Utils.h>
#endif
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

namespace llvm {
	class MCContext;

#if LLVM_VERSION_MAJOR >= 7
	// Instead of including llvm/Transforms/InstCombine/InstCombine.h, which doesn't compile on
	// Windows, just declare the one function we call.
	FunctionPass* createInstructionCombiningPass(bool ExpensiveCombines = true);
#endif
}

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

static std::string printModule(const llvm::Module& llvmModule)
{
	std::string result;
	llvm::raw_string_ostream printStream(result);
	llvmModule.print(printStream, nullptr);
	return result;
}

// Define a LLVM raw output stream that can write directly to a std::vector.
struct LLVMArrayOutputStream : llvm::raw_pwrite_stream
{
	LLVMArrayOutputStream() { SetUnbuffered(); }

	~LLVMArrayOutputStream() override = default;

	void flush() = delete;

	std::vector<U8>&& getOutput() { return std::move(output); }

protected:
	virtual void write_impl(const char* data, size_t numBytes) override
	{
		const Uptr startOffset = output.size();
		output.resize(output.size() + numBytes);
		memcpy(output.data() + startOffset, data, numBytes);
	}
	virtual void pwrite_impl(const char* data, size_t numBytes, U64 offset) override
	{
		WAVM_ASSERT(offset + numBytes > offset && offset + numBytes <= U64(output.size()));
		memcpy(output.data() + offset, data, numBytes);
	}
	virtual U64 current_pos() const override { return output.size(); }

private:
	std::vector<U8> output;
};

static void optimizeLLVMModule(llvm::Module& llvmModule, bool shouldLogMetrics)
{
	// Run some optimization on the module's functions.
	Timing::Timer optimizationTimer;

	llvm::legacy::FunctionPassManager fpm(&llvmModule);
	fpm.add(llvm::createPromoteMemoryToRegisterPass());
	fpm.add(llvm::createInstructionCombiningPass());
	fpm.add(llvm::createCFGSimplificationPass());
	fpm.add(llvm::createJumpThreadingPass());
	fpm.add(llvm::createConstantPropagationPass());

	// This DCE pass is necessary to work around a bug in LLVM's CodeGenPrepare that's triggered
	// if there's a dead div/rem with limited-range divisor:
	// https://bugs.llvm.org/show_bug.cgi?id=43514
	fpm.add(llvm::createDeadCodeEliminationPass());

	fpm.doInitialization();
	for(auto functionIt = llvmModule.begin(); functionIt != llvmModule.end(); ++functionIt)
	{ fpm.run(*functionIt); }

	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond(
			"Optimized LLVM module", optimizationTimer, (F64)llvmModule.size(), "functions");
	}
}

std::vector<U8> LLVMJIT::compileLLVMModule(LLVMContext& llvmContext,
										   llvm::Module&& llvmModule,
										   bool shouldLogMetrics,
										   llvm::TargetMachine* targetMachine)
{
	// Verify the module.
	if(WAVM_ENABLE_ASSERTS)
	{
		std::string verifyOutputString;
		llvm::raw_string_ostream verifyOutputStream(verifyOutputString);
		if(llvm::verifyModule(llvmModule, &verifyOutputStream))
		{
			verifyOutputStream.flush();
			Errors::fatalf("LLVM verification errors:\n%s", verifyOutputString.c_str());
		}
	}

	// Optimize the module;
	optimizeLLVMModule(llvmModule, shouldLogMetrics);

	// Generate machine code for the module.
	Timing::Timer machineCodeTimer;
	std::vector<U8> objectBytes;
	{
		llvm::legacy::PassManager passManager;
		llvm::MCContext* mcContext;
		LLVMArrayOutputStream objectStream;
		WAVM_ERROR_UNLESS(!targetMachine->addPassesToEmitMC(passManager, mcContext, objectStream));
		passManager.run(llvmModule);
		objectBytes = objectStream.getOutput();
	}
	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond(
			"Generated machine code", machineCodeTimer, (F64)llvmModule.size(), "functions");
	}

	return objectBytes;
}

static std::unique_ptr<llvm::TargetMachine> getAndValidateTargetMachine(
	const IR::FeatureSpec& featureSpec,
	const TargetSpec& targetSpec)
{
	// Get the target machine.
	std::unique_ptr<llvm::TargetMachine> targetMachine = getTargetMachine(targetSpec);
	if(!targetMachine)
	{
		Errors::fatalf("Invalid target spec (triple=%s, cpu=%s).",
					   targetSpec.triple.c_str(),
					   targetSpec.cpu.c_str());
	}

	// Validate that the target machine supports the module's FeatureSpec.
	switch(validateTarget(targetSpec, featureSpec))
	{
	case TargetValidationResult::valid: break;

	case TargetValidationResult::invalidTargetSpec:
		Errors::fatalf("Invalid target spec (triple=%s, cpu=%s).\n",
					   targetSpec.triple.c_str(),
					   targetSpec.cpu.c_str());
	case TargetValidationResult::unsupportedArchitecture:
		Errors::fatalf("Unsupported target architecture (triple=%s, cpu=%s)",
					   targetSpec.triple.c_str(),
					   targetSpec.cpu.c_str());
	case TargetValidationResult::x86CPUDoesNotSupportSSE41:
		Errors::fatalf(
			"Target X86 CPU (% s) does not support SSE 4.1, which"
			" WAVM requires for WebAssembly SIMD code.\n",
			targetSpec.cpu.c_str());
	case TargetValidationResult::wavmDoesNotSupportSIMDOnArch:
		Errors::fatalf("WAVM does not support SIMD on the host CPU architecture.\n");

	default: WAVM_UNREACHABLE();
	};

	return targetMachine;
}

std::vector<U8> LLVMJIT::compileModule(const IR::Module& irModule, const TargetSpec& targetSpec)
{
	std::unique_ptr<llvm::TargetMachine> targetMachine
		= getAndValidateTargetMachine(irModule.featureSpec, targetSpec);

	// Emit LLVM IR for the module.
	LLVMContext llvmContext;
	llvm::Module llvmModule("", llvmContext);
	emitModule(irModule, llvmContext, llvmModule, targetMachine.get());

	// Compile the LLVM IR to object code.
	return compileLLVMModule(llvmContext, std::move(llvmModule), true, targetMachine.get());
}

std::string LLVMJIT::emitLLVMIR(const IR::Module& irModule,
								const TargetSpec& targetSpec,
								bool optimize)
{
	std::unique_ptr<llvm::TargetMachine> targetMachine
		= getAndValidateTargetMachine(irModule.featureSpec, targetSpec);

	// Emit LLVM IR for the module.
	LLVMContext llvmContext;
	llvm::Module llvmModule("", llvmContext);
	emitModule(irModule, llvmContext, llvmModule, targetMachine.get());

	// Optimize the LLVM IR.
	if(optimize) { optimizeLLVMModule(llvmModule, true); }

	// Print the LLVM IR.
	return printModule(llvmModule);
}
