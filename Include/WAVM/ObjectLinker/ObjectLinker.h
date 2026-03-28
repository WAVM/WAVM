#pragma once

#include <functional>
#include <string>
#include <vector>
#include "WAVM/DWARF/Sections.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Platform/Unwind.h"

namespace WAVM { namespace ObjectLinker {

	struct LinkResult
	{
		U8* imageBase = nullptr;
		Uptr numImagePages = 0;
		Uptr numCodeBytes = 0;
		Uptr numReadOnlyBytes = 0;
		Uptr numReadWriteBytes = 0;

		Platform::UnwindInfo unwindInfo;

		struct Symbol
		{
			std::string name;
			Uptr address;
			Uptr size;
		};
		std::vector<Symbol> definedSymbols;

		DWARF::Sections dwarf;
	};

	// Callback to allocate and commit virtual pages for the linked image.
	// If null, linkObject uses the default Platform::allocateVirtualPages path.
	using PageAllocator = std::function<U8*(Uptr numPages)>;

	// Links a single object file. All external symbols must be provided in
	// importedSymbolMap (caller merges runtime symbols + module imports).
	// Fatal error on unresolved symbols or unsupported relocations.
	WAVM_API void linkObject(U8* objectBytes,
							 Uptr objectNumBytes,
							 const HashMap<std::string, Uptr>& importedSymbolMap,
							 LinkResult& outResult,
							 PageAllocator allocatePages = nullptr);

}}
