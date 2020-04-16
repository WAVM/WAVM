#pragma once

#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace IR {
	static constexpr U64 maxMemoryPages = 65536;
	static constexpr U64 maxTableElems = UINT32_MAX;
	static constexpr Uptr numBytesPerPage = 65536;
	static constexpr Uptr numBytesPerPageLog2 = 16;
	static constexpr Uptr maxReturnValues = 16;
}}
