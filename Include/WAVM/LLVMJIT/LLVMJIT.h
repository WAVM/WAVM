#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Platform/CPU.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

// Forward declarations
namespace WAVM { namespace IR {
	struct Module;
	struct UntaggedValue;
	struct FeatureSpec;

	enum class CallingConvention;
}}
namespace WAVM { namespace Runtime {
	struct ContextRuntimeData;
	struct ExceptionType;
	struct Function;
	struct Instance;
}}

namespace WAVM { namespace LLVMJIT {

	enum class TargetArch
	{
		x86_64,
		aarch64
	};

	enum class TargetOS
	{
		// "linux_" avoids conflict with the "linux" preprocessor macro.
		linux_,
		macos,
		windows
	};

	struct TargetSpec
	{
		TargetArch arch;
		TargetOS os;
		std::string cpu;

		union
		{
			Platform::X86CPUFeatures<std::optional<bool>> x86FeatureOverrides;
			Platform::AArch64CPUFeatures<std::optional<bool>> aarch64FeatureOverrides;
		};

		TargetSpec() : arch(TargetArch::x86_64), os(TargetOS::windows), x86FeatureOverrides{} {}
	};

	WAVM_API std::string asString(const TargetSpec& targetSpec);

	enum class TargetValidationResult
	{
		valid,
		invalidTargetSpec,
		unsupportedArchitecture,
		x86CPUDoesNotSupportSSE41,
		wavmDoesNotSupportSIMDOnArch,
		memory64Requires64bitTarget,
		table64Requires64bitTarget
	};

	WAVM_API TargetSpec getHostTargetSpec();

	WAVM_API TargetValidationResult validateTarget(const TargetSpec& targetSpec,
												   const IR::FeatureSpec& featureSpec);

	struct Version
	{
		Uptr llvmMajor;
		Uptr llvmMinor;
		Uptr llvmPatch;

		Uptr llvmjitVersion;
	};

	WAVM_API Version getVersion();

	// Compile a module to object code with the host target spec.
	// Cannot fail if validateTarget(targetSpec, irModule.featureSpec) == valid.
	WAVM_API std::vector<U8> compileModule(const IR::Module& irModule,
										   const TargetSpec& targetSpec);

	WAVM_API std::string emitLLVMIR(const IR::Module& irModule,
									const TargetSpec& targetSpec,
									bool optimize);

	WAVM_API std::string disassembleObject(const TargetSpec& targetSpec,
										   const std::vector<U8>& objectBytes);

	// Result of disassembling a single instruction.
	struct DisassembledInstruction
	{
		Uptr numBytes;      // Number of bytes consumed (0 if disassembly failed)
		char mnemonic[256]; // Human-readable instruction text
	};

	// Opaque disassembler object. Pre-create one to avoid per-call setup costs.
	struct Disassembler;

	// Create a disassembler for the given target. Thread-safe to call concurrently.
	WAVM_API std::shared_ptr<Disassembler> createDisassembler(const TargetSpec& targetSpec);

	// Disassemble a single instruction. If the disassembler mutex can't be acquired
	// (e.g. called from a signal handler while another thread holds it), returns
	// a result with numBytes == 0.
	WAVM_API DisassembledInstruction
	disassembleInstruction(const std::shared_ptr<Disassembler>& disasm,
						   const U8* bytes,
						   Uptr numBytes);

	// Convenience overload that creates a temporary disassembler.
	WAVM_API DisassembledInstruction disassembleInstruction(const TargetSpec& targetSpec,
															const U8* bytes,
															Uptr numBytes);

	// Find the first instruction boundary at or after targetOffset within a code region.
	// Handles architecture differences: on fixed-width ISAs (AArch64), aligns directly;
	// on variable-width ISAs (x86-64), disassembles from the start.
	// Returns the offset of the instruction boundary.
	WAVM_API Uptr findInstructionBoundary(const std::shared_ptr<Disassembler>& disasm,
										  const U8* code,
										  Uptr codeSize,
										  Uptr targetOffset);

	// An opaque type that can be used to reference a loaded JIT module.
	struct Module;

	//
	// Structs that are passed to loadModule to bind undefined symbols in object code to values.
	//

	struct InstanceBinding
	{
		Uptr id;
	};

	struct FunctionBinding
	{
		const void* code;
	};

	struct TableBinding
	{
		Uptr id;
	};

	struct MemoryBinding
	{
		Uptr id;
	};

	struct GlobalBinding
	{
		IR::GlobalType type;
		union
		{
			const IR::UntaggedValue* immutableValuePointer;
			Uptr mutableGlobalIndex;
		};
	};

	struct ExceptionTypeBinding
	{
		Uptr id;
	};

	// Loads a module from object code, and binds its undefined symbols to the provided bindings.
	WAVM_API std::shared_ptr<Module> loadModule(
		const std::vector<U8>& objectFileBytes,
		HashMap<std::string, FunctionBinding>&& wavmIntrinsicsExportMap,
		std::vector<IR::FunctionType>&& types,
		std::vector<FunctionBinding>&& functionImports,
		std::vector<TableBinding>&& tables,
		std::vector<MemoryBinding>&& memories,
		std::vector<GlobalBinding>&& globals,
		std::vector<ExceptionTypeBinding>&& exceptionTypes,
		InstanceBinding instance,
		Uptr tableReferenceBias,
		const std::vector<Runtime::FunctionMutableData*>& functionDefMutableDatas,
		std::string&& debugName);

	struct InstructionSource
	{
		Runtime::Function* function;
		Uptr instructionIndex;
	};

	// Finds the JIT function(s) and instruction index at the given address.
	// Writes to a fixed-size array, returns number of entries (innermost first).
	// Signal-safe: no heap allocations.
	WAVM_API Uptr getInstructionSourceByAddress(Uptr address,
												InstructionSource* outSources,
												Uptr maxSources);

	// Generates an invoke thunk for a specific function type.
	WAVM_API Runtime::InvokeThunkPointer getInvokeThunk(IR::FunctionType functionType);
}}
