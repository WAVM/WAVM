#pragma once

//
// Data structures that are used to share data between WAVM C++ code and the compiled WASM code.
//

#include "IR/Value.h"
#include "Inline/BasicTypes.h"

#ifndef RUNTIME_API
#define RUNTIME_API DLL_IMPORT
#endif

namespace Runtime
{
	// Forward declarations
	struct Compartment;
	struct Context;
	struct TableInstance;
	struct MemoryInstance;

#define compartmentReservedBytes (4ull * 1024 * 1024 * 1024)

	enum
	{
		maxThunkArgAndReturnBytes = 256,
		maxGlobalBytes = 4096 - maxThunkArgAndReturnBytes,
		maxMemories = 255,
		maxTables = 256,
		compartmentRuntimeDataAlignmentLog2 = 32,
		contextRuntimeDataAlignment = 4096
	};

	static_assert(sizeof(IR::UntaggedValue) * IR::maxReturnValues <= maxThunkArgAndReturnBytes,
				  "maxThunkArgAndReturnBytes must be large enough to hold IR::maxReturnValues * "
				  "sizeof(UntaggedValue)");

	struct ContextRuntimeData
	{
		U8 thunkArgAndReturnData[maxThunkArgAndReturnBytes];
		U8 globalData[maxGlobalBytes];
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
	static_assert(offsetof(CompartmentRuntimeData, contexts[maxContexts])
					  == compartmentReservedBytes,
				  "CompartmentRuntimeData isn't the expected size");

	struct ExceptionData
	{
		ExceptionTypeInstance* typeInstance;
		U8 isUserException;
		IR::UntaggedValue arguments[1];

		static Uptr calcNumBytes(Uptr numArguments)
		{
			return offsetof(ExceptionData, arguments) + numArguments * sizeof(IR::UntaggedValue);
		}
	};

	inline CompartmentRuntimeData* getCompartmentRuntimeData(ContextRuntimeData* contextRuntimeData)
	{
		return reinterpret_cast<CompartmentRuntimeData*>(reinterpret_cast<Uptr>(contextRuntimeData)
														 & 0xffffffff00000000);
	}

	RUNTIME_API Context* getContextFromRuntimeData(ContextRuntimeData* contextRuntimeData);
	RUNTIME_API ContextRuntimeData* getContextRuntimeData(Context* context);
	RUNTIME_API TableInstance* getTableFromRuntimeData(
		struct ContextRuntimeData* contextRuntimeData,
		Uptr tableId);
	RUNTIME_API MemoryInstance* getMemoryFromRuntimeData(
		struct ContextRuntimeData* contextRuntimeData,
		Uptr memoryId);
}
