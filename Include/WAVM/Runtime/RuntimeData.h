#pragma once

//
// Data structures that are used to share data between WAVM C++ code and the compiled WASM code.
//

#include <atomic>
#include <map>

#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace LLVMJIT {
	struct LoadedModule;
}}

namespace WAVM { namespace Runtime {
	// Forward declarations
	struct Compartment;
	struct Context;
	struct ExceptionTypeInstance;
	struct Object;
	struct TableInstance;
	struct MemoryInstance;

	// Runtime object types. This must be a superset of IR::ObjectKind, with IR::ObjectKind
	// values having the same representation in Runtime::ObjectKind.
	enum class ObjectKind : U8
	{
		// Standard object kinds that may be imported/exported from WebAssembly modules.
		function = 0,
		table = 1,
		memory = 2,
		global = 3,
		exceptionType = 4,

		// Runtime-specific object kinds that are only used by transient runtime objects.
		moduleInstance = 5,
		context = 6,
		compartment = 7,

		invalid = 0xff,
	};
	static_assert(Uptr(IR::ObjectKind::function) == Uptr(ObjectKind::function),
				  "IR::ObjectKind::function != ObjectKind::function");
	static_assert(Uptr(IR::ObjectKind::table) == Uptr(ObjectKind::table),
				  "IR::ObjectKind::table != ObjectKind::table");
	static_assert(Uptr(IR::ObjectKind::memory) == Uptr(ObjectKind::memory),
				  "IR::ObjectKind::memory != ObjectKind::memory");
	static_assert(Uptr(IR::ObjectKind::global) == Uptr(ObjectKind::global),
				  "IR::ObjectKind::global != ObjectKind::global");
	static_assert(Uptr(IR::ObjectKind::exceptionType) == Uptr(ObjectKind::exceptionType),
				  "IR::ObjectKind::exceptionTypeInstance != ObjectKind::exceptionType");

#define compartmentReservedBytes (4ull * 1024 * 1024 * 1024)

	enum
	{
		maxThunkArgAndReturnBytes = 256,
		maxGlobalBytes = 4096 - maxThunkArgAndReturnBytes,
		maxMutableGlobals = maxGlobalBytes / sizeof(IR::UntaggedValue),
		maxMemories = 255,
		maxTables = (4096 - maxMemories * sizeof(void*) - sizeof(Compartment*)) / sizeof(void*),
		compartmentRuntimeDataAlignmentLog2 = 32,
		contextRuntimeDataAlignment = 4096
	};

	static_assert(sizeof(IR::UntaggedValue) * IR::maxReturnValues <= maxThunkArgAndReturnBytes,
				  "maxThunkArgAndReturnBytes must be large enough to hold IR::maxReturnValues * "
				  "sizeof(UntaggedValue)");

	struct ContextRuntimeData
	{
		U8 thunkArgAndReturnData[maxThunkArgAndReturnBytes];
		IR::UntaggedValue mutableGlobals[maxMutableGlobals];
	};

	static_assert(sizeof(ContextRuntimeData) == 4096, "");

	struct CompartmentRuntimeData
	{
		Compartment* compartment;
		void* memoryBases[maxMemories];
		void* tableBases[maxTables];
		ContextRuntimeData contexts[1]; // Actually [maxContexts], but at least MSVC doesn't allow
										// declaring arrays that large.
	};

	enum
	{
		maxContexts
		= 1024 * 1024 - offsetof(CompartmentRuntimeData, contexts) / sizeof(ContextRuntimeData)
	};

	static_assert(offsetof(CompartmentRuntimeData, contexts) % 4096 == 0,
				  "CompartmentRuntimeData::contexts isn't page-aligned");
	static_assert(U64(offsetof(CompartmentRuntimeData, contexts))
						  + U64(maxContexts) * sizeof(ContextRuntimeData)
					  == compartmentReservedBytes,
				  "CompartmentRuntimeData isn't the expected size");

	struct ExceptionData
	{
		Uptr typeId;
		ExceptionTypeInstance* typeInstance;
		U8 isUserException;
		IR::UntaggedValue arguments[1];

		static Uptr calcNumBytes(Uptr numArguments)
		{
			return offsetof(ExceptionData, arguments) + numArguments * sizeof(IR::UntaggedValue);
		}
	};

	struct Object
	{
		const ObjectKind kind;
	};

	// Metadata about a function, used to hold data that can't be emitted directly in an object
	// file, or must be mutable.
	struct FunctionMutableData
	{
		LLVMJIT::LoadedModule* jitModule = nullptr;
		Runtime::FunctionInstance* function = nullptr;
		Uptr numCodeBytes = 0;
		std::atomic<Uptr> numRootReferences{0};
		std::map<U32, U32> offsetToOpIndexMap;
		std::string debugName;

		FunctionMutableData(std::string&& inDebugName) : debugName(inDebugName) {}
	};

	struct FunctionInstance
	{
		Object object;
		FunctionMutableData* mutableData;
		const Uptr moduleInstanceId;
		const IR::FunctionType::Encoding encodedType;
		const U8 code[1];

		FunctionInstance(FunctionMutableData* inMutableData,
						 Uptr inModuleInstanceId,
						 IR::FunctionType::Encoding inEncodedType)
		: object{ObjectKind::function}
		, mutableData(inMutableData)
		, moduleInstanceId(inModuleInstanceId)
		, encodedType(inEncodedType)
		, code{0xcc} // int3
		{
		}
	};

	inline CompartmentRuntimeData* getCompartmentRuntimeData(ContextRuntimeData* contextRuntimeData)
	{
		return reinterpret_cast<CompartmentRuntimeData*>(reinterpret_cast<Uptr>(contextRuntimeData)
														 & 0xffffffff00000000);
	}
}}
