#pragma once

#include <memory>
#include <string>
#include <vector>
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
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
	struct ModuleInstance;
}}

namespace WAVM { namespace LLVMJIT {

	struct TargetSpec
	{
		std::string triple;
		std::string cpu;
	};

	enum class TargetValidationResult
	{
		valid,
		invalidTargetSpec,
		unsupportedArchitecture,
		x86CPUDoesNotSupportSSE41,
		wavmDoesNotSupportSIMDOnArch
	};

	WAVM_API TargetSpec getHostTargetSpec();

	WAVM_API TargetValidationResult validateTarget(const TargetSpec& targetSpec,
												   const IR::FeatureSpec& featureSpec);

	struct Version
	{
		Uptr llvmMajor;
		Uptr llvmMinor;
		Uptr llvmPatch;
	};

	WAVM_API Version getVersion();

	// Compile a module to object code with the host target spec.
	// Cannot fail if validateTarget(targetSpec, irModule.featureSpec) == valid.
	WAVM_API std::vector<U8> compileModule(const IR::Module& irModule,
										   const TargetSpec& targetSpec);

	WAVM_API std::string emitLLVMIR(const IR::Module& irModule,
									const TargetSpec& targetSpec,
									bool optimize);

	// An opaque type that can be used to reference a loaded JIT module.
	struct Module;

	//
	// Structs that are passed to loadModule to bind undefined symbols in object code to values.
	//

	struct ModuleInstanceBinding
	{
		Uptr id;
	};

	struct FunctionBinding
	{
		void* code;
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
		ModuleInstanceBinding moduleInstance,
		Uptr tableReferenceBias,
		const std::vector<Runtime::FunctionMutableData*>& functionDefMutableDatas);

	struct InstructionSource
	{
		Runtime::Function* function;
		Uptr instructionIndex;
	};

	// Finds the JIT function and instruction index at the given address. If no JIT function
	// contains the given address, returns an InstructionSourceInfo with function==nullptr.
	WAVM_API bool getInstructionSourceByAddress(Uptr address, InstructionSource& outSource);

	// Generates an invoke thunk for a specific function type.
	WAVM_API Runtime::InvokeThunkPointer getInvokeThunk(IR::FunctionType functionType);

	// Generates a thunk to call a native function from generated code.
	WAVM_API Runtime::Function* getIntrinsicThunk(void* nativeFunction,
												  IR::FunctionType functionType,
												  const char* debugName);
}}
