#pragma once

//
// Data structures that are used to share data between WAVM C++ code and the compiled WASM code.
//

#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace Runtime {
	// Forward declarations
	struct Compartment;
	struct Context;
	struct ExceptionTypeInstance;
	struct Object;
	struct TableInstance;
	struct MemoryInstance;

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
		ExceptionTypeInstance* typeInstance;
		U8 isUserException;
		IR::UntaggedValue arguments[1];

		static Uptr calcNumBytes(Uptr numArguments)
		{
			return offsetof(ExceptionData, arguments) + numArguments * sizeof(IR::UntaggedValue);
		}
	};

	struct AnyReferee
	{
		Object* object;
	};

	struct AnyFunc
	{
		AnyReferee anyRef;
		IR::FunctionType::Encoding functionTypeEncoding;
		U8 code[1];
	};

	inline CompartmentRuntimeData* getCompartmentRuntimeData(ContextRuntimeData* contextRuntimeData)
	{
		return reinterpret_cast<CompartmentRuntimeData*>(reinterpret_cast<Uptr>(contextRuntimeData)
														 & 0xffffffff00000000);
	}
}}
