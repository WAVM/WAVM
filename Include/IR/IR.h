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

	enum { requireSharedFlagForAtomicOperators = false };
	enum { allowImportExportMutableGlobals = false }; // This needs to stay off until the spec tests are updated
	enum { enableWritingExtendedNamesSection = true };
}
