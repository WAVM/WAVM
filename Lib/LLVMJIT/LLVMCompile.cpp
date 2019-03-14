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
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/ilist_iterator.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Scalar.h"
#if LLVM_VERSION_MAJOR >= 7
#include "llvm/Transforms/Utils.h"
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

#define VERIFY_MODULE WAVM_DEBUG
#define DUMP_UNOPTIMIZED_MODULE 0
#define DUMP_OPTIMIZED_MODULE 0
#define DUMP_OBJECT 0

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

static Uptr printedModuleId = 0;

static void printModule(const llvm::Module& llvmModule, const char* filename)
{
	std::error_code errorCode;
	std::string augmentedFilename
		= std::string(filename) + std::to_string(printedModuleId++) + ".ll";
	llvm::raw_fd_ostream dumpFileStream(
		augmentedFilename, errorCode, llvm::sys::fs::OpenFlags::F_Text);
	llvmModule.print(dumpFileStream, nullptr);
	Log::printf(Log::debug, "Dumped LLVM module to: %s\n", augmentedFilename.c_str());
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
		wavmAssert(offset + numBytes > offset && offset + numBytes <= U64(output.size()));
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
	fpm.doInitialization();
	for(auto functionIt = llvmModule.begin(); functionIt != llvmModule.end(); ++functionIt)
	{ fpm.run(*functionIt); }

	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond(
			"Optimized LLVM module", optimizationTimer, (F64)llvmModule.size(), "functions");
	}

	// Dump the optimized module if desired.
	if(shouldLogMetrics && DUMP_OPTIMIZED_MODULE) { printModule(llvmModule, "llvmOptimizedDump"); }
}

std::vector<U8> LLVMJIT::compileLLVMModule(LLVMContext& llvmContext,
										   llvm::Module&& llvmModule,
										   bool shouldLogMetrics)
{
	auto targetTriple = llvm::sys::getProcessTriple();
#ifdef __APPLE__
	// Didn't figure out exactly why, but this works around a problem with the MacOS dynamic loader.
	// Without it, our symbols can't be found in the JITed object file.
	targetTriple += "-elf";
#endif
	std::unique_ptr<llvm::TargetMachine> targetMachine(
		llvm::EngineBuilder().selectTarget(llvm::Triple(targetTriple),
										   "",
										   llvm::sys::getHostCPUName(),
										   llvm::SmallVector<std::string, 0>{}));

	// Get a target machine object for this host, and set the module to use its data layout.
	llvmModule.setDataLayout(targetMachine->createDataLayout());

	// Dump the module if desired.
	if(shouldLogMetrics && DUMP_UNOPTIMIZED_MODULE) { printModule(llvmModule, "llvmDump"); }

	// Verify the module.
	if(shouldLogMetrics && VERIFY_MODULE)
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
		errorUnless(!targetMachine->addPassesToEmitMC(passManager, mcContext, objectStream));
		passManager.run(llvmModule);
		objectBytes = objectStream.getOutput();
	}
	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond(
			"Generated machine code", machineCodeTimer, (F64)llvmModule.size(), "functions");
	}

	if(shouldLogMetrics && DUMP_OBJECT)
	{
		// Dump the object file.
		std::error_code errorCode;
		static Uptr dumpedObjectId = 0;
		std::string augmentedFilename
			= std::string("jitObject") + std::to_string(dumpedObjectId++) + ".o";
		llvm::raw_fd_ostream dumpFileStream(
			augmentedFilename, errorCode, llvm::sys::fs::OpenFlags::F_None);
		dumpFileStream.write((const char*)objectBytes.data(), objectBytes.size());
		Log::printf(Log::Category::debug, "Dumped object file to: %s\n", augmentedFilename.c_str());
	}

	return objectBytes;
}

std::vector<U8> LLVMJIT::compileModule(const IR::Module& irModule)
{
	LLVMContext llvmContext;

	// Emit LLVM IR for the module.
	llvm::Module llvmModule("", llvmContext);
	emitModule(irModule, llvmContext, llvmModule);

	// Compile the LLVM IR to object code.
	return compileLLVMModule(llvmContext, std::move(llvmModule), true);
}
