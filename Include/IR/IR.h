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

	struct FeatureSpec
	{
		// A feature flag for the MVP, just so the MVP operators can reference it as the required feature flag.
		const bool mvp = true;

		// Proposed standard extensions
		bool importExportMutableGlobals = false;
		bool extendedNamesSection = true;
		bool simd = true;
		bool atomics = true;
		bool exceptionHandling = true;
		bool nonTrappingFloatToInt = true;
		bool extendedSignExtension = true;

		// WAVM-specific extensions
		bool launchThread = false;
		bool sharedTables = false;
		bool requireSharedFlagForAtomicOperators = false; // (true is standard)
	};
}
