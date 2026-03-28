#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "ObjectLinkerPrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Inline/IntCasts.h"
#include "WAVM/Inline/UnalignedArrayView.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/ObjectLinker/ObjectLinker.h"
#include "WAVM/Platform/Defines.h"

using namespace WAVM;
using namespace WAVM::ObjectLinker;

// COFF header structures.

struct CoffFileHeader
{
	U16 Machine;
	U16 NumberOfSections;
	U32 TimeDateStamp;
	U32 PointerToSymbolTable;
	U32 NumberOfSymbols;
	U16 SizeOfOptionalHeader;
	U16 Characteristics;
};

struct CoffSectionHeader
{
	char Name[8];
	U32 VirtualSize;
	U32 VirtualAddress;
	U32 SizeOfRawData;
	U32 PointerToRawData;
	U32 PointerToRelocations;
	U32 PointerToLinenumbers;
	U16 NumberOfRelocations;
	U16 NumberOfLinenumbers;
	U32 Characteristics;
};

// CoffSymbol is 18 bytes on disk; without packing the compiler adds 2 bytes of tail padding.
WAVM_PACKED_STRUCT(struct CoffSymbol {
	union
	{
		char ShortName[8];
		struct
		{
			U32 Zeros;
			U32 Offset;
		} LongName;
	} Name;
	U32 Value;
	I16 SectionNumber;
	U16 Type;
	U8 StorageClass;
	U8 NumberOfAuxSymbols;
});

// CoffRelocation is 10 bytes on disk; without packing the compiler adds 2 bytes of tail padding.
WAVM_PACKED_STRUCT(struct CoffRelocation {
	U32 VirtualAddress;
	U32 SymbolTableIndex;
	U16 Type;
});

// COFF constants.
static constexpr U16 IMAGE_FILE_MACHINE_AMD64 = 0x8664;
static constexpr U16 IMAGE_FILE_MACHINE_ARM64 = 0xAA64;

// Section characteristics.
static constexpr U32 IMAGE_SCN_MEM_EXECUTE = 0x20000000;
static constexpr U32 IMAGE_SCN_MEM_WRITE = 0x80000000;
static constexpr U32 IMAGE_SCN_ALIGN_MASK = 0x00F00000;

// Symbol storage classes.
static constexpr U8 IMAGE_SYM_CLASS_EXTERNAL = 2;

// x86-64 COFF relocation types.
static constexpr U16 IMAGE_REL_AMD64_ADDR64 = 0x0001;
static constexpr U16 IMAGE_REL_AMD64_ADDR32NB = 0x0003;
static constexpr U16 IMAGE_REL_AMD64_REL32 = 0x0004;
static constexpr U16 IMAGE_REL_AMD64_REL32_1 = 0x0005;
static constexpr U16 IMAGE_REL_AMD64_REL32_2 = 0x0006;
static constexpr U16 IMAGE_REL_AMD64_REL32_3 = 0x0007;
static constexpr U16 IMAGE_REL_AMD64_REL32_4 = 0x0008;
static constexpr U16 IMAGE_REL_AMD64_REL32_5 = 0x0009;
static constexpr U16 IMAGE_REL_AMD64_SECTION = 0x000A;
static constexpr U16 IMAGE_REL_AMD64_SECREL = 0x000B;

// AArch64 COFF relocation types.
static constexpr U16 IMAGE_REL_ARM64_ADDR64 = 0x000E;
static constexpr U16 IMAGE_REL_ARM64_ADDR32NB = 0x0002;
static constexpr U16 IMAGE_REL_ARM64_BRANCH26 = 0x0003;
static constexpr U16 IMAGE_REL_ARM64_PAGEBASE_REL21 = 0x0004;
static constexpr U16 IMAGE_REL_ARM64_PAGEOFFSET_12A = 0x0006;
static constexpr U16 IMAGE_REL_ARM64_PAGEOFFSET_12L = 0x0007;
static constexpr U16 IMAGE_REL_ARM64_SECREL = 0x0008;
static constexpr U16 IMAGE_REL_ARM64_SECTION = 0x000D;
static constexpr U16 IMAGE_REL_ARM64_SECREL_LOW12A = 0x0009;
static constexpr U16 IMAGE_REL_ARM64_SECREL_HIGH12A = 0x000A;
static constexpr U16 IMAGE_REL_ARM64_SECREL_LOW12L = 0x000B;

// AArch64 page size and mask for ADRP calculations.
static constexpr U32 AARCH64_PAGE_SHIFT = 12;
static constexpr U64 AARCH64_PAGE_MASK = (U64(1) << AARCH64_PAGE_SHIFT) - 1;

// ADRP range: +/-4GB (21-bit signed page count, each page is 4KB).
static constexpr I64 ADRP_MAX_PAGE_COUNT = 0xFFFFF;   // +4GB
static constexpr I64 ADRP_MIN_PAGE_COUNT = -0x100000; // -4GB

// Stub sizes for out-of-range ADRP fixups.
// Data-access stub: LDR X16,[PC,#12]; modified LDR/STR; B return; .quad addr
static constexpr Uptr ADRP_DATA_STUB_SIZE = 20;
// Separated ADRP stub: LDR Xd,[PC,#8]; B return; .quad pageAddr
static constexpr Uptr ADRP_PAGE_STUB_SIZE = 16;

static const char* getCOFFX64RelocName(U16 type)
{
	switch(type)
	{
	case IMAGE_REL_AMD64_ADDR64: return "ADDR64";
	case IMAGE_REL_AMD64_ADDR32NB: return "ADDR32NB";
	case IMAGE_REL_AMD64_REL32: return "REL32";
	case IMAGE_REL_AMD64_REL32_1: return "REL32_1";
	case IMAGE_REL_AMD64_REL32_2: return "REL32_2";
	case IMAGE_REL_AMD64_REL32_3: return "REL32_3";
	case IMAGE_REL_AMD64_REL32_4: return "REL32_4";
	case IMAGE_REL_AMD64_REL32_5: return "REL32_5";
	case IMAGE_REL_AMD64_SECTION: return "SECTION";
	case IMAGE_REL_AMD64_SECREL: return "SECREL";
	default: return "?";
	}
}

static const char* getCOFFAArch64RelocName(U16 type)
{
	switch(type)
	{
	case IMAGE_REL_ARM64_ADDR64: return "ADDR64";
	case IMAGE_REL_ARM64_ADDR32NB: return "ADDR32NB";
	case IMAGE_REL_ARM64_BRANCH26: return "BRANCH26";
	case IMAGE_REL_ARM64_PAGEBASE_REL21: return "PAGEBASE_REL21";
	case IMAGE_REL_ARM64_PAGEOFFSET_12A: return "PAGEOFFSET_12A";
	case IMAGE_REL_ARM64_PAGEOFFSET_12L: return "PAGEOFFSET_12L";
	case IMAGE_REL_ARM64_SECREL: return "SECREL";
	case IMAGE_REL_ARM64_SECTION: return "SECTION";
	case IMAGE_REL_ARM64_SECREL_LOW12A: return "SECREL_LOW12A";
	case IMAGE_REL_ARM64_SECREL_HIGH12A: return "SECREL_HIGH12A";
	case IMAGE_REL_ARM64_SECREL_LOW12L: return "SECREL_LOW12L";
	default: return "?";
	}
}

static SectionAccess getCOFFSectionAccess(U32 characteristics)
{
	if(characteristics & IMAGE_SCN_MEM_EXECUTE) { return SectionAccess::readExecute; }
	if(characteristics & IMAGE_SCN_MEM_WRITE) { return SectionAccess::readWrite; }
	return SectionAccess::readOnly;
}

static Uptr getCOFFSectionAlignment(U32 characteristics)
{
	U32 alignBits = (characteristics & IMAGE_SCN_ALIGN_MASK) >> 20;
	if(alignBits == 0) { return 1; }
	return Uptr(1) << (alignBits - 1);
}

static std::string getCOFFSymbolName(const CoffSymbol& sym,
									 const char* stringTable,
									 Uptr stringTableSize)
{
	if(sym.Name.LongName.Zeros == 0)
	{
		// Long name: offset into string table.
		U32 offset = sym.Name.LongName.Offset;
		if(offset < stringTableSize) { return std::string(stringTable + offset); }
		return "";
	}
	else
	{
		// Short name: up to 8 characters, null-terminated.
		char name[sizeof(sym.Name.ShortName) + 1] = {0};
		memcpy(name, sym.Name.ShortName, sizeof(sym.Name.ShortName));
		return std::string(name);
	}
}

static void applyCOFFX64Reloc(const CoffRelocation& rel,
							  U8* fixup,
							  Uptr fixupAddr,
							  U64 symbolValue,
							  const CoffSymbol& sym,
							  const std::string& symbolName,
							  ImageLayout& layout)
{
	Log::printf(Log::traceLinking,
				"  reloc @0x%" WAVM_PRIxPTR
				" type=%s(%u) sym=%u (%s)"
				" symval=0x%" PRIx64 "\n",
				fixupAddr,
				getCOFFX64RelocName(rel.Type),
				U32(rel.Type),
				rel.SymbolTableIndex,
				symbolName.c_str(),
				symbolValue);
	switch(rel.Type)
	{
	case IMAGE_REL_AMD64_ADDR64: {
		relocAbs64(fixup, symbolValue, readLE<I64>(fixup));
		break;
	}

	case IMAGE_REL_AMD64_ADDR32NB: {
		I32 addend = readLE<I32>(fixup);
		relocImageRel32(fixup, symbolValue, addend, Uptr(layout.imageBase));
		break;
	}

	case IMAGE_REL_AMD64_REL32: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 4, fixupAddr + 4, layout);
		break;
	}

	case IMAGE_REL_AMD64_REL32_1: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 5, fixupAddr + 4, layout);
		break;
	}

	case IMAGE_REL_AMD64_REL32_2: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 6, fixupAddr + 4, layout);
		break;
	}

	case IMAGE_REL_AMD64_REL32_3: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 7, fixupAddr + 4, layout);
		break;
	}

	case IMAGE_REL_AMD64_REL32_4: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 8, fixupAddr + 4, layout);
		break;
	}

	case IMAGE_REL_AMD64_REL32_5: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 9, fixupAddr + 4, layout);
		break;
	}

	case IMAGE_REL_AMD64_SECTION: {
		// The symbol's section number (1-based).
		writeLE<U16>(fixup, wrap<U16>(sym.SectionNumber));
		break;
	}

	case IMAGE_REL_AMD64_SECREL: {
		I32 addend = readLE<I32>(fixup);
		// Section-relative offset: symbol value minus the section it's defined in.
		Uptr symSectionBase = 0;
		if(sym.SectionNumber > 0)
		{
			symSectionBase = layout.sectionLoadAddresses[wrap<U16>(sym.SectionNumber - 1)];
		}
		relocSecRel32(fixup, symbolValue, addend, symSectionBase);
		break;
	}

	default: Errors::fatalf("Unsupported COFF x86-64 relocation type: %u", rel.Type);
	}
}

// Returns the number of relocations consumed (1, or 2 when an out-of-range ADRP
// consumes the paired PAGEOFFSET relocation).
static U32 applyCOFFAArch64Reloc(const CoffRelocation& rel,
								 U8* fixup,
								 Uptr fixupAddr,
								 U64 symbolValue,
								 const CoffSymbol& sym,
								 const std::string& symbolName,
								 ImageLayout& layout,
								 const CoffRelocation* nextReloc)
{
	Log::printf(Log::traceLinking,
				"  reloc @0x%" WAVM_PRIxPTR
				" type=%s(%u) sym=%u (%s)"
				" symval=0x%" PRIx64 "\n",
				fixupAddr,
				getCOFFAArch64RelocName(rel.Type),
				U32(rel.Type),
				rel.SymbolTableIndex,
				symbolName.c_str(),
				symbolValue);
	switch(rel.Type)
	{
	case IMAGE_REL_ARM64_ADDR64: {
		relocAbs64(fixup, symbolValue, readLE<I64>(fixup));
		break;
	}

	case IMAGE_REL_ARM64_ADDR32NB: {
		I32 addend = readLE<I32>(fixup);
		relocImageRel32(fixup, symbolValue, addend, Uptr(layout.imageBase));
		break;
	}

	case IMAGE_REL_ARM64_BRANCH26: relocBranch26(fixup, symbolValue, 0, fixupAddr, layout); break;

	case IMAGE_REL_ARM64_PAGEBASE_REL21: {
		// Check if the ADRP target page is within +/-4GB range. Use unsigned subtraction
		// for defined overflow, then interpret as signed for the range check.
		U64 symbolPage = symbolValue & ~AARCH64_PAGE_MASK;
		U64 fixupPage = fixupAddr & ~Uptr(AARCH64_PAGE_MASK);
		I64 pageCount = wrap<I64>(symbolPage - fixupPage) >> AARCH64_PAGE_SHIFT;
		if(pageCount > ADRP_MAX_PAGE_COUNT || pageCount < ADRP_MIN_PAGE_COUNT)
		{
			// Out of ADRP range. Strategy depends on the paired page-offset relocation.
			bool nextIsPageOffset = nextReloc && nextReloc->VirtualAddress == rel.VirtualAddress + 4
									&& (nextReloc->Type == IMAGE_REL_ARM64_PAGEOFFSET_12A
										|| nextReloc->Type == IMAGE_REL_ARM64_PAGEOFFSET_12L);

			U8* nextFixup = fixup + 4;
			U32 nextInsn = readLE<U32>(nextFixup);

			if(nextIsPageOffset && nextReloc->Type == IMAGE_REL_ARM64_PAGEOFFSET_12L)
			{
				// ADRP + LDR/STR: emit a data-access stub.
				// The stub loads the full target address from an inline literal
				// into IP0 (X16), then performs the original LDR/STR with X16
				// as the base register, then branches back.

				// Extract the addend from the LDR/STR's imm12 field.
				U32 ldstSize = (nextInsn >> 30) & 3;
				U32 ldstOpc = (nextInsn >> 22) & 3;
				U32 shift = ldstSize;
				if(ldstSize == 0 && ldstOpc >= 2) { shift = 4; } // 128-bit
				U64 addend = U64(((nextInsn >> 10) & U32(AARCH64_PAGE_MASK)) << shift);
				U64 fullAddr = symbolValue + addend;

				if(!layout.stubCurrent || layout.stubCurrent + ADRP_DATA_STUB_SIZE > layout.stubEnd)
				{
					Errors::fatalf("Ran out of stub space for ADRP+LDR/STR data-access stub");
				}
				U8* stub = layout.stubCurrent;
				layout.stubCurrent += ADRP_DATA_STUB_SIZE;
				Uptr stubAddr = reinterpret_cast<Uptr>(stub);

				// [+0]  LDR X16, [PC, #12]: load full address from inline literal
				writeLE<U32>(stub + 0, U32(0x58000070));
				// [+4]  Modified LDR/STR: Rn = X16, imm12 = 0, rest preserved
				writeLE<U32>(stub + 4, (nextInsn & 0xFFC0001F) | (16 << 5));
				// [+8]  B (fixupAddr + 8): return to instruction after the pair
				I64 returnDelta = wrap<I64>(fixupAddr + 8) - wrap<I64>(stubAddr + 8);
				writeLE<U32>(stub + 8, 0x14000000 | (wrap<U32>(returnDelta >> 2) & 0x3FFFFFF));
				// [+12] .quad fullAddr: 8-byte inline literal
				writeLE<U64>(stub + 12, fullAddr);

				// Replace ADRP with B to stub.
				I64 branchDelta = wrap<I64>(stubAddr) - wrap<I64>(fixupAddr);
				writeLE<U32>(fixup, 0x14000000 | (wrap<U32>(branchDelta >> 2) & 0x3FFFFFF));
				// NOP the original LDR/STR (consumed by the stub).
				writeLE<U32>(nextFixup, U32(0xD503201F));

				return 2;
			}

			// ADRP + ADD: redirect through GOT.
			// Create a GOT entry holding the target address and transform the
			// following ADD into a 64-bit LDR from the GOT entry.
			if((nextInsn & 0x1F000000) == 0x11000000)
			{
				Uptr gotEntry = getOrCreateGotEntry(layout, symbolValue);
				U32 rd = nextInsn & 0x1F;
				U32 rn = (nextInsn >> 5) & 0x1F;
				U32 gotPageOffset = wrap<U32>(gotEntry) & U32(AARCH64_PAGE_MASK);
				U32 ldrImm12 = gotPageOffset >> 3; // 64-bit LDR scales by 8
				writeLE<U32>(nextFixup, 0xF9400000 | (ldrImm12 << 10) | (rn << 5) | rd);

				symbolValue = gotEntry;
				relocAdrp(fixup, symbolValue, 0, fixupAddr);
				return nextIsPageOffset ? 2 : 1;
			}

			// ADRP whose paired PAGEOFFSET is not the immediately next relocation
			// (the instruction scheduler separated them). Emit a stub that loads
			// the page address from an inline literal. The PAGEOFFSET relocation
			// will be processed independently when the iterator reaches it.
			{
				U32 adrpInsn = readLE<U32>(fixup);
				U32 rd = adrpInsn & 0x1F;
				U64 pageAddr = symbolValue & ~AARCH64_PAGE_MASK;

				if(!layout.stubCurrent || layout.stubCurrent + ADRP_PAGE_STUB_SIZE > layout.stubEnd)
				{
					Errors::fatalf("Ran out of stub space for separated ADRP stub");
				}
				U8* stub = layout.stubCurrent;
				layout.stubCurrent += ADRP_PAGE_STUB_SIZE;
				Uptr stubAddr = reinterpret_cast<Uptr>(stub);

				// [+0] LDR Xd, [PC, #8]: load page address from literal
				writeLE<U32>(stub + 0, U32(0x58000040) | rd);
				// [+4] B (fixupAddr + 4): return to next instruction
				I64 returnDelta = wrap<I64>(fixupAddr + 4) - wrap<I64>(stubAddr + 4);
				writeLE<U32>(stub + 4, 0x14000000 | (wrap<U32>(returnDelta >> 2) & 0x3FFFFFF));
				// [+8] .quad pageAddr
				writeLE<U64>(stub + 8, pageAddr);

				// Replace ADRP with B to stub.
				I64 branchDelta = wrap<I64>(stubAddr) - wrap<I64>(fixupAddr);
				writeLE<U32>(fixup, 0x14000000 | (wrap<U32>(branchDelta >> 2) & 0x3FFFFFF));

				return 1;
			}
		}
		relocAdrp(fixup, symbolValue, 0, fixupAddr);
		return 1;
	}

	case IMAGE_REL_ARM64_PAGEOFFSET_12A: {
		// If the ADRP handler already transformed the ADD to an LDR
		// (for out-of-range targets), the instruction was fully patched.
		// Detect this by checking the instruction opcode.
		U32 insn = readLE<U32>(fixup);
		if((insn & 0xFFC00000) == 0xF9400000)
		{
			// Already an LDR; the ADRP handler wrote it.
			break;
		}
		// Extract addend from ADD instruction's imm12 field.
		// COFF ARM64 encodes section-relative offsets in the instruction.
		I64 addImm12 = (insn >> 10) & 0xFFF;
		relocAddLo12(fixup, symbolValue, addImm12);
		break;
	}

	case IMAGE_REL_ARM64_PAGEOFFSET_12L: {
		// Extract addend from LDR/STR instruction's imm12 field.
		// COFF ARM64 encodes section-relative offsets in the instruction.
		U32 ldstInsn = readLE<U32>(fixup);
		U32 ldstSize = (ldstInsn >> 30) & 3;
		U32 ldstOpc = (ldstInsn >> 22) & 3;
		U32 shift = ldstSize;
		if(ldstSize == 0 && ldstOpc >= 2) { shift = 4; } // 128-bit
		I64 ldstAddend = I64(((ldstInsn >> 10) & 0xFFF) << shift);
		relocLdstLo12(fixup, symbolValue, ldstAddend);
		break;
	}

	case IMAGE_REL_ARM64_SECREL: {
		Uptr symSectionBase = 0;
		if(sym.SectionNumber > 0)
		{
			symSectionBase = layout.sectionLoadAddresses[wrap<U16>(sym.SectionNumber - 1)];
		}
		I32 addend = readLE<I32>(fixup);
		relocSecRel32(fixup, symbolValue, addend, symSectionBase);
		break;
	}

	case IMAGE_REL_ARM64_SECTION: {
		writeLE<U16>(fixup, wrap<U16>(sym.SectionNumber));
		break;
	}

	case IMAGE_REL_ARM64_SECREL_LOW12A: {
		Uptr symSectionBase = 0;
		if(sym.SectionNumber > 0)
		{
			symSectionBase = layout.sectionLoadAddresses[wrap<U16>(sym.SectionNumber - 1)];
		}
		U32 offset = wrap<U32>(symbolValue - symSectionBase) & 0xFFF;
		U32 insn = readLE<U32>(fixup);
		insn = (insn & 0xFFC003FF) | (offset << 10);
		writeLE<U32>(fixup, insn);
		break;
	}

	case IMAGE_REL_ARM64_SECREL_HIGH12A: {
		Uptr symSectionBase = 0;
		if(sym.SectionNumber > 0)
		{
			symSectionBase = layout.sectionLoadAddresses[wrap<U16>(sym.SectionNumber - 1)];
		}
		U32 offset = (wrap<U32>(symbolValue - symSectionBase) >> 12) & 0xFFF;
		U32 insn = readLE<U32>(fixup);
		insn = (insn & 0xFFC003FF) | (offset << 10);
		writeLE<U32>(fixup, insn);
		break;
	}

	case IMAGE_REL_ARM64_SECREL_LOW12L: {
		Uptr symSectionBase = 0;
		if(sym.SectionNumber > 0)
		{
			symSectionBase = layout.sectionLoadAddresses[wrap<U16>(sym.SectionNumber - 1)];
		}
		U32 secrelOffset = wrap<U32>(symbolValue - symSectionBase);
		U32 insn = readLE<U32>(fixup);
		U32 sizeField = (insn >> 30) & 3;
		U32 imm12 = (secrelOffset & 0xFFF) >> sizeField;
		insn = (insn & 0xFFC003FF) | (imm12 << 10);
		writeLE<U32>(fixup, insn);
		break;
	}

	default: Errors::fatalf("Unsupported COFF AArch64 relocation type: %u", rel.Type);
	}
	return 1;
}

static U32 applyCOFFReloc(Arch arch,
						  const CoffRelocation& rel,
						  U8* fixup,
						  Uptr fixupAddr,
						  U64 symbolValue,
						  const CoffSymbol& sym,
						  const std::string& symbolName,
						  ImageLayout& layout,
						  const CoffRelocation* nextReloc)
{
	switch(arch)
	{
	case Arch::x86_64:
		applyCOFFX64Reloc(rel, fixup, fixupAddr, symbolValue, sym, symbolName, layout);
		return 1;
	case Arch::aarch64:
		return applyCOFFAArch64Reloc(
			rel, fixup, fixupAddr, symbolValue, sym, symbolName, layout, nextReloc);
	default: WAVM_UNREACHABLE();
	}
}

template<Arch arch>
static void linkCOFFArch(U8* bytes,
						 Uptr size,
						 const HashMap<std::string, Uptr>& imports,
						 LinkResult& result,
						 PageAllocator allocatePages)
{
	const CoffFileHeader* header = reinterpret_cast<const CoffFileHeader*>(bytes);
	const CoffSectionHeader* sections = reinterpret_cast<const CoffSectionHeader*>(
		bytes + sizeof(CoffFileHeader) + header->SizeOfOptionalHeader);

	// Get symbol and string tables.
	const CoffSymbol* symbolTable
		= reinterpret_cast<const CoffSymbol*>(bytes + header->PointerToSymbolTable);
	// String table is right after the symbol table.
	const U8* stringTableBase
		= bytes + header->PointerToSymbolTable + header->NumberOfSymbols * sizeof(CoffSymbol);
	U32 stringTableSize = readLE<U32>(stringTableBase);
	const char* stringTable = reinterpret_cast<const char*>(stringTableBase);

	// 1. Collect section layout info.
	std::vector<SectionLayoutInfo> layoutInfos;
	std::vector<std::string> sectionNames;
	for(U16 i = 0; i < header->NumberOfSections; ++i)
	{
		const CoffSectionHeader& sect = sections[i];
		SectionLayoutInfo info;
		info.dataSize = sect.SizeOfRawData;
		info.alignment = getCOFFSectionAlignment(sect.Characteristics);
		info.access = getCOFFSectionAccess(sect.Characteristics);
		layoutInfos.push_back(info);

		// Resolve section name: if Name starts with '/', the remainder is
		// a decimal string table offset (for names longer than 8 chars).
		if(sect.Name[0] == '/')
		{
			char offsetStr[sizeof(sect.Name)] = {0};
			memcpy(offsetStr, sect.Name + 1, sizeof(sect.Name) - 1);
			U32 offset = U32(strtoul(offsetStr, nullptr, 10));
			if(offset < stringTableSize) { sectionNames.emplace_back(stringTable + offset); }
			else
			{
				sectionNames.emplace_back("");
			}
		}
		else
		{
			char name[sizeof(sect.Name) + 1] = {0};
			memcpy(name, sect.Name, sizeof(sect.Name));
			sectionNames.emplace_back(name);
		}
	}

	// 2. Count external symbols for GOT slots, and scan relocations to count
	// stub space needed: unique targets of stub-creating reloc types (cached, 16 bytes each)
	// plus per-site data-access stubs for AArch64 ADRP+LDR/STR pairs (uncached, 20 bytes each).
	Uptr numExternalSymbols = 0;
	HashSet<U32> stubSymsSeen;
	for(U32 i = 0; i < header->NumberOfSymbols; i += 1 + symbolTable[i].NumberOfAuxSymbols)
	{
		if(symbolTable[i].StorageClass == IMAGE_SYM_CLASS_EXTERNAL
		   && symbolTable[i].SectionNumber == 0)
		{
			++numExternalSymbols;
			// __CxxFrameHandler3 gets a stub during symbol resolution (not via relocations)
			// so it must be included in the census.
			std::string name = getCOFFSymbolName(symbolTable[i], stringTable, stringTableSize);
			if(name == "__CxxFrameHandler3") { stubSymsSeen.add(i); }
		}
	}
	Uptr numDataStubSites = 0;
	Uptr numSeparatedAdrpSites = 0;
	for(U16 si = 0; si < header->NumberOfSections; ++si)
	{
		const CoffSectionHeader& sect = sections[si];
		if(sect.NumberOfRelocations == 0) { continue; }
		UnalignedArrayView<CoffRelocation> relocs{bytes + sect.PointerToRelocations,
												  sect.NumberOfRelocations};
		for(U32 ri = 0; ri < sect.NumberOfRelocations; ++ri)
		{
			CoffRelocation rel = relocs[ri];
			if constexpr(arch == Arch::x86_64)
			{
				if(rel.Type >= IMAGE_REL_AMD64_REL32 && rel.Type <= IMAGE_REL_AMD64_REL32_5)
				{
					stubSymsSeen.add(rel.SymbolTableIndex);
				}
			}
			else
			{
				if(rel.Type == IMAGE_REL_ARM64_BRANCH26) { stubSymsSeen.add(rel.SymbolTableIndex); }
				else if(rel.Type == IMAGE_REL_ARM64_PAGEBASE_REL21)
				{
					bool hasAdjacentPageOffset
						= ri + 1 < sect.NumberOfRelocations
						  && relocs[ri + 1].VirtualAddress == rel.VirtualAddress + 4;
					if(hasAdjacentPageOffset
					   && relocs[ri + 1].Type == IMAGE_REL_ARM64_PAGEOFFSET_12L)
					{
						++numDataStubSites;
					}
					else if(!hasAdjacentPageOffset
							|| (relocs[ri + 1].Type != IMAGE_REL_ARM64_PAGEOFFSET_12A
								&& relocs[ri + 1].Type != IMAGE_REL_ARM64_PAGEOFFSET_12L))
					{
						// ADRP whose paired PAGEOFFSET is not adjacent (scheduler
						// separated them). May need a 16-byte literal-pool stub.
						++numSeparatedAdrpSites;
					}
				}
			}
		}
	}

	// 3. Layout image. On AArch64, GOT slots are needed for symbols whose addresses
	// are out of ADRP range (+/-4GB). Any external symbol could be out of range,
	// so allocate slots for all of them as an upper bound.
	Uptr numStubBytes = stubSymsSeen.size() * stubSlotSize + numDataStubSites * ADRP_DATA_STUB_SIZE
						+ numSeparatedAdrpSites * ADRP_PAGE_STUB_SIZE;
	Uptr numGotBytes = arch == Arch::aarch64 ? numExternalSymbols * 8 : 0;
	ImageLayout layout = layoutImage(layoutInfos, numStubBytes, numGotBytes, 0, allocatePages);

	// 4. Copy section data.
	for(U16 i = 0; i < header->NumberOfSections; ++i)
	{
		const CoffSectionHeader& sect = sections[i];
		if(sect.SizeOfRawData > 0 && sect.PointerToRawData > 0)
		{
			memcpy(reinterpret_cast<U8*>(layout.sectionLoadAddresses[i]),
				   bytes + sect.PointerToRawData,
				   sect.SizeOfRawData);
		}
	}

	// 5. Build symbol addresses.
	std::vector<Uptr> symbolAddresses(header->NumberOfSymbols, 0);

	// Create a trampoline for __CxxFrameHandler3 if needed.
	Uptr cxxFrameHandlerStub = 0;

	for(U32 i = 0; i < header->NumberOfSymbols; i += 1 + symbolTable[i].NumberOfAuxSymbols)
	{
		const CoffSymbol& sym = symbolTable[i];
		std::string name = getCOFFSymbolName(sym, stringTable, stringTableSize);

		if(sym.SectionNumber > 0)
		{
			// Defined symbol: section number is 1-based.
			U16 sectIdx = wrap<U16>(sym.SectionNumber - 1);
			if(sectIdx < header->NumberOfSections)
			{
				symbolAddresses[i] = layout.sectionLoadAddresses[sectIdx] + sym.Value;
			}
		}
		else if(sym.SectionNumber == 0 && sym.StorageClass == IMAGE_SYM_CLASS_EXTERNAL)
		{
			// External (undefined) symbol.
			if(name == "__ImageBase")
			{
				// __ImageBase resolves to the image base address.
				symbolAddresses[i] = reinterpret_cast<Uptr>(layout.imageBase);
			}
			else if(name == "__CxxFrameHandler3")
			{
				// Create a trampoline stub within the image for __CxxFrameHandler3.
				// SEH tables use 32-bit image-relative addresses, so the handler
				// must be within 32-bit offset of imageBase.
				const Uptr* addr = imports.get(name);
				if(!addr) { Errors::fatalf("Unresolved COFF symbol: %s", name.c_str()); }
				cxxFrameHandlerStub = getOrCreateStub<arch>(layout, *addr);
				symbolAddresses[i] = cxxFrameHandlerStub;
			}
			else
			{
				const Uptr* addr = imports.get(name);
				if(!addr) { Errors::fatalf("Unresolved COFF symbol: %s", name.c_str()); }
				symbolAddresses[i] = *addr;
			}
		}
		else if(sym.SectionNumber == -1)
		{
			// Absolute symbol.
			symbolAddresses[i] = Uptr(sym.Value);
		}

		Log::printf(Log::traceLinking,
					"  COFF sym [%u]: '%s' sect=%d value=0x%x type=0x%04x class=%u"
					" addr=0x%" WAVM_PRIxPTR "\n",
					i,
					name.c_str(),
					I32(sym.SectionNumber),
					sym.Value,
					U32(sym.Type),
					U32(sym.StorageClass),
					symbolAddresses[i]);
	}

	// 6. Apply relocations per section.
	for(U16 si = 0; si < header->NumberOfSections; ++si)
	{
		const CoffSectionHeader& sect = sections[si];
		if(sect.NumberOfRelocations == 0) { continue; }

		Uptr sectionAddr = layout.sectionLoadAddresses[si];
		const CoffRelocation* relocs
			= reinterpret_cast<const CoffRelocation*>(bytes + sect.PointerToRelocations);

		for(U32 ri = 0; ri < sect.NumberOfRelocations;)
		{
			const CoffRelocation& rel = relocs[ri];
			U8* fixup = reinterpret_cast<U8*>(sectionAddr + rel.VirtualAddress);
			Uptr fixupAddr = sectionAddr + rel.VirtualAddress;

			const CoffSymbol& sym = symbolTable[rel.SymbolTableIndex];
			std::string symbolName = getCOFFSymbolName(sym, stringTable, stringTableSize);
			U64 symbolValue = symbolAddresses[rel.SymbolTableIndex];

			const CoffRelocation* nextReloc
				= (ri + 1 < sect.NumberOfRelocations) ? &relocs[ri + 1] : nullptr;

			ri += applyCOFFReloc(
				arch, rel, fixup, fixupAddr, symbolValue, sym, symbolName, layout, nextReloc);
		}
	}

	// 7. Protect image, fill result.
	protectImage(layout);

	result.imageBase = layout.imageBase;
	result.numImagePages = layout.numPages;
	result.numCodeBytes = layout.codeSize;
	result.numReadOnlyBytes = layout.roSize;
	result.numReadWriteBytes = layout.rwSize;

	// Record section addresses and find unwind/DWARF sections.
	Uptr pdataAddr = 0;
	Uptr pdataSize = 0;
	for(U16 i = 0; i < header->NumberOfSections; ++i)
	{
		Uptr addr = layout.sectionLoadAddresses[i];
		Log::printf(Log::traceLinking,
					"  COFF section [%u]: '%s' addr=0x%" WAVM_PRIxPTR " size=%u\n",
					U32(i),
					sectionNames[i].c_str(),
					addr,
					sections[i].SizeOfRawData);

		if(sectionNames[i] == ".pdata")
		{
			result.unwindInfo.data = reinterpret_cast<const U8*>(addr);
			result.unwindInfo.dataNumBytes = sections[i].SizeOfRawData;
			pdataAddr = addr;
			pdataSize = sections[i].SizeOfRawData;
		}
		else if(sectionNames[i] == ".debug_info" && addr)
		{
			result.dwarf.debugInfo = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugInfoSize = Uptr(sections[i].SizeOfRawData);
		}
		else if(sectionNames[i] == ".debug_abbrev" && addr)
		{
			result.dwarf.debugAbbrev = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugAbbrevSize = Uptr(sections[i].SizeOfRawData);
		}
		else if(sectionNames[i] == ".debug_str" && addr)
		{
			result.dwarf.debugStr = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugStrSize = Uptr(sections[i].SizeOfRawData);
		}
		else if(sectionNames[i] == ".debug_line" && addr)
		{
			result.dwarf.debugLine = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugLineSize = Uptr(sections[i].SizeOfRawData);
		}
		else if(sectionNames[i] == ".debug_addr" && addr)
		{
			result.dwarf.debugAddr = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugAddrSize = Uptr(sections[i].SizeOfRawData);
		}
		else if(sectionNames[i] == ".debug_str_offsets" && addr)
		{
			result.dwarf.debugStrOffsets = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugStrOffsetsSize = Uptr(sections[i].SizeOfRawData);
		}
	}

	// Collect defined function symbols.
	for(U32 i = 0; i < header->NumberOfSymbols; i += 1 + symbolTable[i].NumberOfAuxSymbols)
	{
		const CoffSymbol& sym = symbolTable[i];
		if(sym.SectionNumber <= 0) { continue; }
		if(sym.StorageClass != IMAGE_SYM_CLASS_EXTERNAL) { continue; }

		std::string name = getCOFFSymbolName(sym, stringTable, stringTableSize);
		if(name.empty()) { continue; }

		// COFF function symbols have type 0x20.
		if((sym.Type & 0xFF) != 0x20) { continue; }

		LinkResult::Symbol resultSym;
		resultSym.name = name;
		resultSym.address = symbolAddresses[i];
		resultSym.size = 0; // COFF doesn't store function sizes. Will compute below.
		result.definedSymbols.push_back(std::move(resultSym));
	}

	// Compute function sizes from .pdata.
	if(pdataAddr && pdataSize)
	{
		switch(arch)
		{
		case Arch::x86_64: {
			// x86-64: 12-byte entries (BeginAddress, EndAddress, UnwindInfoAddress).
			Uptr numEntries = pdataSize / 12;
			for(auto& sym : result.definedSymbols)
			{
				for(Uptr e = 0; e < numEntries; ++e)
				{
					const U8* entry = reinterpret_cast<const U8*>(pdataAddr + e * 12);
					U32 beginRVA = readLE<U32>(entry);
					U32 endRVA = readLE<U32>(entry + 4);
					Uptr beginAddr = Uptr(layout.imageBase) + beginRVA;
					if(sym.address == beginAddr)
					{
						sym.size = Uptr(layout.imageBase) + endRVA - beginAddr;
					}
				}
			}
			break;
		}
		case Arch::aarch64: {
			// ARM64: 8-byte entries (BeginAddress, UnwindData).
			Uptr numEntries = pdataSize / 8;
			for(auto& sym : result.definedSymbols)
			{
				for(Uptr e = 0; e < numEntries; ++e)
				{
					const U8* entry = reinterpret_cast<const U8*>(pdataAddr + e * 8);
					U32 beginRVA = readLE<U32>(entry);
					U32 unwindData = readLE<U32>(entry + 4);
					Uptr beginAddr = Uptr(layout.imageBase) + beginRVA;
					if(sym.address != beginAddr) { continue; }

					U32 flag = unwindData & 3;
					if(flag != 0)
					{
						// Packed unwind data: FunctionLength in bits [12:2],
						// in 4-byte instruction units.
						sym.size = Uptr(((unwindData >> 2) & 0x7FF) * 4);
					}
					else
					{
						// Unpacked: UnwindData is an RVA to .xdata.
						// .xdata header bits [17:0] = FunctionLength in 4-byte units.
						const U8* xdata = layout.imageBase + unwindData;
						U32 xdataHeader = readLE<U32>(xdata);
						sym.size = Uptr((xdataHeader & 0x3FFFF) * 4);
					}
				}
			}
			break;
		}
		default: WAVM_UNREACHABLE();
		}
	}
}

void ObjectLinker::linkCOFF(U8* bytes,
							Uptr size,
							const HashMap<std::string, Uptr>& imports,
							LinkResult& result,
							PageAllocator allocatePages)
{
	if(size < sizeof(CoffFileHeader)) { Errors::fatalf("COFF file too small"); }

	const CoffFileHeader* header = reinterpret_cast<const CoffFileHeader*>(bytes);

	const char* machName = "?";
	switch(header->Machine)
	{
	case IMAGE_FILE_MACHINE_AMD64: machName = "x86-64"; break;
	case IMAGE_FILE_MACHINE_ARM64: machName = "AArch64"; break;
	default: break;
	}
	Log::printf(Log::traceLinking,
				"--- linkCOFF: %" WAVM_PRIuPTR " bytes, machine=%s ---\n",
				size,
				machName);

	switch(header->Machine)
	{
	case IMAGE_FILE_MACHINE_AMD64:
		linkCOFFArch<Arch::x86_64>(bytes, size, imports, result, allocatePages);
		break;
	case IMAGE_FILE_MACHINE_ARM64:
		linkCOFFArch<Arch::aarch64>(bytes, size, imports, result, allocatePages);
		break;
	default: Errors::fatalf("Unsupported COFF machine type: 0x%x", header->Machine);
	}
}
