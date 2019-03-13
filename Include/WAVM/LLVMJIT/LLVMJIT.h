#pragma once

#include <memory>
#include <string>
#include <vector>

#include "WAVM/IR/Types.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Runtime/RuntimeData.h"

// Forward declarations
namespace WAVM { namespace IR {
	struct Module;
	struct UntaggedValue;

	enum class CallingConvention;
}}
namespace WAVM { namespace Runtime {
	struct ContextRuntimeData;
	struct ExceptionType;
	struct Function;
	struct ModuleInstance;
}}

namespace WAVM { namespace LLVMJIT {
	// Compiles a module to object code.
	LLVMJIT_API std::vector<U8> compileModule(const IR::Module& irModule);

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
		IR::CallingConvention callingConvention;
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
	LLVMJIT_API std::shared_ptr<Module> loadModule(
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

	// Finds the JIT function whose code contains the given address. If no JIT function contains the
	// given address, returns null.
	LLVMJIT_API Runtime::Function* getFunctionByAddress(Uptr address);

	// Generates an invoke thunk for a specific function type.
	LLVMJIT_API Runtime::InvokeThunkPointer getInvokeThunk(IR::FunctionType functionType);

	// Generates a thunk to call a native function from generated code.
	LLVMJIT_API Runtime::Function* getIntrinsicThunk(void* nativeFunction,
													 IR::FunctionType functionType,
													 IR::CallingConvention callingConvention,
													 const char* debugName);
}}
