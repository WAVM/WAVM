#pragma once

#include <string>
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace IR {
	struct Module;
}}

namespace WAVM { namespace WAST {
	// Prints a module in WAST format.
	WASTPRINT_API std::string print(const IR::Module& module);
}}
