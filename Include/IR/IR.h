#pragma once

#ifndef IR_API
	#define IR_API DLL_IMPORT
#endif

namespace IR
{
	enum : U64 { maxMemoryPages = 65536 };
	enum : U64 { maxTableElems = U64(UINT32_MAX) + 1 };
	enum { numBytesPerPage = (Uptr)65536 };
	enum { numBytesPerPageLog2 = (Uptr)16 };
	enum { maxReturnValues = (Uptr)8 };

	struct FeatureSpec
	{
		// A feature flag for the MVP, just so the MVP operators can reference it as the required feature flag.
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

		// WAVM-specific extensions
		bool sharedTables = true;
		bool requireSharedFlagForAtomicOperators = true; // (true is standard)
	};
}
