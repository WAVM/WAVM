#include <string.h>
#include <memory>
#include <string>
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

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm/ADT/StringRef.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

namespace llvm {
	class MCContext;
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

static void optimizeLLVMModule(llvm::Module& llvmModule,
							   bool shouldLogMetrics,
							   llvm::TargetMachine* targetMachine)
{
	Timing::Timer optimizationTimer;

	// Log pre-optimization IR if trace-compilation logging is enabled.
	if(Log::isCategoryEnabled(Log::traceCompilation))
	{
		Log::printf(Log::traceCompilation,
					"=== Pre-optimization LLVM IR ===\n%s\n",
					printModule(llvmModule).c_str());
	}

	// Create the analysis managers. They must be declared in this order due to
	// inter-analysis-manager references during destruction.
	llvm::LoopAnalysisManager LAM;
	llvm::FunctionAnalysisManager FAM;
	llvm::CGSCCAnalysisManager CGAM;
	llvm::ModuleAnalysisManager MAM;

	// Create the PassBuilder with the target machine to enable target-specific optimizations.
	llvm::PassBuilder PB(targetMachine);

	// Register all analyses with the managers.
	PB.registerModuleAnalyses(MAM);
	PB.registerCGSCCAnalyses(CGAM);
	PB.registerFunctionAnalyses(FAM);
	PB.registerLoopAnalyses(LAM);
	PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

	// Build the optimization pipeline using the new pass manager.
	// Use LLVM's default O2 optimization pipeline.
	llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
	MPM.run(llvmModule, MAM);

	// Log post-optimization IR if trace-compilation logging is enabled.
	if(Log::isCategoryEnabled(Log::traceCompilation))
	{
		Log::printf(Log::traceCompilation,
					"=== Post-optimization LLVM IR ===\n%s\n",
					printModule(llvmModule).c_str());
	}

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
	optimizeLLVMModule(llvmModule, shouldLogMetrics, targetMachine);

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
		std::string tripleStr = getTriple(targetSpec);
		Errors::fatalf(
			"Invalid target spec (triple=%s, cpu=%s).", tripleStr.c_str(), targetSpec.cpu.c_str());
	}

	// Validate that the target machine supports the module's FeatureSpec.
	switch(validateTarget(targetSpec, featureSpec))
	{
	case TargetValidationResult::valid: break;

	case TargetValidationResult::invalidTargetSpec: {
		std::string tripleStr = getTriple(targetSpec);
		Errors::fatalf("Invalid target spec (triple=%s, cpu=%s).\n",
					   tripleStr.c_str(),
					   targetSpec.cpu.c_str());
	}
	case TargetValidationResult::unsupportedArchitecture: {
		std::string tripleStr = getTriple(targetSpec);
		Errors::fatalf("Unsupported target architecture (triple=%s, cpu=%s)",
					   tripleStr.c_str(),
					   targetSpec.cpu.c_str());
	}
	case TargetValidationResult::x86CPUDoesNotSupportSSE41:
		Errors::fatalf(
			"Target X86 CPU (%s) does not support SSE 4.1, which"
			" WAVM requires for WebAssembly SIMD code.\n",
			targetSpec.cpu.c_str());
	case TargetValidationResult::wavmDoesNotSupportSIMDOnArch:
		Errors::fatalf("WAVM does not support SIMD on the host CPU architecture.\n");
	case TargetValidationResult::memory64Requires64bitTarget:
		Errors::fatalf("Target CPU (%s) does not support 64-bit memories.\n",
					   targetSpec.cpu.c_str());
	case TargetValidationResult::table64Requires64bitTarget:
		Errors::fatalf("Target CPU (%s) does not support 64-bit tables.\n", targetSpec.cpu.c_str());

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
	if(optimize) { optimizeLLVMModule(llvmModule, true, targetMachine.get()); }

	// Print the LLVM IR.
	return printModule(llvmModule);
}