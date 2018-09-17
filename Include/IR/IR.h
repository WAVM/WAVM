#pragma once

#include "Inline/BasicTypes.h"

#ifndef IR_API
#define IR_API DLL_IMPORT
#endif

namespace IR
{
	enum : U64
	{
		maxMemoryPages = 65536,
		maxTableElems = U64(UINT32_MAX) + 1
	};
	enum : Uptr
	{
		numBytesPerPage = (Uptr)65536,
		numBytesPerPageLog2 = (Uptr)16,
		maxReturnValues = (Uptr)16,
	};

	struct FeatureSpec
	{
		// A feature flag for the MVP, just so the MVP operators can reference it as the required
		// feature flag.
		bool mvp = true;

		// Proposed standard extensions
		bool importExportMutableGlobals = true;
		bool extendedNamesSection = true;
		bool simd = true;
		bool atomics = true;
		bool exceptionHandling = true;
		bool nonTrappingFloatToInt = true;
		bool extendedSignExtension = true;
		bool multipleResultsAndBlockParams = true;
		bool bulkMemoryOperations = true;
		bool referenceTypes = true;

		// WAVM-specific extensions
		bool sharedTables = true;
		bool requireSharedFlagForAtomicOperators = false; // (true is standard)

		Uptr maxLocals = 65536;
		Uptr maxLabelsPerFunction = UINTPTR_MAX;
	};
}
