#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "ObjectLinkerPrivate.h"
#include "WAVM/DWARF/Constants.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Inline/IntCasts.h"
#include "WAVM/Inline/UnalignedArrayView.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/ObjectLinker/ObjectLinker.h"

using namespace WAVM;
using namespace WAVM::DWARF;
using namespace WAVM::ObjectLinker;

// Mach-O header structures (64-bit only).

struct MachHeader64
{
	U32 magic;
	U32 cputype;
	U32 cpusubtype;
	U32 filetype;
	U32 ncmds;
	U32 sizeofcmds;
	U32 flags;
	U32 reserved;
};

struct LoadCommand
{
	U32 cmd;
	U32 cmdsize;
};

struct SegmentCommand64
{
	U32 cmd;
	U32 cmdsize;
	char segname[16];
	U64 vmaddr;
	U64 vmsize;
	U64 fileoff;
	U64 filesize;
	U32 maxprot;
	U32 initprot;
	U32 nsects;
	U32 flags;
};

struct MachOSection64
{
	char sectname[16];
	char segname[16];
	U64 addr;
	U64 size;
	U32 offset;
	U32 align;
	U32 reloff;
	U32 nreloc;
	U32 flags;
	U32 reserved1;
	U32 reserved2;
	U32 reserved3;
};

struct SymtabCommand
{
	U32 cmd;
	U32 cmdsize;
	U32 symoff;
	U32 nsyms;
	U32 stroff;
	U32 strsize;
};

struct NList64
{
	U32 n_strx;
	U8 n_type;
	U8 n_sect;
	U16 n_desc;
	U64 n_value;
};

struct MachORelocationInfo
{
	I32 r_address;
	U32 r_info; // r_symbolnum:24, r_pcrel:1, r_length:2, r_extern:1, r_type:4
};

// Mach-O constants.
static constexpr U32 MH_MAGIC_64 = 0xFEEDFACF;
static constexpr U32 MH_CIGAM_64 = 0xCFFAEDFE;
static constexpr U32 LC_SEGMENT_64 = 0x19;
static constexpr U32 LC_SYMTAB = 0x02;

// CPU types.
static constexpr U32 CPU_TYPE_X86_64 = 0x01000007;
static constexpr U32 CPU_TYPE_ARM64 = 0x0100000C;

// Symbol type bits.
static constexpr U8 N_EXT = 0x01;
static constexpr U8 N_TYPE_MASK = 0x0E;
static constexpr U8 N_UNDF = 0x00;
static constexpr U8 N_SECT = 0x0E;

// Section flags.
static constexpr U32 S_ATTR_PURE_INSTRUCTIONS = 0x80000000;
static constexpr U32 S_ATTR_SOME_INSTRUCTIONS = 0x00000400;

// x86-64 relocation types.
static constexpr U32 X86_64_RELOC_UNSIGNED = 0;
static constexpr U32 X86_64_RELOC_SIGNED = 1;
static constexpr U32 X86_64_RELOC_BRANCH = 2;
static constexpr U32 X86_64_RELOC_GOT_LOAD = 3;
static constexpr U32 X86_64_RELOC_GOT = 4;
static constexpr U32 X86_64_RELOC_SUBTRACTOR = 5;
static constexpr U32 X86_64_RELOC_SIGNED_1 = 6;
static constexpr U32 X86_64_RELOC_SIGNED_2 = 7;
static constexpr U32 X86_64_RELOC_SIGNED_4 = 8;

// AArch64 relocation types.
static constexpr U32 ARM64_RELOC_UNSIGNED = 0;
static constexpr U32 ARM64_RELOC_SUBTRACTOR = 1;
static constexpr U32 ARM64_RELOC_BRANCH26 = 2;
static constexpr U32 ARM64_RELOC_PAGE21 = 3;
static constexpr U32 ARM64_RELOC_PAGEOFF12 = 4;
static constexpr U32 ARM64_RELOC_GOT_LOAD_PAGE21 = 5;
static constexpr U32 ARM64_RELOC_GOT_LOAD_PAGEOFF12 = 6;
static constexpr U32 ARM64_RELOC_POINTER_TO_GOT = 7;
static constexpr U32 ARM64_RELOC_ADDEND = 10;

// Helper to extract relocation info fields.
static U32 relocSymbolNum(const MachORelocationInfo& r) { return r.r_info & 0x00FFFFFF; }
static bool relocPcrel(const MachORelocationInfo& r) { return (r.r_info >> 24) & 1; }
static U32 relocLength(const MachORelocationInfo& r) { return (r.r_info >> 25) & 3; }
static bool relocExtern(const MachORelocationInfo& r) { return (r.r_info >> 27) & 1; }
static U32 relocType(const MachORelocationInfo& r) { return (r.r_info >> 28) & 0xF; }

static const char* getMachOX64RelocName(U32 type)
{
	switch(type)
	{
	case X86_64_RELOC_UNSIGNED: return "UNSIGNED";
	case X86_64_RELOC_SIGNED: return "SIGNED";
	case X86_64_RELOC_BRANCH: return "BRANCH";
	case X86_64_RELOC_GOT_LOAD: return "GOT_LOAD";
	case X86_64_RELOC_GOT: return "GOT";
	case X86_64_RELOC_SUBTRACTOR: return "SUBTRACTOR";
	case X86_64_RELOC_SIGNED_1: return "SIGNED_1";
	case X86_64_RELOC_SIGNED_2: return "SIGNED_2";
	case X86_64_RELOC_SIGNED_4: return "SIGNED_4";
	default: return "?";
	}
}

static const char* getMachOAArch64RelocName(U32 type)
{
	switch(type)
	{
	case ARM64_RELOC_UNSIGNED: return "UNSIGNED";
	case ARM64_RELOC_SUBTRACTOR: return "SUBTRACTOR";
	case ARM64_RELOC_BRANCH26: return "BRANCH26";
	case ARM64_RELOC_PAGE21: return "PAGE21";
	case ARM64_RELOC_PAGEOFF12: return "PAGEOFF12";
	case ARM64_RELOC_GOT_LOAD_PAGE21: return "GOT_LOAD_PAGE21";
	case ARM64_RELOC_GOT_LOAD_PAGEOFF12: return "GOT_LOAD_PAGEOFF12";
	case ARM64_RELOC_POINTER_TO_GOT: return "POINTER_TO_GOT";
	case ARM64_RELOC_ADDEND: return "ADDEND";
	default: return "?";
	}
}

static SectionAccess getMachOSectionAccess(const MachOSection64& sect)
{
	if(sect.flags & S_ATTR_PURE_INSTRUCTIONS || sect.flags & S_ATTR_SOME_INSTRUCTIONS)
	{
		return SectionAccess::readExecute;
	}
	if(memcmp(sect.segname, "__DATA", 6) == 0) { return SectionAccess::readWrite; }
	return SectionAccess::readOnly;
}

struct CIEEncodings
{
	U8 fdeEncoding;  // 'R' augmentation: how pc_begin/pc_range are encoded in FDEs.
	U8 lsdaEncoding; // 'L' augmentation: how the LSDA pointer is encoded in FDEs.
	bool hasAugData; // Whether the CIE has 'z' augmentation (FDEs have augmentation data).
};

// Parse a CIE at the given offset within an eh_frame section to extract pointer encodings.
static CIEEncodings parseCIEEncodings(const U8* ehFrame, Uptr ehFrameSize, Uptr cieOffset)
{
	CIEEncodings result = {0, 0xFF /* DW_EH_PE_omit */, false};
	if(cieOffset + 12 > ehFrameSize) { return result; }
	U32 cieLength = readLE<U32>(ehFrame + cieOffset);
	if(cieLength == 0 || cieOffset + 4 + cieLength > ehFrameSize) { return result; }
	// Verify this is a CIE (CIE ID = 0 in .eh_frame format).
	U32 cieId = readLE<U32>(ehFrame + cieOffset + 4);
	if(cieId != 0) { return result; }
	Uptr p = cieOffset + 9; // Past length(4), CIE id(4), version(1).
	Uptr cieEnd = cieOffset + 4 + cieLength;
	const char* aug = reinterpret_cast<const char*>(ehFrame + p);
	while(p < cieEnd && ehFrame[p] != 0) { ++p; }
	++p; // Skip null terminator.
	// Skip code alignment, data alignment, RA register (uleb128 each).
	for(int i = 0; i < 3; ++i)
	{
		while(p < cieEnd && (ehFrame[p] & 0x80)) { ++p; }
		++p;
	}
	if(aug[0] != 'z' || p >= cieEnd) { return result; }
	result.hasAugData = true;
	// Skip augmentation data length (uleb128).
	while(p < cieEnd && (ehFrame[p] & 0x80)) { ++p; }
	++p;
	for(const char* c = aug + 1; *c && p < cieEnd; ++c)
	{
		if(*c == 'R')
		{
			result.fdeEncoding = ehFrame[p++];
			Log::printf(Log::traceLinking,
						"    parseCIE: aug='%s' R=0x%02x L=0x%02x at p=%" WAVM_PRIuPTR "\n",
						aug,
						result.fdeEncoding,
						result.lsdaEncoding,
						p);
		}
		else if(*c == 'L') { result.lsdaEncoding = ehFrame[p++]; }
		else if(*c == 'P')
		{
			U8 enc = ehFrame[p++];
			Uptr sz = getEncodedPointerSize(enc);
			if(sz > 0) { p += sz; }
		}
		else if(*c == 'S' || *c == 'B') {}
		else
		{
			break;
		}
	}
	return result;
}

// Correct a single 8-byte pcrel field in the eh_frame: resolve the target in the object address
// space, find its section, and adjust for the difference between object and load deltas.
// Only needed on x86-64 where LLVM resolves these at assembly time without relocations; on
// AArch64, explicit SUBTRACTOR+UNSIGNED relocation pairs handle them.
static void correctEhFramePcRel64(U8* ehFrame,
								  Uptr pos,
								  U64 ehFrameDelta,
								  const std::vector<MachOSection64*>& sectionHeaders,
								  const ImageLayout& layout)
{
	U64 pcRel = readLE<U64>(ehFrame + pos);
	if(pcRel == 0) { return; }
	U64 fieldLoadAddr = reinterpret_cast<Uptr>(ehFrame) + pos;
	U64 fieldObjAddr = fieldLoadAddr - ehFrameDelta;
	U64 targetObjAddr = fieldObjAddr + pcRel;

	for(Uptr si = 0; si < sectionHeaders.size(); ++si)
	{
		U64 sectObjAddr = sectionHeaders[si]->addr;
		U64 sectSize = sectionHeaders[si]->size;
		if(targetObjAddr >= sectObjAddr && targetObjAddr < sectObjAddr + sectSize)
		{
			U64 correction = (U64(layout.sectionLoadAddresses[si]) - sectObjAddr) - ehFrameDelta;
			if(correction != 0)
			{
				pcRel += correction;
				writeLE<U64>(ehFrame + pos, pcRel);
			}
			return;
		}
	}
}

// Parse FDEs in an eh_frame section, building a function-address-to-FDE-offset map for compact
// unwind synthesis. If correctPcRelValues is true (x86-64), also corrects pcrel pc_begin and LSDA
// fields that LLVM resolved at assembly time; our layout may have changed relative distances.
// On AArch64, LLVM emits explicit relocations for these, so no correction is needed.
static void parseAndCorrectEhFrameFDEs(bool correctPcRelValues,
									   U8* ehFrame,
									   Uptr ehFrameSize,
									   U64 ehFrameDelta,
									   const std::vector<MachOSection64*>& sectionHeaders,
									   const ImageLayout& layout,
									   HashMap<Uptr, U32>& funcAddrToFdeOffset)
{
	Uptr pos = 0;
	while(pos + 8 <= ehFrameSize)
	{
		U32 entryLength = readLE<U32>(ehFrame + pos);
		if(entryLength == 0 || entryLength == 0xFFFFFFFF) { break; }
		Uptr entryStart = pos;
		Uptr entryEnd = entryStart + 4 + entryLength;
		pos += 4; // Past length.
		U32 ciePtr = readLE<U32>(ehFrame + pos);
		pos += 4; // Past CIE pointer.

		if(ciePtr == 0)
		{
			pos = entryEnd;
			continue;
		} // CIE; skip.

		// FDE: resolve the CIE it references. CIE pointer is relative to the field's position.
		Uptr ciePtrFieldPos = entryStart + 4;
		Uptr cieOffset = ciePtrFieldPos - ciePtr;
		CIEEncodings cie = parseCIEEncodings(ehFrame, ehFrameSize, cieOffset);
		Uptr ptrSize = getEncodedPointerSize(cie.fdeEncoding);
		if(ptrSize == 0) { ptrSize = sizeof(void*); }
		bool isPcRel = (cie.fdeEncoding & 0x70) == 0x10;

		// pc_begin.
		Uptr pcBeginPos = pos;
		if(!isPcRel || pos + ptrSize > entryEnd)
		{
			pos = entryEnd;
			continue;
		}
		if(correctPcRelValues)
		{
			WAVM_ERROR_UNLESS(ptrSize == 8);
			correctEhFramePcRel64(ehFrame, pos, ehFrameDelta, sectionHeaders, layout);
		}
		pos += ptrSize;

		// pc_range (skip; it's a length, not a relocatable address).
		pos += ptrSize;

		// Augmentation data (if CIE has 'z'): length (uleb128) then LSDA pointer.
		Uptr lsdaPtrSize = getEncodedPointerSize(cie.lsdaEncoding);
		bool lsdaIsPcRel = (cie.lsdaEncoding & 0x70) == 0x10;
		if(cie.hasAugData && lsdaIsPcRel && lsdaPtrSize > 0 && pos < entryEnd)
		{
			// Skip augmentation data length (uleb128).
			while(pos < entryEnd && (ehFrame[pos] & 0x80)) { ++pos; }
			++pos;
			// LSDA pointer.
			if(correctPcRelValues && pos + lsdaPtrSize <= entryEnd)
			{
				WAVM_ERROR_UNLESS(lsdaPtrSize == 8);
				correctEhFramePcRel64(ehFrame, pos, ehFrameDelta, sectionHeaders, layout);
			}
		}

		// Record the (now-corrected) pc_begin for compact unwind synthesis.
		// Read the pcrel value and add to the field's load address to get the function address.
		U64 pcBeginFieldAddr = reinterpret_cast<Uptr>(ehFrame) + pcBeginPos;
		U64 pcRelValue = (ptrSize == 4) ? U64(readLE<U32>(ehFrame + pcBeginPos))
										: readLE<U64>(ehFrame + pcBeginPos);
		Uptr targetLoadAddr = Uptr(pcBeginFieldAddr + pcRelValue);
		funcAddrToFdeOffset.set(targetLoadAddr, narrow<U32>("FDE offset", entryStart));

		pos = entryEnd;
	}
}

// Resolve the paired UNSIGNED relocation in a SUBTRACTOR pair.
static void applyMachOSubtractor(U8* fixup,
								 U32 rlength,
								 U64 symbolB,
								 const MachORelocationInfo& nextRel,
								 const std::vector<Uptr>& symbolAddresses,
								 const std::vector<MachOSection64*>& sectionHeaders,
								 const ImageLayout& layout)
{
	U64 symbolA;
	if(relocExtern(nextRel)) { symbolA = symbolAddresses[relocSymbolNum(nextRel)]; }
	else
	{
		U32 nextSectNum = relocSymbolNum(nextRel);
		symbolA
			= layout.sectionLoadAddresses[nextSectNum - 1] - sectionHeaders[nextSectNum - 1]->addr;
	}
	if(rlength == 3)
	{
		I64 addend = readLE<I64>(fixup);
		writeLE<U64>(fixup, wrap<U64>(wrap<I64>(symbolA) - wrap<I64>(symbolB) + addend));
	}
	else
	{
		I32 addend = readLE<I32>(fixup);
		writeLE<U32>(fixup, wrap<U32>(wrap<I64>(symbolA) - wrap<I64>(symbolB) + addend));
	}
}

// Returns the number of relocations consumed (1, or 2 for SUBTRACTOR pairs).
static U32 applyMachOX64Reloc(UnalignedArrayView<MachORelocationInfo> relocs,
							  U32 ri,
							  U32 nreloc,
							  U8* fixup,
							  Uptr fixupAddr,
							  U64 symbolValue,
							  const MachORelocationInfo& rel,
							  const std::vector<Uptr>& symbolAddresses,
							  const std::vector<MachOSection64*>& sectionHeaders,
							  ImageLayout& layout)
{
	U32 rtype = relocType(rel);
	U32 rlength = relocLength(rel);

	Log::printf(Log::traceLinking,
				"  reloc @0x%" WAVM_PRIxPTR
				" type=%s(%u) extern=%u sym=%u"
				" symval=0x%" PRIx64 " len=%u\n",
				fixupAddr,
				getMachOX64RelocName(rtype),
				rtype,
				U32(relocExtern(rel)),
				relocSymbolNum(rel),
				symbolValue,
				rlength);
	switch(rtype)
	{
	case X86_64_RELOC_UNSIGNED: {
		if(rlength == 3) { relocAbs64(fixup, symbolValue, readLE<I64>(fixup)); }
		else if(rlength == 2) { relocAbs32(fixup, symbolValue, readLE<U32>(fixup)); }
		break;
	}

	case X86_64_RELOC_SIGNED:
	case X86_64_RELOC_BRANCH: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 4, fixupAddr + 4, layout);
		break;
	}

	case X86_64_RELOC_SIGNED_1: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 5, fixupAddr + 4, layout);
		break;
	}

	case X86_64_RELOC_SIGNED_2: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 6, fixupAddr + 4, layout);
		break;
	}

	case X86_64_RELOC_SIGNED_4: {
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, symbolValue, I64(addend) + 8, fixupAddr + 4, layout);
		break;
	}

	case X86_64_RELOC_GOT_LOAD:
	case X86_64_RELOC_GOT: {
		Uptr gotEntry = getOrCreateGotEntry(layout, symbolValue);
		I32 addend = readLE<I32>(fixup);
		relocRel32(fixup, gotEntry, I64(addend), fixupAddr + 4, layout);
		break;
	}

	case X86_64_RELOC_SUBTRACTOR: {
		if(ri + 1 >= nreloc)
		{
			Errors::fatalf("X86_64_RELOC_SUBTRACTOR without following relocation");
		}
		MachORelocationInfo nextRel = relocs[ri + 1];
		if(relocType(nextRel) != X86_64_RELOC_UNSIGNED)
		{
			Errors::fatalf("X86_64_RELOC_SUBTRACTOR not followed by X86_64_RELOC_UNSIGNED");
		}
		applyMachOSubtractor(
			fixup, rlength, symbolValue, nextRel, symbolAddresses, sectionHeaders, layout);
		return 2;
	}

	default: Errors::fatalf("Unsupported MachO x86-64 relocation type: %u", rtype);
	}
	return 1;
}

// Returns the number of relocations consumed (1, or 2 for SUBTRACTOR pairs).
static U32 applyMachOAArch64Reloc(UnalignedArrayView<MachORelocationInfo> relocs,
								  U32 ri,
								  U32 nreloc,
								  U8* fixup,
								  Uptr fixupAddr,
								  U64 symbolValue,
								  const MachORelocationInfo& rel,
								  I64 extraAddend,
								  const std::vector<Uptr>& symbolAddresses,
								  const std::vector<MachOSection64*>& sectionHeaders,
								  ImageLayout& layout)
{
	U32 rtype = relocType(rel);
	U32 rlength = relocLength(rel);

	Log::printf(Log::traceLinking,
				"  reloc @0x%" WAVM_PRIxPTR
				" type=%s(%u) extern=%u sym=%u"
				" symval=0x%" PRIx64 " len=%u addend=%" PRId64 "\n",
				fixupAddr,
				getMachOAArch64RelocName(rtype),
				rtype,
				U32(relocExtern(rel)),
				relocSymbolNum(rel),
				symbolValue,
				rlength,
				extraAddend);
	switch(rtype)
	{
	case ARM64_RELOC_UNSIGNED: {
		if(rlength == 3) { relocAbs64(fixup, symbolValue, readLE<I64>(fixup)); }
		else
		{
			I32 addend = readLE<I32>(fixup);
			relocPrel32(fixup, symbolValue, addend, fixupAddr);
		}
		break;
	}

	case ARM64_RELOC_SUBTRACTOR: {
		if(ri + 1 >= nreloc)
		{
			Errors::fatalf("ARM64_RELOC_SUBTRACTOR without following relocation");
		}
		MachORelocationInfo nextRel = relocs[ri + 1];
		if(relocType(nextRel) != ARM64_RELOC_UNSIGNED)
		{
			Errors::fatalf("ARM64_RELOC_SUBTRACTOR not followed by ARM64_RELOC_UNSIGNED");
		}
		applyMachOSubtractor(
			fixup, rlength, symbolValue, nextRel, symbolAddresses, sectionHeaders, layout);
		return 2;
	}

	case ARM64_RELOC_BRANCH26:
		relocBranch26(fixup, symbolValue, extraAddend, fixupAddr, layout);
		break;

	case ARM64_RELOC_PAGE21: relocAdrp(fixup, symbolValue, extraAddend, fixupAddr); break;

	case ARM64_RELOC_PAGEOFF12: {
		U32 insn = readLE<U32>(fixup);
		if((insn & 0x3B000000) == 0x39000000) { relocLdstLo12(fixup, symbolValue, extraAddend); }
		else
		{
			relocAddLo12(fixup, symbolValue, extraAddend);
		}
		break;
	}

	case ARM64_RELOC_GOT_LOAD_PAGE21: {
		Uptr gotEntry = getOrCreateGotEntry(layout, symbolValue);
		relocAdrp(fixup, gotEntry, 0, fixupAddr);
		break;
	}

	case ARM64_RELOC_GOT_LOAD_PAGEOFF12: {
		Uptr gotEntry = getOrCreateGotEntry(layout, symbolValue);
		relocLdstLo12(fixup, gotEntry, 0);
		break;
	}

	case ARM64_RELOC_POINTER_TO_GOT: {
		Uptr gotEntry = getOrCreateGotEntry(layout, symbolValue);
		if(relocPcrel(rel))
		{
			I64 delta = wrap<I64>(gotEntry) - wrap<I64>(fixupAddr);
			writeLE<U32>(fixup, wrap<U32>(delta));
		}
		else
		{
			writeLE<U64>(fixup, wrap<U64>(gotEntry));
		}
		break;
	}

	default: Errors::fatalf("Unsupported MachO AArch64 relocation type: %u", rtype);
	}
	return 1;
}

// Returns the number of relocations consumed.
static U32 applyMachOReloc(Arch arch,
						   UnalignedArrayView<MachORelocationInfo> relocs,
						   U32 ri,
						   U32 nreloc,
						   U8* fixup,
						   Uptr fixupAddr,
						   U64 symbolValue,
						   const MachORelocationInfo& rel,
						   I64 extraAddend,
						   const std::vector<Uptr>& symbolAddresses,
						   const std::vector<MachOSection64*>& sectionHeaders,
						   ImageLayout& layout)
{
	switch(arch)
	{
	case Arch::x86_64:
		return applyMachOX64Reloc(relocs,
								  ri,
								  nreloc,
								  fixup,
								  fixupAddr,
								  symbolValue,
								  rel,
								  symbolAddresses,
								  sectionHeaders,
								  layout);
	case Arch::aarch64:
		return applyMachOAArch64Reloc(relocs,
									  ri,
									  nreloc,
									  fixup,
									  fixupAddr,
									  symbolValue,
									  rel,
									  extraAddend,
									  symbolAddresses,
									  sectionHeaders,
									  layout);
	default: WAVM_UNREACHABLE();
	}
}

template<Arch arch>
static void linkMachOArch(U8* bytes,
						  Uptr size,
						  const HashMap<std::string, Uptr>& imports,
						  LinkResult& result,
						  PageAllocator allocatePages)
{
	const MachHeader64* header = reinterpret_cast<const MachHeader64*>(bytes);

	// Parse load commands to find segments and symtab.
	std::vector<MachOSection64*> sectionHeaders;
	const SymtabCommand* symtab = nullptr;

	U8* cmdPtr = bytes + sizeof(MachHeader64);
	for(U32 i = 0; i < header->ncmds; ++i)
	{
		const LoadCommand* cmd = reinterpret_cast<const LoadCommand*>(cmdPtr);
		if(cmd->cmd == LC_SEGMENT_64)
		{
			const SegmentCommand64* seg = reinterpret_cast<const SegmentCommand64*>(cmdPtr);
			U8* sectPtr = cmdPtr + sizeof(SegmentCommand64);
			for(U32 s = 0; s < seg->nsects; ++s)
			{
				MachOSection64* sect
					= reinterpret_cast<MachOSection64*>(sectPtr + s * sizeof(MachOSection64));
				sectionHeaders.push_back(sect);
			}
		}
		else if(cmd->cmd == LC_SYMTAB) { symtab = reinterpret_cast<const SymtabCommand*>(cmdPtr); }
		cmdPtr += cmd->cmdsize;
	}

	// If there's no LC_SYMTAB (e.g. an empty module with no functions), treat as zero symbols.
	const U32 nsyms = symtab ? symtab->nsyms : 0;
	UnalignedArrayView<NList64> symbols
		= symtab ? UnalignedArrayView<NList64>{bytes + symtab->symoff, nsyms}
				 : UnalignedArrayView<NList64>{nullptr, 0};
	const char* strtab = symtab ? reinterpret_cast<const char*>(bytes + symtab->stroff) : "";

	// 1. Collect section layout info.
	std::vector<SectionLayoutInfo> layoutInfos;
	for(const auto* sect : sectionHeaders)
	{
		SectionLayoutInfo info;
		info.dataSize = sect->size;
		info.alignment = Uptr(1) << sect->align;
		info.access = getMachOSectionAccess(*sect);
		layoutInfos.push_back(info);
	}

	// 2. Scan relocations to count GOT slots (unique GOT-type reloc targets)
	// and stub slots (unique stub-creating reloc targets).
	Uptr numGotSlots = 0;
	Uptr numStubSlots = 0;
	{
		// Use symbol index for extern relocs, section number offset by nsyms for non-extern.
		HashSet<U32> gotSymsSeen;
		HashSet<U32> stubSymsSeen;
		for(const auto* sect : sectionHeaders)
		{
			if(sect->nreloc == 0) { continue; }
			UnalignedArrayView<MachORelocationInfo> relocs{bytes + sect->reloff, sect->nreloc};
			for(U32 ri = 0; ri < sect->nreloc; ++ri)
			{
				MachORelocationInfo rel = relocs[ri];
				U32 rtype = relocType(rel);

				// Check for stub-creating relocation types.
				bool needsStub;
				if constexpr(arch == Arch::x86_64)
				{
					needsStub = rtype == X86_64_RELOC_SIGNED || rtype == X86_64_RELOC_BRANCH
								|| rtype == X86_64_RELOC_SIGNED_1 || rtype == X86_64_RELOC_SIGNED_2
								|| rtype == X86_64_RELOC_SIGNED_4;
				}
				else
				{
					needsStub = rtype == ARM64_RELOC_BRANCH26;
				}
				if(needsStub) { stubSymsSeen.add(relocSymbolNum(rel)); }

				// Check for GOT-type relocation types.
				bool isGot;
				if constexpr(arch == Arch::x86_64)
				{
					isGot = rtype == X86_64_RELOC_GOT_LOAD || rtype == X86_64_RELOC_GOT;
				}
				else
				{
					isGot = rtype == ARM64_RELOC_GOT_LOAD_PAGE21
							|| rtype == ARM64_RELOC_GOT_LOAD_PAGEOFF12
							|| rtype == ARM64_RELOC_POINTER_TO_GOT;
				}
				if(!isGot) { continue; }
				U32 key = relocExtern(rel) ? relocSymbolNum(rel) : nsyms + relocSymbolNum(rel);
				gotSymsSeen.add(key);
			}
		}
		numGotSlots = gotSymsSeen.size();
		numStubSlots = stubSymsSeen.size();
	}

	// 2b. Find __LD,__compact_unwind and pre-calculate synthesized __unwind_info size.
	// The linker must synthesize __unwind_info from __LD,__compact_unwind because LLVM
	// never generates __TEXT,__unwind_info in object files; that's the linker's job.
	Uptr compactUnwindSectionIdx = UINTPTR_MAX;
	Uptr unwindInfoReserveBytes = 0;
	for(Uptr i = 0; i < sectionHeaders.size(); ++i)
	{
		if(strncmp(sectionHeaders[i]->sectname, "__compact_unwind", 16) == 0
		   && sectionHeaders[i]->size > 0)
		{
			compactUnwindSectionIdx = i;
			Uptr numEntries = Uptr(sectionHeaders[i]->size) / 32;
			// Upper bound: header(28) + personalities(numEntries*4) + index(24)
			//            + LSDAs(numEntries*8) + page header(8) + entries(numEntries*8)
			unwindInfoReserveBytes = 60 + numEntries * 20;
			// Personality pointers also need GOT entries (upper bound: one per entry).
			numGotSlots += numEntries;
			break;
		}
	}

	// 3. Layout image.
	ImageLayout layout = layoutImage(layoutInfos,
									 numStubSlots * stubSlotSize,
									 numGotSlots * 8,
									 unwindInfoReserveBytes,
									 allocatePages);

	// 4. Copy section data to load addresses.
	for(Uptr i = 0; i < sectionHeaders.size(); ++i)
	{
		if(sectionHeaders[i]->size > 0 && sectionHeaders[i]->offset > 0)
		{
			memcpy(reinterpret_cast<U8*>(layout.sectionLoadAddresses[i]),
				   bytes + sectionHeaders[i]->offset,
				   sectionHeaders[i]->size);
		}
	}

	// 5. Build symbol addresses.
	std::vector<Uptr> symbolAddresses(nsyms, 0);
	for(U32 i = 0; i < nsyms; ++i)
	{
		const NList64 sym = symbols[i];
		U8 type = sym.n_type & N_TYPE_MASK;
		if(type == N_SECT)
		{
			// Defined symbol: section index is 1-based.
			Uptr sectIdx = Uptr(sym.n_sect) - 1;
			if(sectIdx >= sectionHeaders.size())
			{
				Errors::fatalf("MachO symbol section index out of range");
			}
			Uptr sectionBase = layout.sectionLoadAddresses[sectIdx];
			Uptr sectionFileBase = sectionHeaders[sectIdx]->addr;
			symbolAddresses[i] = sectionBase + (sym.n_value - sectionFileBase);
		}
		else if(type == N_UNDF && (sym.n_type & N_EXT))
		{
			// External symbol.
			const char* name = strtab + sym.n_strx;
			// Strip leading underscore on MachO.
			const char* demangledName = name[0] == '_' ? name + 1 : name;
			const Uptr* addr = imports.get(std::string(demangledName));
			if(!addr)
			{
				Errors::fatalf("Unresolved MachO symbol: %s (demangled: %s)", name, demangledName);
			}
			symbolAddresses[i] = *addr;
		}

		const char* symName = strtab + sym.n_strx;
		Log::printf(Log::traceLinking,
					"  MachO sym [%u]: '%s' type=0x%02x sect=%u value=0x%" PRIx64
					" addr=0x%" WAVM_PRIxPTR "\n",
					i,
					symName,
					U32(sym.n_type),
					U32(sym.n_sect),
					sym.n_value,
					symbolAddresses[i]);
	}

	// 6. Apply relocations per section.
	for(Uptr sectIdx = 0; sectIdx < sectionHeaders.size(); ++sectIdx)
	{
		const MachOSection64* sect = sectionHeaders[sectIdx];
		if(sect->nreloc == 0) { continue; }

		Uptr sectionAddr = layout.sectionLoadAddresses[sectIdx];
		UnalignedArrayView<MachORelocationInfo> relocs{bytes + sect->reloff, sect->nreloc};

		I64 extraAddend = 0;
		for(U32 ri = 0; ri < sect->nreloc;)
		{
			MachORelocationInfo rel = relocs[ri];

			// Handle ARM64_RELOC_ADDEND: consume the addend for the next relocation.
			if(arch == Arch::aarch64 && relocType(rel) == ARM64_RELOC_ADDEND)
			{
				extraAddend = rel.r_info & 0x00FFFFFF;
				if(extraAddend & 0x800000) { extraAddend |= ~I64(0xFFFFFF); }
				++ri;
				continue;
			}

			U8* fixup = reinterpret_cast<U8*>(sectionAddr + wrap<U32>(rel.r_address));
			Uptr fixupAddr = sectionAddr + wrap<U32>(rel.r_address);

			// Resolve the symbol value.
			U64 symbolValue = 0;
			if(relocExtern(rel))
			{
				U32 symIdx = relocSymbolNum(rel);
				if(symIdx >= nsyms)
				{
					Errors::fatalf("MachO relocation symbol index out of range");
				}
				symbolValue = symbolAddresses[symIdx];
			}
			else
			{
				U32 sectNum = relocSymbolNum(rel);
				if(sectNum == 0 || sectNum > sectionHeaders.size())
				{
					Errors::fatalf("MachO non-extern relocation section number out of range");
				}
				symbolValue
					= layout.sectionLoadAddresses[sectNum - 1] - sectionHeaders[sectNum - 1]->addr;
			}

			ri += applyMachOReloc(arch,
								  relocs,
								  ri,
								  sect->nreloc,
								  fixup,
								  fixupAddr,
								  symbolValue,
								  rel,
								  extraAddend,
								  symbolAddresses,
								  sectionHeaders,
								  layout);
			extraAddend = 0;
		}
	}

	// 7. Fill result.
	result.imageBase = layout.imageBase;
	result.numImagePages = layout.numPages;
	result.numCodeBytes = layout.codeSize;
	result.numReadOnlyBytes = layout.roSize;
	result.numReadWriteBytes = layout.rwSize;

	// Record section addresses and find unwind sections.
	for(Uptr i = 0; i < sectionHeaders.size(); ++i)
	{
		const MachOSection64* sect = sectionHeaders[i];
		Uptr addr = layout.sectionLoadAddresses[i];
		char seg[sizeof(sect->segname) + 1] = {0};
		char sn[sizeof(sect->sectname) + 1] = {0};
		memcpy(seg, sect->segname, sizeof(sect->segname));
		memcpy(sn, sect->sectname, sizeof(sect->sectname));
		std::string fullName = std::string(seg) + "," + std::string(sn);
		Log::printf(Log::traceLinking,
					"  MachO section [%" WAVM_PRIuPTR "]: '%s' addr=0x%" WAVM_PRIxPTR
					" size=%" WAVM_PRIuPTR " nreloc=%u\n",
					i,
					fullName.c_str(),
					addr,
					Uptr(sect->size),
					sect->nreloc);
		if(strncmp(sect->sectname, "__eh_frame", 16) == 0)
		{
			result.unwindInfo.data = reinterpret_cast<const U8*>(addr);
			result.unwindInfo.dataNumBytes = sect->size;
		}
		else if(strncmp(sect->sectname, "__unwind_info", 16) == 0)
		{
			result.unwindInfo.compactUnwind = reinterpret_cast<const U8*>(addr);
			result.unwindInfo.compactUnwindNumBytes = sect->size;
		}
		else if(strncmp(sect->sectname, "__debug_info", 16) == 0)
		{
			result.dwarf.debugInfo = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugInfoSize = Uptr(sect->size);
		}
		else if(strncmp(sect->sectname, "__debug_abbrev", 16) == 0)
		{
			result.dwarf.debugAbbrev = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugAbbrevSize = Uptr(sect->size);
		}
		else if(strncmp(sect->sectname, "__debug_str", 16) == 0)
		{
			result.dwarf.debugStr = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugStrSize = Uptr(sect->size);
		}
		else if(strncmp(sect->sectname, "__debug_line", 16) == 0)
		{
			result.dwarf.debugLine = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugLineSize = Uptr(sect->size);
		}
		else if(strncmp(sect->sectname, "__debug_addr", 16) == 0)
		{
			result.dwarf.debugAddr = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugAddrSize = Uptr(sect->size);
		}
		else if(strncmp(sect->sectname, "__debug_str_offs", 16) == 0)
		{
			result.dwarf.debugStrOffsets = reinterpret_cast<const U8*>(addr);
			result.dwarf.debugStrOffsetsSize = Uptr(sect->size);
		}
	}

	// Relocate pcrel FDE pc_begin values in __eh_frame and build a function-address-to-FDE-offset
	// map (used by compact unwind synthesis below).
	//
	// LLVM resolves pcrel references between sections at assembly time based on their positions
	// in the object file, which interleaves __TEXT and __DWARF sections. Our layout may place
	// them at different relative distances, so the pcrel values need correction.
	HashMap<Uptr, U32> funcAddrToFdeOffset;
	if(result.unwindInfo.data && result.unwindInfo.dataNumBytes)
	{
		// Find the eh_frame section index to compute its object-to-load delta.
		Uptr ehFrameSectIdx = UINTPTR_MAX;
		for(Uptr si = 0; si < sectionHeaders.size(); ++si)
		{
			if(layout.sectionLoadAddresses[si] == reinterpret_cast<Uptr>(result.unwindInfo.data))
			{
				ehFrameSectIdx = si;
				break;
			}
		}

		if(ehFrameSectIdx != UINTPTR_MAX)
		{
			U64 ehFrameDelta = U64(layout.sectionLoadAddresses[ehFrameSectIdx])
							   - sectionHeaders[ehFrameSectIdx]->addr;
			parseAndCorrectEhFrameFDEs(arch == Arch::x86_64,
									   const_cast<U8*>(result.unwindInfo.data),
									   result.unwindInfo.dataNumBytes,
									   ehFrameDelta,
									   sectionHeaders,
									   layout,
									   funcAddrToFdeOffset);
		}
	}

	// Synthesize __unwind_info from __LD,__compact_unwind.
	// LLVM never generates __TEXT,__unwind_info in object files; that's the linker's job.
	// The synthesized data is written into reserved space in the image's read-only segment.
	if(compactUnwindSectionIdx != UINTPTR_MAX && !result.unwindInfo.compactUnwind)
	{
		Uptr i = compactUnwindSectionIdx;
		const MachOSection64* cuSect = sectionHeaders[i];

		// Each compact_unwind_entry is 32 bytes:
		//   U64 functionStart, U32 length, U32 encoding, U64 personality, U64 lsda
		Uptr numEntries = Uptr(cuSect->size) / 32;
		const U8* cuData = reinterpret_cast<const U8*>(layout.sectionLoadAddresses[i]);
		Uptr imageBase = Uptr(layout.imageBase);

		// Collect unique personality pointers and build entry list.
		struct CUEntry
		{
			U32 funcOffset;
			U32 funcLength;
			U32 encoding;
			U64 personality;
			U64 lsda;
		};
		std::vector<CUEntry> entries;
		std::vector<U64> personalities;
		for(Uptr e = 0; e < numEntries; ++e)
		{
			const U8* p = cuData + e * 32;
			CUEntry entry;
			U64 funcStart = readLE<U64>(p);
			entry.funcLength = readLE<U32>(p + 8);
			entry.encoding = readLE<U32>(p + 12);
			entry.personality = readLE<U64>(p + 16);
			entry.lsda = readLE<U64>(p + 24);
			// Keep UNWIND_HAS_LSDA (0x40000000) in the encoding; system libunwind
			// checks this bit to decide whether to search the LSDA index table.
			entry.funcOffset = narrow<U32>("compact_unwind funcOffset", funcStart - imageBase);
			Log::printf(Log::traceLinking,
						"  compact_unwind[%" WAVM_PRIuPTR "]: funcStart=0x%" PRIx64
						" funcOffset=0x%x len=%u"
						" encoding=0x%08x personality=0x%" PRIx64 " lsda=0x%" PRIx64 "\n",
						e,
						funcStart,
						entry.funcOffset,
						entry.funcLength,
						entry.encoding,
						entry.personality,
						entry.lsda);

			// Map personality pointer to 1-based index in the array.
			if(entry.personality)
			{
				U32 persIdx = 0;
				for(Uptr pi = 0; pi < personalities.size(); ++pi)
				{
					if(personalities[pi] == entry.personality)
					{
						persIdx = U32(pi + 1);
						break;
					}
				}
				if(!persIdx)
				{
					personalities.push_back(entry.personality);
					persIdx = U32(personalities.size());
				}
				// Clear old personality bits (29:28) and set new index.
				entry.encoding = (entry.encoding & 0xCFFFFFFF) | (persIdx << 28);
			}
			entries.push_back(entry);
		}

		// Sort entries by function offset.
		std::sort(entries.begin(), entries.end(), [](const CUEntry& a, const CUEntry& b) {
			return a.funcOffset < b.funcOffset;
		});

		// Count LSDA entries.
		Uptr numLsda = 0;
		for(const auto& e : entries)
		{
			if(e.lsda) { ++numLsda; }
		}

		// For DWARF mode entries, fill in the FDE offset (lower 24 bits of encoding).
		// System libunwind uses this offset to find the function's FDE in .eh_frame.
		// LLVM leaves the offset as 0 in __LD,__compact_unwind; the linker must fill it.
		// This matches JITLink's writeSecondLevelPages which encodes FDE offsets.
		U32 dwarfMode;
		switch(arch)
		{
		case Arch::aarch64: dwarfMode = 0x03000000; break;
		case Arch::x86_64: dwarfMode = 0x04000000; break;
		default: WAVM_UNREACHABLE();
		}

		// Use the funcAddrToFdeOffset map (built by relocateEhFrameFDEs above) to fill in
		// FDE offsets for DWARF mode entries.
		{
			for(auto& entry : entries)
			{
				if((entry.encoding & 0x0F000000) == dwarfMode)
				{
					Uptr funcAddr = imageBase + entry.funcOffset;
					const U32* fdeOffset = funcAddrToFdeOffset.get(funcAddr);
					if(fdeOffset)
					{
						entry.encoding |= (*fdeOffset & 0x00FFFFFF);
						Log::printf(Log::traceLinking,
									"  DWARF entry funcOffset=0x%x: FDE at .eh_frame+0x%x\n",
									entry.funcOffset,
									*fdeOffset);
					}
				}
			}
		}

		// Build __unwind_info layout.
		// Regular second-level pages hold at most 511 entries: (4096 - 8) / 8.
		static constexpr U32 maxEntriesPerPage = 511;
		U32 numSortedEntries = U32(entries.size());
		U32 numPages
			= numSortedEntries ? (numSortedEntries + maxEntriesPerPage - 1) / maxEntriesPerPage : 1;

		U32 headerSize = 28;
		U32 personalityOff = headerSize;
		U32 personalitySize = narrow<U32>("personalitySize", personalities.size() * 4);
		U32 indexOff = personalityOff + personalitySize;
		U32 indexCount = numPages + 1; // one entry per page + sentinel
		U32 indexSize = indexCount * 12;
		U32 lsdaOff = indexOff + indexSize;
		U32 lsdaSize = narrow<U32>("lsdaSize", numLsda * 8);
		U32 pagesOff = lsdaOff + lsdaSize;
		U32 pagesSize = 0;
		for(U32 p = 0; p < numPages; ++p)
		{
			U32 start = p * maxEntriesPerPage;
			U32 count = std::min(maxEntriesPerPage, numSortedEntries - start);
			pagesSize += 8 + count * 8;
		}
		U32 totalSize = pagesOff + pagesSize;

		WAVM_ERROR_UNLESS(layout.extraReadOnly);
		WAVM_ERROR_UNLESS(totalSize <= unwindInfoReserveBytes);
		U8* out = layout.extraReadOnly;
		memset(out, 0, totalSize);

		// Header.
		writeLE<U32>(out + 0, 1);               // version
		writeLE<U32>(out + 4, personalityOff);  // commonEncodingsArraySectionOffset
		writeLE<U32>(out + 8, 0);               // commonEncodingsArrayCount
		writeLE<U32>(out + 12, personalityOff); // personalityArraySectionOffset
		writeLE<U32>(
			out + 16,
			narrow<U32>("personality count", personalities.size())); // personalityArrayCount
		writeLE<U32>(out + 20, indexOff);                            // indexSectionOffset
		writeLE<U32>(out + 24, indexCount);                          // indexCount

		// Personality array (offsets to GOT entries containing personality addresses).
		// System libunwind dereferences personality array entries as pointers to read
		// the actual personality function address from a GOT entry. This matches
		// JITLink's CompactUnwindManager which uses GOTManager for personalities.
		for(Uptr pi = 0; pi < personalities.size(); ++pi)
		{
			Uptr gotEntry = getOrCreateGotEntry(layout, personalities[pi]);
			U32 persOffset = narrow<U32>("personality GOT offset", gotEntry - imageBase);
			writeLE<U32>(out + personalityOff + U32(pi) * 4, persOffset);
			Log::printf(Log::traceLinking,
						"  personality[%" WAVM_PRIuPTR "]: addr=0x%" PRIx64
						" gotEntry=0x%" WAVM_PRIxPTR " imageRelOffset=0x%x\n",
						pi,
						personalities[pi],
						gotEntry,
						persOffset);
		}

		// LSDA index.
		U32 lsdaIdx = 0;
		for(const auto& e : entries)
		{
			if(e.lsda)
			{
				U32 lsdaImgRel = narrow<U32>("LSDA offset", e.lsda - imageBase);
				writeLE<U32>(out + lsdaOff + lsdaIdx * 8, e.funcOffset);
				writeLE<U32>(out + lsdaOff + lsdaIdx * 8 + 4, lsdaImgRel);
				Log::printf(Log::traceLinking,
							"  lsda[%u]: funcOffset=0x%x lsdaAddr=0x%" PRIx64
							" lsdaImageRel=0x%x\n",
							lsdaIdx,
							e.funcOffset,
							e.lsda,
							lsdaImgRel);
				++lsdaIdx;
			}
		}

		// Build LSDA counts per page (for first-level index lsdaIndexArraySectionOffset).
		// LSDAs are sorted by funcOffset (same order as entries), so we count how many
		// LSDAs fall within each page's entry range.
		std::vector<U32> lsdaCountBeforePage(numPages + 1, 0);
		{
			U32 runningLsda = 0;
			for(U32 p = 0; p < numPages; ++p)
			{
				lsdaCountBeforePage[p] = runningLsda;
				U32 start = p * maxEntriesPerPage;
				U32 end = std::min(start + maxEntriesPerPage, numSortedEntries);
				for(U32 ei = start; ei < end; ++ei)
				{
					if(entries[ei].lsda) { ++runningLsda; }
				}
			}
			lsdaCountBeforePage[numPages] = runningLsda;
		}

		// First-level index and second-level pages.
		U32 curPageOff = pagesOff;
		U32 lastFuncEnd = 0;
		if(!entries.empty())
		{
			lastFuncEnd = entries.back().funcOffset + entries.back().funcLength;
		}
		for(U32 p = 0; p < numPages; ++p)
		{
			U32 start = p * maxEntriesPerPage;
			U32 end = std::min(start + maxEntriesPerPage, numSortedEntries);
			U32 count = end - start;
			U32 funcOff = (start < numSortedEntries) ? entries[start].funcOffset : 0;

			// First-level index entry.
			writeLE<U32>(out + indexOff + p * 12 + 0, funcOff);
			writeLE<U32>(out + indexOff + p * 12 + 4, curPageOff);
			writeLE<U32>(out + indexOff + p * 12 + 8, lsdaOff + lsdaCountBeforePage[p] * 8);

			// Second-level regular page.
			writeLE<U32>(out + curPageOff + 0, 2);          // kind = REGULAR
			writeLE<U16>(out + curPageOff + 4, U16(8));     // entryPageOffset (header is 8 bytes)
			writeLE<U16>(out + curPageOff + 6, U16(count)); // entryCount
			for(U32 ei = 0; ei < count; ++ei)
			{
				U32 off = curPageOff + 8 + ei * 8;
				writeLE<U32>(out + off, entries[start + ei].funcOffset);
				writeLE<U32>(out + off + 4, entries[start + ei].encoding);
				Log::printf(Log::traceLinking,
							"  page[%u] entry[%u]: funcOffset=0x%x encoding=0x%08x\n",
							p,
							ei,
							entries[start + ei].funcOffset,
							entries[start + ei].encoding);
			}

			curPageOff += 8 + count * 8;
		}

		// Sentinel index entry.
		writeLE<U32>(out + indexOff + numPages * 12 + 0, lastFuncEnd);
		writeLE<U32>(out + indexOff + numPages * 12 + 4, 0);
		writeLE<U32>(out + indexOff + numPages * 12 + 8,
					 lsdaOff + lsdaCountBeforePage[numPages] * 8);

		result.unwindInfo.compactUnwind = out;
		result.unwindInfo.compactUnwindNumBytes = totalSize;

		Log::printf(Log::traceLinking,
					"  Synthesized __unwind_info: %u bytes, imageBase=0x%" WAVM_PRIxPTR
					" data=0x%" WAVM_PRIxPTR "\n",
					totalSize,
					imageBase,
					Uptr(out));
		Log::printf(Log::traceLinking,
					"    header: version=1 commonEncOff=%u commonEncCnt=0"
					" persOff=%u persCnt=%u indexOff=%u indexCnt=%u\n",
					personalityOff,
					personalityOff,
					U32(personalities.size()),
					indexOff,
					indexCount);
		Log::printf(Log::traceLinking,
					"    %u entries, %u pages, %u personalities, %" WAVM_PRIuPTR " LSDAs\n",
					numSortedEntries,
					numPages,
					U32(personalities.size()),
					numLsda);
	}

	// Collect defined function symbols.
	for(U32 i = 0; i < nsyms; ++i)
	{
		const NList64 sym = symbols[i];
		if((sym.n_type & N_TYPE_MASK) != N_SECT) { continue; }
		if(!(sym.n_type & N_EXT)) { continue; }

		const char* name = strtab + sym.n_strx;
		// Demangle (strip leading underscore).
		const char* demangledName = name[0] == '_' ? name + 1 : name;

		LinkResult::Symbol resultSym;
		resultSym.name = demangledName;
		resultSym.address = symbolAddresses[i];
		// MachO doesn't store symbol sizes directly; we'll compute them from the next symbol
		// or section end. For now, set size to 0 and let the caller handle it.
		resultSym.size = 0;
		result.definedSymbols.push_back(std::move(resultSym));
	}

	// Compute symbol sizes by sorting symbols within each section and using gaps.
	struct SymWithIndex
	{
		Uptr address;
		Uptr resultIndex;
		U8 section;
	};
	std::vector<SymWithIndex> allSyms;
	for(Uptr i = 0; i < result.definedSymbols.size(); ++i)
	{
		for(U32 j = 0; j < nsyms; ++j)
		{
			if(symbolAddresses[j] == result.definedSymbols[i].address
			   && (symbols[j].n_type & N_TYPE_MASK) == N_SECT && (symbols[j].n_type & N_EXT))
			{
				allSyms.push_back({result.definedSymbols[i].address, i, symbols[j].n_sect});
				break;
			}
		}
	}
	std::sort(allSyms.begin(), allSyms.end(), [](const SymWithIndex& a, const SymWithIndex& b) {
		return a.section < b.section || (a.section == b.section && a.address < b.address);
	});
	for(Uptr i = 0; i < allSyms.size(); ++i)
	{
		Uptr nextAddr;
		if(i + 1 < allSyms.size() && allSyms[i + 1].section == allSyms[i].section)
		{
			nextAddr = allSyms[i + 1].address;
		}
		else
		{
			Uptr sectIdx = Uptr(allSyms[i].section) - 1;
			nextAddr = layout.sectionLoadAddresses[sectIdx] + sectionHeaders[sectIdx]->size;
		}
		result.definedSymbols[allSyms[i].resultIndex].size = nextAddr - allSyms[i].address;
	}

	// Protect image now that all writes (including synthesized __unwind_info) are done.
	protectImage(layout);
}

void ObjectLinker::linkMachO(U8* bytes,
							 Uptr size,
							 const HashMap<std::string, Uptr>& imports,
							 LinkResult& result,
							 PageAllocator allocatePages)
{
	if(size < sizeof(MachHeader64)) { Errors::fatalf("MachO file too small"); }

	const MachHeader64* header = reinterpret_cast<const MachHeader64*>(bytes);
	U32 magic = header->magic;
	if(magic != MH_MAGIC_64 && magic != MH_CIGAM_64) { Errors::fatalf("Not a 64-bit MachO file"); }

	const char* cpuName = "?";
	switch(header->cputype)
	{
	case CPU_TYPE_X86_64: cpuName = "x86-64"; break;
	case CPU_TYPE_ARM64: cpuName = "AArch64"; break;
	default: break;
	}
	Log::printf(
		Log::traceLinking, "--- linkMachO: %" WAVM_PRIuPTR " bytes, cpu=%s ---\n", size, cpuName);

	switch(header->cputype)
	{
	case CPU_TYPE_X86_64:
		linkMachOArch<Arch::x86_64>(bytes, size, imports, result, allocatePages);
		break;
	case CPU_TYPE_ARM64:
		linkMachOArch<Arch::aarch64>(bytes, size, imports, result, allocatePages);
		break;
	default: Errors::fatalf("Unsupported MachO CPU type: 0x%x", header->cputype);
	}
}
