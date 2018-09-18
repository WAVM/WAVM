#pragma once

#ifndef WASTPRINT_API
#define WASTPRINT_API DLL_IMPORT
#endif

#include <string>
#include "Inline/BasicTypes.h"

namespace WAVM { namespace IR {
	struct Module;
}}

namespace WAVM { namespace WAST {
	// Prints a module in WAST format.
	WASTPRINT_API std::string print(const IR::Module& module);
}}
