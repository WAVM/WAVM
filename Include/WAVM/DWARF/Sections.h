#pragma once

#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace DWARF {

	struct Sections
	{
		const U8* debugInfo = nullptr;
		Uptr debugInfoSize = 0;
		const U8* debugAbbrev = nullptr;
		Uptr debugAbbrevSize = 0;
		const U8* debugStr = nullptr;
		Uptr debugStrSize = 0;
		const U8* debugLine = nullptr;
		Uptr debugLineSize = 0;
		const U8* debugAddr = nullptr;
		Uptr debugAddrSize = 0;
		const U8* debugStrOffsets = nullptr;
		Uptr debugStrOffsetsSize = 0;
	};

}}
