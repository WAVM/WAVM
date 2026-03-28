#include <cinttypes>
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
#include "WAVM/Logging/Logging.h"
#include "WAVM/ObjectLinker/ObjectLinker.h"

using namespace WAVM;
using namespace WAVM::ObjectLinker;

// ELF header structures (64-bit).

struct Elf64_Ehdr
{
	U8 e_ident[16];
	U16 e_type;
	U16 e_machine;
	U32 e_version;
	U64 e_entry;
	U64 e_phoff;
	U64 e_shoff;
	U32 e_flags;
	U16 e_ehsize;
	U16 e_phentsize;
	U16 e_phnum;
	U16 e_shentsize;
	U16 e_shnum;
	U16 e_shstrndx;
};

struct Elf64_Shdr
{
	U32 sh_name;
	U32 sh_type;
	U64 sh_flags;
	U64 sh_addr;
	U64 sh_offset;
	U64 sh_size;
	U32 sh_link;
	U32 sh_info;
	U64 sh_addralign;
	U64 sh_entsize;
};

struct Elf64_Sym
{
	U32 st_name;
	U8 st_info;
	U8 st_other;
	U16 st_shndx;
	U64 st_value;
	U64 st_size;
};

struct Elf64_Rela
{
	U64 r_offset;
	U64 r_info;
	I64 r_addend;
};

struct Elf64_Rel
{
	U64 r_offset;
	U64 r_info;
};

// ELF constants.
static constexpr U32 SHT_SYMTAB = 2;
static constexpr U32 SHT_STRTAB = 3;
static constexpr U32 SHT_RELA = 4;
static constexpr U32 SHT_REL = 9;
static constexpr U32 SHT_NULL = 0;

static constexpr U64 SHF_WRITE = 0x1;
static constexpr U64 SHF_ALLOC = 0x2;
static constexpr U64 SHF_EXECINSTR = 0x4;

static constexpr U16 SHN_UNDEF = 0;
static constexpr U16 SHN_ABS = 0xFFF1;

static constexpr U16 EM_X86_64 = 62;
static constexpr U16 EM_AARCH64 = 183;

// ELF symbol binding/type.
#define ELF64_ST_BIND(info) ((info) >> 4)
#define ELF64_ST_TYPE(info) ((info) & 0xF)
static constexpr U8 STB_GLOBAL = 1;
static constexpr U8 STT_FUNC = 2;
// ELF relocation info.
#define ELF64_R_SYM(info) ((info) >> 32)
#define ELF64_R_TYPE(info) ((U32)(info))

// x86-64 ELF relocation types.
static constexpr U32 R_X86_64_64 = 1;
static constexpr U32 R_X86_64_PC32 = 2;
static constexpr U32 R_X86_64_PLT32 = 4;
static constexpr U32 R_X86_64_32 = 10;
static constexpr U32 R_X86_64_32S = 11;
static constexpr U32 R_X86_64_GOTPCREL = 9;
static constexpr U32 R_X86_64_PC64 = 24;
static constexpr U32 R_X86_64_GOTPCRELX = 41;
static constexpr U32 R_X86_64_REX_GOTPCRELX = 42;

// AArch64 ELF relocation types.
static constexpr U32 R_AARCH64_ABS64 = 257;
static constexpr U32 R_AARCH64_ABS32 = 258;
static constexpr U32 R_AARCH64_PREL64 = 260;
static constexpr U32 R_AARCH64_PREL32 = 261;
static constexpr U32 R_AARCH64_CALL26 = 283;
static constexpr U32 R_AARCH64_JUMP26 = 282;
static constexpr U32 R_AARCH64_ADR_PREL_PG_HI21 = 275;
static constexpr U32 R_AARCH64_ADD_ABS_LO12_NC = 277;
static constexpr U32 R_AARCH64_LDST8_ABS_LO12_NC = 278;
static constexpr U32 R_AARCH64_LDST16_ABS_LO12_NC = 284;
static constexpr U32 R_AARCH64_LDST32_ABS_LO12_NC = 285;
static constexpr U32 R_AARCH64_LDST64_ABS_LO12_NC = 286;
static constexpr U32 R_AARCH64_LDST128_ABS_LO12_NC = 299;
static constexpr U32 R_AARCH64_MOVW_UABS_G0 = 263;
static constexpr U32 R_AARCH64_MOVW_UABS_G0_NC = 264;
static constexpr U32 R_AARCH64_MOVW_UABS_G1 = 265;
static constexpr U32 R_AARCH64_MOVW_UABS_G1_NC = 266;
static constexpr U32 R_AARCH64_MOVW_UABS_G2 = 267;
static constexpr U32 R_AARCH64_MOVW_UABS_G2_NC = 268;
static constexpr U32 R_AARCH64_MOVW_UABS_G3 = 269;
static constexpr U32 R_AARCH64_ADR_GOT_PAGE = 311;
static constexpr U32 R_AARCH64_LD64_GOT_LO12_NC = 312;

static const char* getELFX64RelocName(U32 type)
{
	switch(type)
	{
	case R_X86_64_64: return "R_X86_64_64";
	case R_X86_64_PC32: return "R_X86_64_PC32";
	case R_X86_64_PLT32: return "R_X86_64_PLT32";
	case R_X86_64_32: return "R_X86_64_32";
	case R_X86_64_32S: return "R_X86_64_32S";
	case R_X86_64_PC64: return "R_X86_64_PC64";
	case R_X86_64_GOTPCREL: return "R_X86_64_GOTPCREL";
	case R_X86_64_GOTPCRELX: return "R_X86_64_GOTPCRELX";
	case R_X86_64_REX_GOTPCRELX: return "R_X86_64_REX_GOTPCRELX";
	default: return "?";
	}
}

static const char* getELFAArch64RelocName(U32 type)
{
	switch(type)
	{
	case R_AARCH64_ABS64: return "R_AARCH64_ABS64";
	case R_AARCH64_ABS32: return "R_AARCH64_ABS32";
	case R_AARCH64_PREL64: return "R_AARCH64_PREL64";
	case R_AARCH64_PREL32: return "R_AARCH64_PREL32";
	case R_AARCH64_CALL26: return "R_AARCH64_CALL26";
	case R_AARCH64_JUMP26: return "R_AARCH64_JUMP26";
	case R_AARCH64_ADR_PREL_PG_HI21: return "R_AARCH64_ADR_PREL_PG_HI21";
	case R_AARCH64_ADD_ABS_LO12_NC: return "R_AARCH64_ADD_ABS_LO12_NC";
	case R_AARCH64_LDST8_ABS_LO12_NC: return "R_AARCH64_LDST8_ABS_LO12_NC";
	case R_AARCH64_LDST16_ABS_LO12_NC: return "R_AARCH64_LDST16_ABS_LO12_NC";
	case R_AARCH64_LDST32_ABS_LO12_NC: return "R_AARCH64_LDST32_ABS_LO12_NC";
	case R_AARCH64_LDST64_ABS_LO12_NC: return "R_AARCH64_LDST64_ABS_LO12_NC";
	case R_AARCH64_LDST128_ABS_LO12_NC: return "R_AARCH64_LDST128_ABS_LO12_NC";
	case R_AARCH64_MOVW_UABS_G0: return "R_AARCH64_MOVW_UABS_G0";
	case R_AARCH64_MOVW_UABS_G0_NC: return "R_AARCH64_MOVW_UABS_G0_NC";
	case R_AARCH64_MOVW_UABS_G1: return "R_AARCH64_MOVW_UABS_G1";
	case R_AARCH64_MOVW_UABS_G1_NC: return "R_AARCH64_MOVW_UABS_G1_NC";
	case R_AARCH64_MOVW_UABS_G2: return "R_AARCH64_MOVW_UABS_G2";
	case R_AARCH64_MOVW_UABS_G2_NC: return "R_AARCH64_MOVW_UABS_G2_NC";
	case R_AARCH64_MOVW_UABS_G3: return "R_AARCH64_MOVW_UABS_G3";
	case R_AARCH64_ADR_GOT_PAGE: return "R_AARCH64_ADR_GOT_PAGE";
	case R_AARCH64_LD64_GOT_LO12_NC: return "R_AARCH64_LD64_GOT_LO12_NC";
	default: return "?";
	}
}

static SectionAccess getELFSectionAccess(U64 flags)
{
	if(flags & SHF_EXECINSTR) { return SectionAccess::readExecute; }
	if(flags & SHF_WRITE) { return SectionAccess::readWrite; }
	return SectionAccess::readOnly;
}

static void applyELFX64Reloc(U32 rtype,
							 U8* fixup,
							 Uptr fixupAddr,
							 U64 symbolValue,
							 I64 addend,
							 Uptr symIdx,
							 ImageLayout& layout)
{
	Log::printf(Log::traceLinking,
				"  reloc @0x%" WAVM_PRIxPTR " type=%s(%u) sym=%" WAVM_PRIuPTR " symval=0x%" PRIx64
				" addend=%" PRId64 "\n",
				fixupAddr,
				getELFX64RelocName(rtype),
				rtype,
				symIdx,
				symbolValue,
				addend);
	switch(rtype)
	{
	case R_X86_64_64: relocAbs64(fixup, symbolValue, addend); break;
	case R_X86_64_32: relocAbs32(fixup, symbolValue, addend); break;
	case R_X86_64_32S: relocAbs32s(fixup, symbolValue, addend); break;
	case R_X86_64_PC64: relocRel64(fixup, symbolValue, addend, fixupAddr); break;

	case R_X86_64_PC32:
	case R_X86_64_PLT32: relocRel32(fixup, symbolValue, addend, fixupAddr, layout); break;

	case R_X86_64_GOTPCREL:
	case R_X86_64_GOTPCRELX:
	case R_X86_64_REX_GOTPCRELX: {
		Uptr gotEntry = getOrCreateGotEntry(layout, symbolValue);
		relocRel32(fixup, gotEntry, addend, fixupAddr, layout);
		break;
	}

	default: Errors::fatalf("Unsupported ELF x86-64 relocation type: %u", rtype);
	}
}

static void applyELFAArch64Reloc(U32 rtype,
								 U8* fixup,
								 Uptr fixupAddr,
								 U64 symbolValue,
								 I64 addend,
								 Uptr symIdx,
								 ImageLayout& layout)
{
	Log::printf(Log::traceLinking,
				"  reloc @0x%" WAVM_PRIxPTR " type=%s(%u) sym=%" WAVM_PRIuPTR " symval=0x%" PRIx64
				" addend=%" PRId64 "\n",
				fixupAddr,
				getELFAArch64RelocName(rtype),
				rtype,
				symIdx,
				symbolValue,
				addend);
	switch(rtype)
	{
	case R_AARCH64_ABS64: relocAbs64(fixup, symbolValue, addend); break;
	case R_AARCH64_ABS32: relocAbs32(fixup, symbolValue, addend); break;

	case R_AARCH64_PREL64: relocRel64(fixup, symbolValue, addend, fixupAddr); break;
	case R_AARCH64_PREL32: relocPrel32(fixup, symbolValue, addend, fixupAddr); break;

	case R_AARCH64_CALL26:
	case R_AARCH64_JUMP26: relocBranch26(fixup, symbolValue, addend, fixupAddr, layout); break;

	case R_AARCH64_ADR_PREL_PG_HI21: relocAdrp(fixup, symbolValue, addend, fixupAddr); break;

	case R_AARCH64_ADD_ABS_LO12_NC: relocAddLo12(fixup, symbolValue, addend); break;

	case R_AARCH64_LDST8_ABS_LO12_NC:
	case R_AARCH64_LDST16_ABS_LO12_NC:
	case R_AARCH64_LDST32_ABS_LO12_NC:
	case R_AARCH64_LDST64_ABS_LO12_NC:
	case R_AARCH64_LDST128_ABS_LO12_NC: relocLdstLo12(fixup, symbolValue, addend); break;

	case R_AARCH64_MOVW_UABS_G0:
	case R_AARCH64_MOVW_UABS_G0_NC: relocMovwUabs(fixup, symbolValue, addend, 0); break;
	case R_AARCH64_MOVW_UABS_G1:
	case R_AARCH64_MOVW_UABS_G1_NC: relocMovwUabs(fixup, symbolValue, addend, 16); break;
	case R_AARCH64_MOVW_UABS_G2:
	case R_AARCH64_MOVW_UABS_G2_NC: relocMovwUabs(fixup, symbolValue, addend, 32); break;
	case R_AARCH64_MOVW_UABS_G3: relocMovwUabs(fixup, symbolValue, addend, 48); break;

	case R_AARCH64_ADR_GOT_PAGE: {
		Uptr gotEntry = getOrCreateGotEntry(layout, symbolValue);
		relocAdrp(fixup, gotEntry, 0, fixupAddr);
		break;
	}

	case R_AARCH64_LD64_GOT_LO12_NC: {
		Uptr gotEntry = getOrCreateGotEntry(layout, symbolValue);
		relocLdstLo12(fixup, gotEntry, 0);
		break;
	}

	default: Errors::fatalf("Unsupported ELF AArch64 relocation type: %u", rtype);
	}
}

static void applyELFReloc(Arch arch,
						  U32 rtype,
						  U8* fixup,
						  Uptr fixupAddr,
						  U64 symbolValue,
						  I64 addend,
						  Uptr symIdx,
						  ImageLayout& layout)
{
	switch(arch)
	{
	case Arch::x86_64:
		applyELFX64Reloc(rtype, fixup, fixupAddr, symbolValue, addend, symIdx, layout);
		break;
	case Arch::aarch64:
		applyELFAArch64Reloc(rtype, fixup, fixupAddr, symbolValue, addend, symIdx, layout);
		break;
	default: WAVM_UNREACHABLE();
	}
}

static I64 readELFX64ImplicitAddend(U32 rtype, const U8* fixup)
{
	switch(rtype)
	{
	case R_X86_64_64:
	case R_X86_64_PC64: return readLE<I64>(fixup);
	case R_X86_64_PC32:
	case R_X86_64_PLT32:
	case R_X86_64_32:
	case R_X86_64_32S: return readLE<I32>(fixup);
	default: return 0;
	}
}

template<Arch arch>
static void linkELFArch(U8* bytes,
						Uptr size,
						const HashMap<std::string, Uptr>& imports,
						LinkResult& result,
						PageAllocator allocatePages)
{
	const Elf64_Ehdr* ehdr = reinterpret_cast<const Elf64_Ehdr*>(bytes);

	// Find section headers. e_shoff must be 8-byte aligned per the ELF spec.
	const Elf64_Shdr* shdrs = reinterpret_cast<const Elf64_Shdr*>(bytes + ehdr->e_shoff);

	// Find section name string table.
	const Elf64_Shdr& shstrtab = shdrs[ehdr->e_shstrndx];
	const char* shstrtabData = reinterpret_cast<const char*>(bytes + shstrtab.sh_offset);

	// Find symtab and strtab.
	const Elf64_Shdr* symtabShdr = nullptr;
	const Elf64_Shdr* strtabShdr = nullptr;
	for(U16 i = 0; i < ehdr->e_shnum; ++i)
	{
		if(shdrs[i].sh_type == SHT_SYMTAB)
		{
			symtabShdr = &shdrs[i];
			strtabShdr = &shdrs[symtabShdr->sh_link];
			break;
		}
	}
	if(!symtabShdr || !strtabShdr) { Errors::fatalf("ELF object has no symbol table"); }

	const Elf64_Sym* symbols = reinterpret_cast<const Elf64_Sym*>(bytes + symtabShdr->sh_offset);
	U32 numSymbols = narrow<U32>("ELF symbol count", symtabShdr->sh_size / sizeof(Elf64_Sym));
	const char* strtab = reinterpret_cast<const char*>(bytes + strtabShdr->sh_offset);

	// 1. Collect section layout info (skip null section and non-allocatable sections).
	struct SectionMapping
	{
		U16 elfIndex;
	};
	std::vector<SectionMapping> sectionMappings;
	std::vector<SectionLayoutInfo> layoutInfos;

	// Map from ELF section index to our layout index. -1 means not mapped.
	std::vector<Iptr> elfToLayoutIndex(ehdr->e_shnum, -1);

	// Track DWARF debug section indices so we can populate result.dwarf after layout.
	Iptr debugInfoElfIdx = -1;
	Iptr debugAbbrevElfIdx = -1;
	Iptr debugStrElfIdx = -1;
	Iptr debugLineElfIdx = -1;
	Iptr debugAddrElfIdx = -1;
	Iptr debugStrOffsetsElfIdx = -1;

	for(U16 i = 1; i < ehdr->e_shnum; ++i)
	{
		const Elf64_Shdr& shdr = shdrs[i];
		// Skip non-content sections.
		if(shdr.sh_type == SHT_NULL || shdr.sh_type == SHT_SYMTAB || shdr.sh_type == SHT_STRTAB
		   || shdr.sh_type == SHT_RELA || shdr.sh_type == SHT_REL)
		{
			continue;
		}
		// Load SHF_ALLOC sections as before. Additionally load DWARF debug sections
		// (.debug_info, .debug_abbrev, .debug_str) as read-only even without SHF_ALLOC.
		const char* sectionName = shstrtabData + shdr.sh_name;
		bool isDwarf
			= (strcmp(sectionName, ".debug_info") == 0 || strcmp(sectionName, ".debug_abbrev") == 0
			   || strcmp(sectionName, ".debug_str") == 0 || strcmp(sectionName, ".debug_line") == 0
			   || strcmp(sectionName, ".debug_addr") == 0
			   || strcmp(sectionName, ".debug_str_offsets") == 0);
		if(!(shdr.sh_flags & SHF_ALLOC) && !isDwarf) { continue; }
		// Skip sections with zero size.
		if(shdr.sh_size == 0) { continue; }

		SectionLayoutInfo info;
		info.dataSize = Uptr(shdr.sh_size);
		info.alignment = shdr.sh_addralign ? Uptr(shdr.sh_addralign) : 1;
		info.access = isDwarf ? SectionAccess::readOnly : getELFSectionAccess(shdr.sh_flags);

		// Track DWARF section indices for populating result.dwarf later.
		if(strcmp(sectionName, ".debug_info") == 0) { debugInfoElfIdx = Iptr(i); }
		else if(strcmp(sectionName, ".debug_abbrev") == 0) { debugAbbrevElfIdx = Iptr(i); }
		else if(strcmp(sectionName, ".debug_str") == 0) { debugStrElfIdx = Iptr(i); }
		else if(strcmp(sectionName, ".debug_line") == 0) { debugLineElfIdx = Iptr(i); }
		else if(strcmp(sectionName, ".debug_addr") == 0) { debugAddrElfIdx = Iptr(i); }
		else if(strcmp(sectionName, ".debug_str_offsets") == 0) { debugStrOffsetsElfIdx = Iptr(i); }

		elfToLayoutIndex[i] = Iptr(sectionMappings.size());
		sectionMappings.push_back({i});
		layoutInfos.push_back(info);
	}

	// 2. Count external symbols for GOT slots, and scan relocations to count
	// unique stub targets (symbols referenced by stub-creating relocation types).
	Uptr numExternalSymbols = 0;
	for(U32 i = 0; i < numSymbols; ++i)
	{
		if(symbols[i].st_shndx == SHN_UNDEF && ELF64_ST_BIND(symbols[i].st_info) >= STB_GLOBAL)
		{
			++numExternalSymbols;
		}
	}

	HashSet<U32> stubSymsSeen;
	for(U16 si = 0; si < ehdr->e_shnum; ++si)
	{
		const Elf64_Shdr& shdr = shdrs[si];
		if(shdr.sh_type == SHT_RELA)
		{
			const Elf64_Rela* relas = reinterpret_cast<const Elf64_Rela*>(bytes + shdr.sh_offset);
			Uptr numRelas = shdr.sh_size / sizeof(Elf64_Rela);
			for(Uptr ri = 0; ri < numRelas; ++ri)
			{
				U32 rtype = ELF64_R_TYPE(relas[ri].r_info);
				bool needsStub;
				if constexpr(arch == Arch::x86_64)
				{
					needsStub = rtype == R_X86_64_PC32 || rtype == R_X86_64_PLT32;
				}
				else
				{
					needsStub = rtype == R_AARCH64_CALL26 || rtype == R_AARCH64_JUMP26;
				}
				if(needsStub) { stubSymsSeen.add(U32(ELF64_R_SYM(relas[ri].r_info))); }
			}
		}
		else if(shdr.sh_type == SHT_REL)
		{
			const Elf64_Rel* rels = reinterpret_cast<const Elf64_Rel*>(bytes + shdr.sh_offset);
			Uptr numRels = shdr.sh_size / sizeof(Elf64_Rel);
			for(Uptr ri = 0; ri < numRels; ++ri)
			{
				U32 rtype = ELF64_R_TYPE(rels[ri].r_info);
				bool needsStub;
				if constexpr(arch == Arch::x86_64)
				{
					needsStub = rtype == R_X86_64_PC32 || rtype == R_X86_64_PLT32;
				}
				else
				{
					needsStub = rtype == R_AARCH64_CALL26 || rtype == R_AARCH64_JUMP26;
				}
				if(needsStub) { stubSymsSeen.add(U32(ELF64_R_SYM(rels[ri].r_info))); }
			}
		}
	}

	// 3. Layout image.
	ImageLayout layout = layoutImage(
		layoutInfos, stubSymsSeen.size() * stubSlotSize, numExternalSymbols * 8, 0, allocatePages);

	// 4. Copy section data.
	for(Uptr i = 0; i < sectionMappings.size(); ++i)
	{
		const Elf64_Shdr& shdr = shdrs[sectionMappings[i].elfIndex];
		if(shdr.sh_size > 0 && shdr.sh_offset > 0)
		{
			// SHT_NOBITS sections (like .bss) have no file data.
			if(shdr.sh_type != 8) // SHT_NOBITS = 8
			{
				memcpy(reinterpret_cast<U8*>(layout.sectionLoadAddresses[i]),
					   bytes + shdr.sh_offset,
					   Uptr(shdr.sh_size));
			}
		}
	}

	// 5. Build symbol addresses.
	std::vector<Uptr> symbolAddresses(numSymbols, 0);
	for(U32 i = 0; i < numSymbols; ++i)
	{
		const Elf64_Sym& sym = symbols[i];
		if(sym.st_shndx == SHN_UNDEF)
		{
			if(i == 0) { continue; } // Null symbol
			const char* name = strtab + sym.st_name;
			const Uptr* addr = imports.get(std::string(name));
			if(!addr) { Errors::fatalf("Unresolved ELF symbol: %s", name); }
			symbolAddresses[i] = *addr;
		}
		else if(sym.st_shndx == SHN_ABS) { symbolAddresses[i] = Uptr(sym.st_value); }
		else if(sym.st_shndx < ehdr->e_shnum)
		{
			Iptr layoutIdx = elfToLayoutIndex[sym.st_shndx];
			if(layoutIdx >= 0 && (shdrs[sym.st_shndx].sh_flags & SHF_ALLOC))
			{
				symbolAddresses[i]
					= layout.sectionLoadAddresses[Uptr(layoutIdx)] + Uptr(sym.st_value);
			}
			// Non-ALLOC section symbols (debug sections) stay at 0, matching the sh_addr=0
			// convention used by static linkers. This ensures 32-bit relocations within
			// debug sections produce section-relative offsets that fit in U32.
		}

		const char* symName = strtab + sym.st_name;
		Log::printf(Log::traceLinking,
					"  ELF sym [%u]: '%s' shndx=%u value=0x%" PRIx64
					" info=0x%02x addr=0x%" WAVM_PRIxPTR "\n",
					i,
					symName,
					U32(sym.st_shndx),
					sym.st_value,
					U32(sym.st_info),
					symbolAddresses[i]);
	}

	// 6. Apply relocations.
	for(U16 si = 0; si < ehdr->e_shnum; ++si)
	{
		const Elf64_Shdr& shdr = shdrs[si];
		if(shdr.sh_type != SHT_RELA && shdr.sh_type != SHT_REL) { continue; }

		// sh_info is the section index that relocations apply to.
		U32 targetSectIdx = shdr.sh_info;
		Iptr layoutIdx = elfToLayoutIndex[targetSectIdx];
		if(layoutIdx < 0) { continue; } // Relocations for sections we didn't load.
		Uptr sectionAddr = layout.sectionLoadAddresses[Uptr(layoutIdx)];

		if(shdr.sh_type == SHT_RELA)
		{
			const Elf64_Rela* relas = reinterpret_cast<const Elf64_Rela*>(bytes + shdr.sh_offset);
			Uptr numRelas = shdr.sh_size / sizeof(Elf64_Rela);
			for(Uptr ri = 0; ri < numRelas; ++ri)
			{
				Uptr symIdx = Uptr(ELF64_R_SYM(relas[ri].r_info));
				U32 rtype = ELF64_R_TYPE(relas[ri].r_info);
				U8* fixup = reinterpret_cast<U8*>(sectionAddr + Uptr(relas[ri].r_offset));
				Uptr fixupAddr = sectionAddr + Uptr(relas[ri].r_offset);
				applyELFReloc(arch,
							  rtype,
							  fixup,
							  fixupAddr,
							  symbolAddresses[symIdx],
							  relas[ri].r_addend,
							  symIdx,
							  layout);
			}
		}
		else
		{
			const Elf64_Rel* rels = reinterpret_cast<const Elf64_Rel*>(bytes + shdr.sh_offset);
			Uptr numRels = shdr.sh_size / sizeof(Elf64_Rel);
			for(Uptr ri = 0; ri < numRels; ++ri)
			{
				Uptr symIdx = Uptr(ELF64_R_SYM(rels[ri].r_info));
				U32 rtype = ELF64_R_TYPE(rels[ri].r_info);
				U8* fixup = reinterpret_cast<U8*>(sectionAddr + Uptr(rels[ri].r_offset));
				Uptr fixupAddr = sectionAddr + Uptr(rels[ri].r_offset);
				I64 addend = arch == Arch::x86_64 ? readELFX64ImplicitAddend(rtype, fixup) : 0;
				applyELFReloc(
					arch, rtype, fixup, fixupAddr, symbolAddresses[symIdx], addend, symIdx, layout);
			}
		}
	}

	// 7. Protect image, fill result.
	protectImage(layout);

	result.imageBase = layout.imageBase;
	result.numImagePages = layout.numPages;
	result.numCodeBytes = layout.codeSize;
	result.numReadOnlyBytes = layout.roSize;
	result.numReadWriteBytes = layout.rwSize;

	// Record section addresses and patch sh_addr in the ELF bytes for GDB and DWARF.
	for(Uptr i = 0; i < sectionMappings.size(); ++i)
	{
		Uptr addr = layout.sectionLoadAddresses[i];
		const char* name = shstrtabData + shdrs[sectionMappings[i].elfIndex].sh_name;
		Log::printf(Log::traceLinking,
					"  ELF section [%" WAVM_PRIuPTR "]: '%s' addr=0x%" WAVM_PRIxPTR
					" size=%" WAVM_PRIuPTR "\n",
					i,
					name,
					addr,
					Uptr(shdrs[sectionMappings[i].elfIndex].sh_size));

		if(addr)
		{
			Elf64_Shdr* shdr = reinterpret_cast<Elf64_Shdr*>(
				bytes + ehdr->e_shoff + sectionMappings[i].elfIndex * sizeof(Elf64_Shdr));
			shdr->sh_addr = U64(addr);
		}

		if(strcmp(name, ".eh_frame") == 0)
		{
			result.unwindInfo.data = reinterpret_cast<const U8*>(addr);
			result.unwindInfo.dataNumBytes = Uptr(shdrs[sectionMappings[i].elfIndex].sh_size);
		}
	}

	// Populate DWARF section pointers from loaded section addresses.
	auto setDwarfSection = [&](Iptr elfIdx, const U8*& outPtr, Uptr& outSize) {
		if(elfIdx >= 0)
		{
			Iptr layoutIdx = elfToLayoutIndex[elfIdx];
			if(layoutIdx >= 0)
			{
				outPtr = reinterpret_cast<const U8*>(layout.sectionLoadAddresses[Uptr(layoutIdx)]);
				outSize = Uptr(shdrs[elfIdx].sh_size);
			}
		}
	};
	setDwarfSection(debugInfoElfIdx, result.dwarf.debugInfo, result.dwarf.debugInfoSize);
	setDwarfSection(debugAbbrevElfIdx, result.dwarf.debugAbbrev, result.dwarf.debugAbbrevSize);
	setDwarfSection(debugStrElfIdx, result.dwarf.debugStr, result.dwarf.debugStrSize);
	setDwarfSection(debugLineElfIdx, result.dwarf.debugLine, result.dwarf.debugLineSize);
	setDwarfSection(debugAddrElfIdx, result.dwarf.debugAddr, result.dwarf.debugAddrSize);
	setDwarfSection(
		debugStrOffsetsElfIdx, result.dwarf.debugStrOffsets, result.dwarf.debugStrOffsetsSize);

	// Collect defined function symbols.
	for(U32 i = 0; i < numSymbols; ++i)
	{
		const Elf64_Sym& sym = symbols[i];
		if(sym.st_shndx == SHN_UNDEF || sym.st_shndx == SHN_ABS) { continue; }
		if(ELF64_ST_TYPE(sym.st_info) != STT_FUNC) { continue; }
		if(ELF64_ST_BIND(sym.st_info) < STB_GLOBAL) { continue; }

		const char* name = strtab + sym.st_name;
		if(!name[0]) { continue; }

		LinkResult::Symbol resultSym;
		resultSym.name = name;
		resultSym.address = symbolAddresses[i];
		resultSym.size = Uptr(sym.st_size);
		result.definedSymbols.push_back(std::move(resultSym));
	}
}

void ObjectLinker::linkELF(U8* bytes,
						   Uptr size,
						   const HashMap<std::string, Uptr>& imports,
						   LinkResult& result,
						   PageAllocator allocatePages)
{
	if(size < sizeof(Elf64_Ehdr)) { Errors::fatalf("ELF file too small"); }

	const Elf64_Ehdr* ehdr = reinterpret_cast<const Elf64_Ehdr*>(bytes);

	// Verify it's 64-bit ELF.
	if(ehdr->e_ident[4] != 2) { Errors::fatalf("Not a 64-bit ELF file"); }

	const char* machName = "?";
	switch(ehdr->e_machine)
	{
	case EM_X86_64: machName = "x86-64"; break;
	case EM_AARCH64: machName = "AArch64"; break;
	default: break;
	}
	Log::printf(Log::traceLinking,
				"--- linkELF: %" WAVM_PRIuPTR " bytes, machine=%s ---\n",
				size,
				machName);

	switch(ehdr->e_machine)
	{
	case EM_X86_64: linkELFArch<Arch::x86_64>(bytes, size, imports, result, allocatePages); break;
	case EM_AARCH64: linkELFArch<Arch::aarch64>(bytes, size, imports, result, allocatePages); break;
	default: Errors::fatalf("Unsupported ELF machine type: %u", ehdr->e_machine);
	}
}
