#pragma once

// Internal types shared between ObjectLinker.cpp and format-specific files.

#include <cinttypes>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/IntCasts.h"
#include "WAVM/ObjectLinker/ObjectLinker.h"

namespace WAVM { namespace ObjectLinker {

	struct LinkResult;

	enum class Arch
	{
		x86_64,
		aarch64
	};

	enum class SectionAccess
	{
		readExecute,
		readOnly,
		readWrite,
	};

	struct SectionLayoutInfo
	{
		Uptr dataSize;
		Uptr alignment;
		SectionAccess access;
	};

	// Stub size: 16 bytes for both x86-64 (14 + 2 padding) and AArch64 (16 exactly).
	inline constexpr Uptr stubSlotSize = 16;

	struct ImageLayout
	{
		U8* imageBase = nullptr;
		Uptr numPages = 0;
		Uptr numSections = 0;
		std::vector<Uptr> sectionLoadAddresses;
		Uptr codeSize = 0;
		Uptr roSize = 0;
		Uptr rwSize = 0;
		U8* stubBase = nullptr;
		U8* stubEnd = nullptr;
		U8* stubCurrent = nullptr;
		U8* gotBase = nullptr;
		U8* gotEnd = nullptr;
		U8* gotCurrent = nullptr;
		U8* extraReadOnly = nullptr;
		HashMap<Uptr, Uptr> stubCache; // target address -> stub address
		HashMap<Uptr, Uptr> gotCache;  // symbol address -> GOT entry address
	};

	ImageLayout layoutImage(const std::vector<SectionLayoutInfo>& sections,
							Uptr numStubBytes,
							Uptr numGotBytes,
							Uptr numExtraReadOnlyBytes = 0,
							PageAllocator allocatePages = nullptr);

	void protectImage(const ImageLayout& layout);

	template<Arch arch> Uptr getOrCreateStub(ImageLayout& layout, Uptr targetAddr);

	Uptr getOrCreateGotEntry(ImageLayout& layout, Uptr symbolAddr);

	// Little-endian read/write helpers.
	template<typename T> inline T readLE(const U8* p)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		T value;
		memcpy(&value, p, sizeof(T));
		return value;
	}
	template<typename T> inline void writeLE(U8* p, T value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		memcpy(p, &value, sizeof(T));
	}

	// ---- Shared relocation operations (both architectures) ----

	inline void relocAbs64(U8* fixup, U64 symbolValue, I64 addend)
	{
		writeLE<U64>(fixup, symbolValue + wrap<U64>(addend));
	}

	inline void relocPrel32(U8* fixup, U64 symbolValue, I64 addend, Uptr relocAddr)
	{
		I64 delta = wrap<I64>(symbolValue + wrap<U64>(addend) - relocAddr);
		writeLE<I32>(fixup, wrap<I32>(delta));
	}

	inline void relocImageRel32(U8* fixup, U64 symbolValue, I64 addend, Uptr imageBase)
	{
		I64 delta = wrap<I64>(symbolValue + wrap<U64>(addend) - imageBase);
		writeLE<I32>(fixup, narrow<I32>("imagerel32 relocation", delta));
	}

	inline void relocSecRel32(U8* fixup, U64 symbolValue, I64 addend, Uptr sectionBase)
	{
		I64 delta = wrap<I64>(symbolValue + wrap<U64>(addend) - sectionBase);
		writeLE<I32>(fixup, narrow<I32>("secrel32 relocation", delta));
	}

	// ---- x86-64-specific relocation operations ----

	inline void relocAbs32(U8* fixup, U64 symbolValue, I64 addend)
	{
		U64 value = symbolValue + wrap<U64>(addend);
		writeLE<U32>(fixup, narrow<U32>("abs32 relocation", value));
	}

	inline void relocAbs32s(U8* fixup, U64 symbolValue, I64 addend)
	{
		I64 svalue = wrap<I64>(symbolValue + wrap<U64>(addend));
		writeLE<I32>(fixup, narrow<I32>("abs32s relocation", svalue));
	}

	inline void relocRel32(U8* fixup,
						   U64 symbolValue,
						   I64 addend,
						   Uptr relocAddr,
						   ImageLayout& layout)
	{
		U64 value = symbolValue + wrap<U64>(addend);
		I64 delta = wrap<I64>(value) - wrap<I64>(relocAddr);
		if(delta > std::numeric_limits<I32>::max() || delta < std::numeric_limits<I32>::min())
		{
			Uptr stubAddr = getOrCreateStub<Arch::x86_64>(layout, value);
			delta = wrap<I64>(stubAddr - relocAddr);
		}
		writeLE<I32>(fixup, narrow<I32>("rel32 relocation", delta));
	}

	inline void relocRel64(U8* fixup, U64 symbolValue, I64 addend, Uptr relocAddr)
	{
		I64 delta = wrap<I64>(symbolValue + wrap<U64>(addend) - relocAddr);
		writeLE<I64>(fixup, delta);
	}

	// ---- AArch64-specific relocation operations ----

	inline void relocBranch26(U8* fixup,
							  U64 symbolValue,
							  I64 addend,
							  Uptr relocAddr,
							  ImageLayout& layout)
	{
		U64 value = symbolValue + wrap<U64>(addend);
		I64 delta = wrap<I64>(value) - wrap<I64>(relocAddr);
		if(delta > 0x7FFFFFF || delta < -0x8000000)
		{
			Uptr stubAddr = getOrCreateStub<Arch::aarch64>(layout, value);
			delta = wrap<I64>(stubAddr) - wrap<I64>(relocAddr);
			if(delta > 0x7FFFFFF || delta < -0x8000000)
			{
				Errors::fatalf("branch26 relocation still out of range after stub: delta=%" PRId64,
							   delta);
			}
		}
		U32 insn = readLE<U32>(fixup);
		insn = (insn & 0xFC000000) | (wrap<U32>(delta >> 2) & 0x03FFFFFF);
		writeLE<U32>(fixup, insn);
	}

	inline void relocAdrp(U8* fixup, U64 symbolValue, I64 addend, Uptr relocAddr)
	{
		U64 value = symbolValue + wrap<U64>(addend);
		I64 pageDelta = wrap<I64>(value & ~0xFFFULL) - wrap<I64>(relocAddr & ~Uptr(0xFFF));
		I64 pageCount = pageDelta >> 12;
		U32 immlo = wrap<U32>(pageCount) & 3;
		U32 immhi = wrap<U32>(pageCount >> 2) & 0x7FFFF;
		U32 insn = readLE<U32>(fixup);
		insn = (insn & 0x9F00001F) | (immlo << 29) | (immhi << 5);
		writeLE<U32>(fixup, insn);
	}

	inline void relocAddLo12(U8* fixup, U64 symbolValue, I64 addend)
	{
		U32 imm12 = wrap<U32>(symbolValue + wrap<U64>(addend)) & 0xFFF;
		U32 insn = readLE<U32>(fixup);
		insn = (insn & 0xFFC003FF) | (imm12 << 10);
		writeLE<U32>(fixup, insn);
	}

	inline void relocLdstLo12(U8* fixup, U64 symbolValue, I64 addend)
	{
		U32 insn = readLE<U32>(fixup);
		U32 size = (insn >> 30) & 3;
		U32 opc = (insn >> 22) & 3;
		U32 shift = size;
		if(size == 0 && opc >= 2) { shift = 4; } // 128-bit V register load/store
		U32 imm12 = (wrap<U32>(symbolValue + wrap<U64>(addend)) & 0xFFF) >> shift;
		insn = (insn & 0xFFC003FF) | (imm12 << 10);
		writeLE<U32>(fixup, insn);
	}

	// Patch a MOVZ/MOVK instruction's 16-bit immediate field (bits [20:5]).
	// groupShift is 0, 16, 32, or 48 for G0, G1, G2, G3 respectively.
	inline void relocMovwUabs(U8* fixup, U64 symbolValue, I64 addend, U32 groupShift)
	{
		U64 value = symbolValue + wrap<U64>(addend);
		U16 imm16 = U16((value >> groupShift) & 0xFFFF);
		U32 insn = readLE<U32>(fixup);
		insn = (insn & 0xFFE0001F) | (U32(imm16) << 5);
		writeLE<U32>(fixup, insn);
	}

	// Format-specific linkers.
	void linkMachO(U8* bytes,
				   Uptr size,
				   const HashMap<std::string, Uptr>& imports,
				   LinkResult& result,
				   PageAllocator allocatePages);
	void linkELF(U8* bytes,
				 Uptr size,
				 const HashMap<std::string, Uptr>& imports,
				 LinkResult& result,
				 PageAllocator allocatePages);
	void linkCOFF(U8* bytes,
				  Uptr size,
				  const HashMap<std::string, Uptr>& imports,
				  LinkResult& result,
				  PageAllocator allocatePages);

}}
