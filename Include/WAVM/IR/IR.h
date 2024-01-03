#pragma once

#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace IR {
	inline constexpr U64 maxMemory32Pages = 65536;           // 2^16 pages -> 2^32 bytes
	inline constexpr U64 maxMemory64Pages = 281474976710656; // 2^48 pages -> 2^64 bytes
	inline constexpr U64 maxTable32Elems = UINT32_MAX;
	inline constexpr U64 maxTable64Elems = UINT64_MAX;
	inline constexpr Uptr numBytesPerPage = 65536;
	inline constexpr Uptr numBytesPerPageLog2 = 16;
	inline constexpr Uptr numBytesTaggedPerPage = numBytesPerPage / 16u;
	inline constexpr Uptr numBytesTaggedPerPageLog2 = 12;
	inline constexpr Uptr maxReturnValues = 16;
}}
