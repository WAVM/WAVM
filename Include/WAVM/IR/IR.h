#pragma once

#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace IR {
	enum : U64
	{
		maxMemoryPages = 65536,
		maxTableElems = U64(INT32_MAX)
	};
	enum : Uptr
	{
		numBytesPerPage = (Uptr)65536,
		numBytesPerPageLog2 = (Uptr)16,
		maxReturnValues = (Uptr)16,
	};
}}
