#pragma once

#include <map>
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
	struct ExceptionTypeInstance;
	struct FunctionInstance;
	struct ModuleInstance;
}}

namespace WAVM { namespace LLVMJIT {
	// Compiles a module to object code.
	LLVMJIT_API std::vector<U8> compileModule(const IR::Module& irModule);

	// An opaque type that can be used to reference a loaded JIT module.
	struct LoadedModule;

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
			Uptr mutableGlobalId;
		};
	};

	struct ExceptionTypeBinding
	{
		Uptr id;
	};

	// Loads a module from object code, and binds its undefined symbols to the provided bindings.
	LLVMJIT_API LoadedModule* loadModule(
		const std::vector<U8>& objectFileBytes,
		HashMap<std::string, FunctionBinding>&& wavmIntrinsicsExportMap,
		std::vector<IR::FunctionType>&& types,
		std::vector<FunctionBinding>&& functionImports,
		std::vector<TableBinding>&& tables,
		std::vector<MemoryBinding>&& memories,
		std::vector<GlobalBinding>&& globals,
		std::vector<ExceptionTypeBinding>&& exceptionTypes,
		MemoryBinding defaultMemory,
		TableBinding defaultTable,
		ModuleInstanceBinding moduleInstance,
		Uptr tableReferenceBias,
		const std::vector<Runtime::FunctionMutableData*>& functionDefMutableDatas);

	// Unloads a JIT module, freeings its memory.
	LLVMJIT_API void unloadModule(LoadedModule* loadedModule);

	// Finds the JIT function whose code contains the given address. If no JIT function contains the
	// given address, returns null.
	LLVMJIT_API Runtime::FunctionInstance* getFunctionByAddress(Uptr address);

	typedef Runtime::ContextRuntimeData* (*InvokeThunkPointer)(Runtime::FunctionInstance*,
															   Runtime::ContextRuntimeData*);

	// Generates an invoke thunk for a specific function type.
	LLVMJIT_API InvokeThunkPointer getInvokeThunk(IR::FunctionType functionType);

	// Generates a thunk to call a native function from generated code.
	LLVMJIT_API Runtime::FunctionInstance* getIntrinsicThunk(
		void* nativeFunction,
		IR::FunctionType functionType,
		IR::CallingConvention callingConvention,
		const char* debugName);
}}
