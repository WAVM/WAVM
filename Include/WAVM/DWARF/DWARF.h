#pragma once

#include "WAVM/DWARF/Sections.h"
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace DWARF {

	// A source location at a given address, one frame in an inline chain.
	struct SourceLocation
	{
		const char* linkageName; // into .debug_str, or nullptr
		const char* name;        // into .debug_str, or nullptr
		Uptr line;
	};

	// Map an instruction address to source locations (inline chain).
	// Signal-safe: parses DWARF v4 on-the-fly, stack allocation only.
	// Returns number of locations (innermost first), 0 if not found.
	WAVM_API Uptr getSourceLocations(const Sections& sections,
									 Uptr address,
									 SourceLocation* outLocations,
									 Uptr maxLocations);

}}
