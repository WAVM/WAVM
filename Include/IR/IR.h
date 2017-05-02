#pragma once

#include "Platform/Platform.h"

#ifndef IR_API
	#define IR_API DLL_IMPORT
#endif

namespace IR
{
	enum { maxMemoryPages = (uintp)65536 };
	enum { numBytesPerPage = (uintp)65536 };
	enum { numBytesPerPageLog2 = (uintp)16 };

	enum { requireSharedFlagForAtomicOperators = false };
}
