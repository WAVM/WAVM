#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "TestUtils.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/ObjectLinker/ObjectLinker.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Memory.h"
#include "wavm-test.h"

using namespace WAVM;
using namespace WAVM::ObjectLinker;
using namespace WAVM::Testing;

namespace {

	// ===== Constants =====

	// COFF machine types
	static constexpr U16 IMAGE_FILE_MACHINE_AMD64 = 0x8664;
	static constexpr U16 IMAGE_FILE_MACHINE_ARM64 = 0xAA64;

	// COFF section characteristics
	static constexpr U32 IMAGE_SCN_MEM_EXECUTE = 0x20000000;

	// COFF symbol storage class
	static constexpr U8 IMAGE_SYM_CLASS_EXTERNAL = 2;

	// COFF x86-64 relocation types
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

	// COFF ARM64 relocation types
	static constexpr U16 IMAGE_REL_ARM64_ADDR64 = 0x000E;
	static constexpr U16 IMAGE_REL_ARM64_ADDR32NB = 0x0002;
	static constexpr U16 IMAGE_REL_ARM64_BRANCH26 = 0x0003;
	static constexpr U16 IMAGE_REL_ARM64_PAGEBASE_REL21 = 0x0004;
	static constexpr U16 IMAGE_REL_ARM64_PAGEOFFSET_12A = 0x0006;
	static constexpr U16 IMAGE_REL_ARM64_PAGEOFFSET_12L = 0x0007;
	static constexpr U16 IMAGE_REL_ARM64_SECREL = 0x0008;
	static constexpr U16 IMAGE_REL_ARM64_SECREL_LOW12A = 0x0009;
	static constexpr U16 IMAGE_REL_ARM64_SECREL_HIGH12A = 0x000A;
	static constexpr U16 IMAGE_REL_ARM64_SECREL_LOW12L = 0x000B;
	static constexpr U16 IMAGE_REL_ARM64_SECTION = 0x000D;

	// ELF machine types
	static constexpr U16 EM_X86_64 = 62;
	static constexpr U16 EM_AARCH64 = 183;

	// ELF section types
	static constexpr U32 SHT_PROGBITS = 1;
	static constexpr U32 SHT_SYMTAB = 2;
	static constexpr U32 SHT_STRTAB = 3;
	static constexpr U32 SHT_RELA = 4;

	// ELF section flags
	static constexpr U64 SHF_ALLOC = 0x2;
	static constexpr U64 SHF_EXECINSTR = 0x4;

	// ELF symbol binding/type
	static constexpr U8 STB_GLOBAL = 1;
	static constexpr U8 STT_FUNC = 2;

	// ELF x86-64 relocation types
	static constexpr U32 R_X86_64_64 = 1;
	static constexpr U32 R_X86_64_PC32 = 2;
	static constexpr U32 R_X86_64_PLT32 = 4;
	static constexpr U32 R_X86_64_32 = 10;
	static constexpr U32 R_X86_64_32S = 11;
	static constexpr U32 R_X86_64_GOTPCREL = 9;
	static constexpr U32 R_X86_64_PC64 = 24;
	static constexpr U32 R_X86_64_GOTPCRELX = 41;
	static constexpr U32 R_X86_64_REX_GOTPCRELX = 42;

	// ELF AArch64 relocation types
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

	// Mach-O constants
	static constexpr U32 MH_MAGIC_64 = 0xFEEDFACF;
	static constexpr U32 LC_SEGMENT_64 = 0x19;
	static constexpr U32 LC_SYMTAB = 0x02;
	static constexpr U32 CPU_TYPE_X86_64 = 0x01000007;
	static constexpr U32 CPU_TYPE_ARM64 = 0x0100000C;
	static constexpr U8 N_EXT = 0x01;
	static constexpr U8 N_SECT = 0x0E;
	static constexpr U32 S_ATTR_PURE_INSTRUCTIONS = 0x80000000;

	// Mach-O x86-64 relocation types
	static constexpr U32 X86_64_RELOC_UNSIGNED = 0;
	static constexpr U32 X86_64_RELOC_SIGNED = 1;
	static constexpr U32 X86_64_RELOC_BRANCH = 2;
	static constexpr U32 X86_64_RELOC_GOT_LOAD = 3;
	static constexpr U32 X86_64_RELOC_GOT = 4;
	static constexpr U32 X86_64_RELOC_SIGNED_1 = 6;
	static constexpr U32 X86_64_RELOC_SIGNED_2 = 7;
	static constexpr U32 X86_64_RELOC_SIGNED_4 = 8;

	// Mach-O AArch64 relocation types
	static constexpr U32 ARM64_RELOC_UNSIGNED = 0;
	static constexpr U32 ARM64_RELOC_BRANCH26 = 2;
	static constexpr U32 ARM64_RELOC_PAGE21 = 3;
	static constexpr U32 ARM64_RELOC_PAGEOFF12 = 4;
	static constexpr U32 ARM64_RELOC_GOT_LOAD_PAGE21 = 5;
	static constexpr U32 ARM64_RELOC_GOT_LOAD_PAGEOFF12 = 6;
	static constexpr U32 ARM64_RELOC_POINTER_TO_GOT = 7;
	static constexpr U32 ARM64_RELOC_ADDEND = 10;

	// AArch64 instruction templates
	static constexpr U32 adrpX0 = 0x90000000;
	static constexpr U32 addX0X0Imm0 = 0x91000000;
	static constexpr U32 ldrX0X0Imm0 = 0xF9400000;
	static constexpr U32 ldrX0X0Imm8 = 0xF9400400;
	static constexpr U32 strX1X0Imm0 = 0xF9000001;
	static constexpr U32 ldrW0X0Imm0 = 0xB9400000;
	static constexpr U32 nop = 0xD503201F;
	static constexpr U32 ret = 0xD65F03C0;
	static constexpr U32 blTarget0 = 0x94000000;
	static constexpr U32 bTarget0 = 0x14000000;
	static constexpr U32 movzX0Imm0 = 0xD2800000;
	static constexpr U32 movkX0Imm0Lsl16 = 0xF2A00000;
	static constexpr U32 movkX0Imm0Lsl32 = 0xF2C00000;
	static constexpr U32 movkX0Imm0Lsl48 = 0xF2E00000;
	static constexpr U32 ldrbW0X0Imm0 = 0x39400000;
	static constexpr U32 ldrhW0X0Imm0 = 0x79400000;
	static constexpr U32 ldrQ0X0Imm0 = 0x3DC00000;

	// Import addresses
	static constexpr Uptr nearImportedAddress = 0x1000;
	static constexpr U64 farImportedAddress = 0x10000000000ull;

	// ===== Struct definitions =====

	WAVM_PACKED_STRUCT(struct CoffFileHeader {
		U16 Machine;
		U16 NumberOfSections;
		U32 TimeDateStamp;
		U32 PointerToSymbolTable;
		U32 NumberOfSymbols;
		U16 SizeOfOptionalHeader;
		U16 Characteristics;
	});

	WAVM_PACKED_STRUCT(struct CoffSectionHeader {
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
	});

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

	WAVM_PACKED_STRUCT(struct CoffRelocation {
		U32 VirtualAddress;
		U32 SymbolTableIndex;
		U16 Type;
	});

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

	struct TestMachOSection64
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

	struct TestSymtabCommand
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

	struct TestMachOReloc
	{
		I32 r_address;
		U32 r_info;
	};

	static_assert(sizeof(CoffFileHeader) == 20, "");
	static_assert(sizeof(CoffSectionHeader) == 40, "");
	static_assert(sizeof(CoffSymbol) == 18, "");
	static_assert(sizeof(CoffRelocation) == 10, "");
	static_assert(sizeof(Elf64_Ehdr) == 64, "");
	static_assert(sizeof(Elf64_Shdr) == 64, "");
	static_assert(sizeof(Elf64_Sym) == 24, "");
	static_assert(sizeof(Elf64_Rela) == 24, "");
	static_assert(sizeof(MachHeader64) == 32, "");
	static_assert(sizeof(SegmentCommand64) == 72, "");
	static_assert(sizeof(TestMachOSection64) == 80, "");
	static_assert(sizeof(TestSymtabCommand) == 24, "");
	static_assert(sizeof(NList64) == 16, "");
	static_assert(sizeof(TestMachOReloc) == 8, "");

	// ===== Helpers =====

	using Serialization::ArrayOutputStream;
	using Serialization::serializeBytes;

	// Write a struct or scalar value to a serialization stream.
	template<typename T> static void write(Serialization::OutputStream& stream, const T& value)
	{
		serializeBytes(stream, (const U8*)&value, sizeof(T));
	}

	static U32 readU32(const U8* bytes)
	{
		U32 value;
		memcpy(&value, bytes, sizeof(value));
		return value;
	}

	static U64 readU64(const U8* bytes)
	{
		U64 value;
		memcpy(&value, bytes, sizeof(value));
		return value;
	}

	static I32 readI32(const U8* bytes)
	{
		I32 value;
		memcpy(&value, bytes, sizeof(value));
		return value;
	}

	static I64 readI64(const U8* bytes)
	{
		I64 value;
		memcpy(&value, bytes, sizeof(value));
		return value;
	}

	static U16 readU16(const U8* bytes)
	{
		U16 value;
		memcpy(&value, bytes, sizeof(value));
		return value;
	}

	static std::vector<U8> arm64Code(std::initializer_list<U32> insns)
	{
		ArrayOutputStream stream;
		for(U32 insn : insns) { write(stream, insn); }
		return stream.getBytes();
	}

	static std::vector<U8> arm64Code(const std::vector<U32>& insns)
	{
		ArrayOutputStream stream;
		for(U32 insn : insns) { write(stream, insn); }
		return stream.getBytes();
	}

	static U32 makeMachORelocInfo(U32 symbolnum, bool pcrel, U32 length, bool isExtern, U32 type)
	{
		return (symbolnum & 0x00FFFFFF) | (U32(pcrel) << 24) | ((length & 3) << 25)
			   | (U32(isExtern) << 27) | ((type & 0xF) << 28);
	}

	// ===== Object builders =====

	// Build a minimal COFF object with one .text section.
	// Symbol table: [0..N-1] = imported symbols, [N] = "func" at offset 0 in section 1.
	static std::vector<U8> makeCOFFObject(U16 machine,
										  const std::vector<U8>& code,
										  const std::vector<CoffRelocation>& relocs,
										  const std::vector<std::string>& importNames)
	{
		U32 numImports = U32(importNames.size());
		U32 numSymbols = numImports + 1;
		U32 codeSize = U32(code.size());
		U32 numRelocs = U32(relocs.size());

		U32 pointerToRawData = sizeof(CoffFileHeader) + sizeof(CoffSectionHeader);
		U32 pointerToRelocations = pointerToRawData + codeSize;
		U32 pointerToSymbolTable = pointerToRelocations + numRelocs * sizeof(CoffRelocation);

		CoffFileHeader fileHeader = {};
		fileHeader.Machine = machine;
		fileHeader.NumberOfSections = 1;
		fileHeader.PointerToSymbolTable = pointerToSymbolTable;
		fileHeader.NumberOfSymbols = numSymbols;

		CoffSectionHeader sectionHeader = {};
		memcpy(sectionHeader.Name, ".text", 5);
		sectionHeader.SizeOfRawData = codeSize;
		sectionHeader.PointerToRawData = pointerToRawData;
		sectionHeader.PointerToRelocations = pointerToRelocations;
		sectionHeader.NumberOfRelocations = U16(numRelocs);
		sectionHeader.Characteristics = IMAGE_SCN_MEM_EXECUTE;

		ArrayOutputStream stream;
		write(stream, fileHeader);
		write(stream, sectionHeader);
		serializeBytes(stream, code.data(), code.size());
		for(const auto& rel : relocs) { write(stream, rel); }

		for(U32 i = 0; i < numImports; ++i)
		{
			CoffSymbol sym = {};
			const auto& name = importNames[i];
			if(name.size() <= 8) { memcpy(sym.Name.ShortName, name.c_str(), name.size()); }
			sym.SectionNumber = 0;
			sym.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
			write(stream, sym);
		}

		CoffSymbol funcSym = {};
		memcpy(funcSym.Name.ShortName, "func", 4);
		funcSym.SectionNumber = 1;
		funcSym.Type = 0x20;
		funcSym.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
		write(stream, funcSym);

		write(stream, U32(4)); // empty string table
		return stream.getBytes();
	}

	// COFF builder with a "target" symbol defined at a given offset in section 1.
	// Symbol table: [0] = "target" in section 1 at targetValue, [1] = "func" in section 1 at 0.
	static std::vector<U8> makeCOFFObjectWithTarget(U16 machine,
													const std::vector<U8>& code,
													const std::vector<CoffRelocation>& relocs,
													U32 targetValue)
	{
		U32 numSymbols = 2;
		U32 codeSize = U32(code.size());
		U32 numRelocs = U32(relocs.size());

		U32 pointerToRawData = sizeof(CoffFileHeader) + sizeof(CoffSectionHeader);
		U32 pointerToRelocations = pointerToRawData + codeSize;
		U32 pointerToSymbolTable = pointerToRelocations + numRelocs * sizeof(CoffRelocation);

		CoffFileHeader fileHeader = {};
		fileHeader.Machine = machine;
		fileHeader.NumberOfSections = 1;
		fileHeader.PointerToSymbolTable = pointerToSymbolTable;
		fileHeader.NumberOfSymbols = numSymbols;

		CoffSectionHeader sectionHeader = {};
		memcpy(sectionHeader.Name, ".text", 5);
		sectionHeader.SizeOfRawData = codeSize;
		sectionHeader.PointerToRawData = pointerToRawData;
		sectionHeader.PointerToRelocations = pointerToRelocations;
		sectionHeader.NumberOfRelocations = U16(numRelocs);
		sectionHeader.Characteristics = IMAGE_SCN_MEM_EXECUTE;

		ArrayOutputStream stream;
		write(stream, fileHeader);
		write(stream, sectionHeader);
		serializeBytes(stream, code.data(), code.size());
		for(const auto& rel : relocs) { write(stream, rel); }

		CoffSymbol targetSym = {};
		memcpy(targetSym.Name.ShortName, "target", 6);
		targetSym.Value = targetValue;
		targetSym.SectionNumber = 1;
		targetSym.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
		write(stream, targetSym);

		CoffSymbol funcSym = {};
		memcpy(funcSym.Name.ShortName, "func", 4);
		funcSym.SectionNumber = 1;
		funcSym.Type = 0x20;
		funcSym.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
		write(stream, funcSym);

		write(stream, U32(4)); // empty string table
		return stream.getBytes();
	}

	// Build a minimal ELF relocatable object with one .text section.
	// Symbols: [0]=null, [1..N]=imports, [N+1]="func".
	static std::vector<U8> makeELFObject(U16 machine,
										 const std::vector<U8>& code,
										 const std::vector<Elf64_Rela>& relas,
										 const std::vector<std::string>& importNames)
	{
		U32 numImports = U32(importNames.size());
		U32 numSymbols = numImports + 2; // null + imports + func
		U32 codeSize = U32(code.size());
		U32 numRelas = U32(relas.size());

		// Build .strtab: "\0" + import names + "func\0"
		std::vector<U8> strtab;
		strtab.push_back(0);
		std::vector<U32> importNameOffsets;
		for(const auto& name : importNames)
		{
			importNameOffsets.push_back(U32(strtab.size()));
			for(char c : name) { strtab.push_back(U8(c)); }
			strtab.push_back(0);
		}
		U32 funcNameOffset = U32(strtab.size());
		for(char c : std::string("func")) { strtab.push_back(U8(c)); }
		strtab.push_back(0);

		// .shstrtab: section name strings
		// "\0.text\0.rela.text\0.symtab\0.strtab\0.shstrtab\0"
		const char shstrtabData[] = "\0.text\0.rela.text\0.symtab\0.strtab\0.shstrtab";
		U32 shstrtabSize = sizeof(shstrtabData); // includes trailing \0
		// Section name offsets:
		U32 nameText = 1;
		U32 nameRelaText = 7;
		U32 nameSymtab = 18;
		U32 nameStrtab = 26;
		U32 nameShstrtab = 34;

		// Calculate sizes and offsets. Align struct sections to 8 bytes
		// (Elf64_Rela, Elf64_Sym, Elf64_Shdr all have 8-byte fields).
		auto align8 = [](U32 v) -> U32 { return (v + 7) & ~U32(7); };
		U32 relaSize = numRelas * sizeof(Elf64_Rela);
		U32 symtabSize = numSymbols * sizeof(Elf64_Sym);
		U32 strtabSize = U32(strtab.size());

		U32 textOff = sizeof(Elf64_Ehdr);
		U32 relaOff = align8(textOff + codeSize);
		U32 symtabOff = align8(relaOff + relaSize);
		U32 strtabOff = symtabOff + symtabSize; // strtab is bytes, no alignment needed
		U32 shstrtabOff = strtabOff + strtabSize;
		U32 shdrOff = align8(shstrtabOff + shstrtabSize);

		// Build the ELF file
		ArrayOutputStream stream;

		// ELF header
		Elf64_Ehdr ehdr = {};
		ehdr.e_ident[0] = 0x7f;
		ehdr.e_ident[1] = 'E';
		ehdr.e_ident[2] = 'L';
		ehdr.e_ident[3] = 'F';
		ehdr.e_ident[4] = 2; // ELFCLASS64
		ehdr.e_ident[5] = 1; // ELFDATA2LSB
		ehdr.e_ident[6] = 1; // EV_CURRENT
		ehdr.e_type = 1;     // ET_REL
		ehdr.e_machine = machine;
		ehdr.e_version = 1;
		ehdr.e_shoff = shdrOff;
		ehdr.e_ehsize = sizeof(Elf64_Ehdr);
		ehdr.e_shentsize = sizeof(Elf64_Shdr);
		ehdr.e_shnum = 6;
		ehdr.e_shstrndx = 5;
		write(stream, ehdr);

		// .text data
		serializeBytes(stream, code.data(), code.size());

		// .rela.text data
		serializeZerosUntilOffset(stream, relaOff);
		for(const auto& rela : relas) { write(stream, rela); }

		// .symtab data
		serializeZerosUntilOffset(stream, symtabOff);
		Elf64_Sym nullSym = {};
		write(stream, nullSym);
		for(U32 i = 0; i < numImports; ++i)
		{
			Elf64_Sym sym = {};
			sym.st_name = importNameOffsets[i];
			sym.st_info = (STB_GLOBAL << 4);
			sym.st_shndx = 0; // SHN_UNDEF
			write(stream, sym);
		}
		Elf64_Sym funcSym = {};
		funcSym.st_name = funcNameOffset;
		funcSym.st_info = (STB_GLOBAL << 4) | STT_FUNC;
		funcSym.st_shndx = 1; // .text section
		funcSym.st_value = 0;
		funcSym.st_size = codeSize;
		write(stream, funcSym);

		// .strtab data
		serializeBytes(stream, strtab.data(), strtab.size());

		// .shstrtab data
		serializeBytes(stream, (const U8*)shstrtabData, shstrtabSize);

		// Section headers
		serializeZerosUntilOffset(stream, shdrOff);
		Elf64_Shdr nullShdr = {};
		write(stream, nullShdr);

		Elf64_Shdr textShdr = {};
		textShdr.sh_name = nameText;
		textShdr.sh_type = SHT_PROGBITS;
		textShdr.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
		textShdr.sh_offset = textOff;
		textShdr.sh_size = codeSize;
		textShdr.sh_addralign = 16;
		write(stream, textShdr);

		Elf64_Shdr relaShdr = {};
		relaShdr.sh_name = nameRelaText;
		relaShdr.sh_type = SHT_RELA;
		relaShdr.sh_offset = relaOff;
		relaShdr.sh_size = relaSize;
		relaShdr.sh_link = 3; // .symtab
		relaShdr.sh_info = 1; // .text
		relaShdr.sh_addralign = 8;
		relaShdr.sh_entsize = sizeof(Elf64_Rela);
		write(stream, relaShdr);

		Elf64_Shdr symtabShdr = {};
		symtabShdr.sh_name = nameSymtab;
		symtabShdr.sh_type = SHT_SYMTAB;
		symtabShdr.sh_offset = symtabOff;
		symtabShdr.sh_size = symtabSize;
		symtabShdr.sh_link = 4; // .strtab
		symtabShdr.sh_info = 1; // first non-local symbol index
		symtabShdr.sh_addralign = 8;
		symtabShdr.sh_entsize = sizeof(Elf64_Sym);
		write(stream, symtabShdr);

		Elf64_Shdr strtabShdr = {};
		strtabShdr.sh_name = nameStrtab;
		strtabShdr.sh_type = SHT_STRTAB;
		strtabShdr.sh_offset = strtabOff;
		strtabShdr.sh_size = strtabSize;
		strtabShdr.sh_addralign = 1;
		write(stream, strtabShdr);

		Elf64_Shdr shstrtabShdr = {};
		shstrtabShdr.sh_name = nameShstrtab;
		shstrtabShdr.sh_type = SHT_STRTAB;
		shstrtabShdr.sh_offset = shstrtabOff;
		shstrtabShdr.sh_size = shstrtabSize;
		shstrtabShdr.sh_addralign = 1;
		write(stream, shstrtabShdr);

		return stream.getBytes();
	}

	// Build a minimal Mach-O object with one __TEXT,__text section.
	// Symbols: [0..N-1] = imports (names have leading _), [N] = "_func".
	// Import map keys should NOT have leading underscore (linker strips it).
	static std::vector<U8> makeMachOObject(U32 cputype,
										   const std::vector<U8>& code,
										   const std::vector<TestMachOReloc>& relocs,
										   const std::vector<std::string>& importNames)
	{
		U32 numImports = U32(importNames.size());
		U32 numSymbols = numImports + 1;
		U32 codeSize = U32(code.size());
		U32 numRelocs = U32(relocs.size());

		// Build string table: "\0_name1\0_name2\0..._func\0"
		std::vector<U8> strtab;
		strtab.push_back(0);
		std::vector<U32> importStrOffsets;
		for(const auto& name : importNames)
		{
			importStrOffsets.push_back(U32(strtab.size()));
			strtab.push_back('_');
			for(char c : name) { strtab.push_back(U8(c)); }
			strtab.push_back(0);
		}
		U32 funcStrOffset = U32(strtab.size());
		for(char c : std::string("_func")) { strtab.push_back(U8(c)); }
		strtab.push_back(0);

		U32 relocSize = numRelocs * U32(sizeof(TestMachOReloc));
		U32 symtabSize = numSymbols * U32(sizeof(NList64));
		U32 strtabSize = U32(strtab.size());

		U32 dataStart = U32(sizeof(MachHeader64) + sizeof(SegmentCommand64)
							+ sizeof(TestMachOSection64) + sizeof(TestSymtabCommand));
		U32 codeOff = dataStart;
		U32 relocOff = codeOff + codeSize;
		U32 symtabOff = relocOff + relocSize;
		U32 strtabOff = symtabOff + symtabSize;

		ArrayOutputStream stream;

		// Mach-O header
		MachHeader64 hdr = {};
		hdr.magic = MH_MAGIC_64;
		hdr.cputype = cputype;
		hdr.filetype = 1; // MH_OBJECT
		hdr.ncmds = 2;
		hdr.sizeofcmds = U32(sizeof(SegmentCommand64) + sizeof(TestMachOSection64)
							 + sizeof(TestSymtabCommand));
		write(stream, hdr);

		// LC_SEGMENT_64
		SegmentCommand64 seg = {};
		seg.cmd = LC_SEGMENT_64;
		seg.cmdsize = U32(sizeof(SegmentCommand64) + sizeof(TestMachOSection64));
		seg.vmsize = codeSize;
		seg.fileoff = codeOff;
		seg.filesize = codeSize;
		seg.maxprot = 7;
		seg.initprot = 7;
		seg.nsects = 1;
		write(stream, seg);

		// Section
		TestMachOSection64 sect = {};
		memcpy(sect.sectname, "__text", 6);
		memcpy(sect.segname, "__TEXT", 6);
		sect.size = codeSize;
		sect.offset = codeOff;
		sect.align = 2; // 2^2 = 4 byte alignment
		sect.reloff = relocOff;
		sect.nreloc = numRelocs;
		sect.flags = S_ATTR_PURE_INSTRUCTIONS;
		write(stream, sect);

		// LC_SYMTAB
		TestSymtabCommand symcmd = {};
		symcmd.cmd = LC_SYMTAB;
		symcmd.cmdsize = sizeof(TestSymtabCommand);
		symcmd.symoff = symtabOff;
		symcmd.nsyms = numSymbols;
		symcmd.stroff = strtabOff;
		symcmd.strsize = strtabSize;
		write(stream, symcmd);

		// Code
		serializeBytes(stream, code.data(), code.size());

		// Relocations
		for(const auto& rel : relocs) { write(stream, rel); }

		// Symbol table
		for(U32 i = 0; i < numImports; ++i)
		{
			NList64 sym = {};
			sym.n_strx = importStrOffsets[i];
			sym.n_type = N_EXT; // N_UNDF | N_EXT
			write(stream, sym);
		}
		NList64 funcSym = {};
		funcSym.n_strx = funcStrOffset;
		funcSym.n_type = N_SECT | N_EXT;
		funcSym.n_sect = 1;
		write(stream, funcSym);

		// String table
		serializeBytes(stream, strtab.data(), strtab.size());

		return stream.getBytes();
	}

	// ===== RAII wrapper and import helpers =====

	// RAII wrapper that pre-allocates image pages so import addresses can be computed
	// relative to a known base. All tests use link() to link into the pre-allocated pages.
	struct LinkedImage
	{
		static constexpr Uptr reservePages = 16;
		LinkResult result;

		// Address near the image (just past the reserved region), for relative relocation tests.
		Uptr nearAddr;

		LinkedImage()
		{
			preAllocBase_ = Platform::allocateVirtualPages(reservePages);
			WAVM_ERROR_UNLESS(preAllocBase_);
			WAVM_ERROR_UNLESS(Platform::commitVirtualPages(preAllocBase_, reservePages));
			nearAddr = reinterpret_cast<Uptr>(preAllocBase_)
					   + reservePages * Platform::getBytesPerPage() + 0x1000;
		}

		void link(U8* objectBytes, Uptr objectNumBytes, const HashMap<std::string, Uptr>& imports)
		{
			linkObject(objectBytes, objectNumBytes, imports, result, [this](Uptr numPages) -> U8* {
				WAVM_ERROR_UNLESS(numPages <= reservePages);
				return preAllocBase_;
			});
		}

		~LinkedImage()
		{
			if(preAllocBase_) { Platform::freeVirtualPages(preAllocBase_, reservePages); }
		}

	private:
		U8* preAllocBase_ = nullptr;
	};

	static HashMap<std::string, Uptr> makeFarImportMap()
	{
		HashMap<std::string, Uptr> imports;
		imports.addOrFail("imported", Uptr(farImportedAddress));
		return imports;
	}

	static HashMap<std::string, Uptr> makeNearImportMap(Uptr addr)
	{
		HashMap<std::string, Uptr> imports;
		imports.addOrFail("imported", addr);
		return imports;
	}

	static HashMap<std::string, Uptr> makeImportMap(
		const std::vector<std::pair<std::string, Uptr>>& entries)
	{
		HashMap<std::string, Uptr> imports;
		for(const auto& e : entries) { imports.addOrFail(e.first, e.second); }
		return imports;
	}

	// Helper: get code pointer from linked image.
	// Caller should CHECK_EQ(definedSymbols.size(), ...) separately for diagnostic quality.
	static const U8* getCodePtr(const LinkedImage& img)
	{
		WAVM_ERROR_UNLESS(img.result.definedSymbols.size() >= 1);
		return reinterpret_cast<const U8*>(img.result.definedSymbols[0].address);
	}

	// =====================================================================
	// COFF x86-64 tests
	// =====================================================================

	static void testCOFFX64Addr64(TEST_STATE_PARAM)
	{
		std::vector<U8> code(8, 0);
		code.push_back(0xC3); // ret
		CoffRelocation rel = {0, 0, IMAGE_REL_AMD64_ADDR64};
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_AMD64, code, {rel}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU64(p), U64(nearImportedAddress));
	}

	static void testCOFFX64Addr32NB(TEST_STATE_PARAM)
	{
		// ADDR32NB: writes symbolValue + addend - imageBase as I32.
		// Fixup bytes must be 0 (inline addend). Use a defined symbol to guarantee it fits.
		std::vector<U8> code(20, 0);
		CoffRelocation rel = {0, 0, IMAGE_REL_AMD64_ADDR32NB}; // targets "target" (sym 0)
		auto obj = makeCOFFObjectWithTarget(IMAGE_FILE_MACHINE_AMD64, code, {rel}, 16);
		HashMap<std::string, Uptr> imports;
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr imageBase = Uptr(img.result.imageBase);
		// "func" (sym 1) is at offset 0 = definedSymbols[0].address.
		// "target" (sym 0) is at offset 16 = definedSymbols[0].address + 16.
		Uptr targetAddr = img.result.definedSymbols[0].address + 16;
		I32 expected = I32(Iptr(targetAddr) - Iptr(imageBase));
		CHECK_EQ(readI32(p), expected);
	}

	static void testCOFFX64Rel32Near(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		CoffRelocation rel = {0, 0, IMAGE_REL_AMD64_REL32};
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_AMD64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		// COFF REL32: value = sym + (inline_addend + 4), delta = value - (fixupAddr + 4)
		// inline_addend = 0, so delta = (sym + 4) - (fixupAddr + 4) = sym - fixupAddr
		I32 expected = I32(Iptr(img.nearAddr) - Iptr(fixupAddr));
		CHECK_EQ(readI32(p), expected);
	}

	static void testCOFFX64Rel32Far(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		CoffRelocation rel = {0, 0, IMAGE_REL_AMD64_REL32};
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_AMD64, code, {rel}, {"imported"});
		auto imports = makeFarImportMap();
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		// Follow the rel32 displacement to find the stub.
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		// COFF REL32: value = sym + (inline_addend + 4), delta = value - (fixupAddr + 4)
		// delta = stub - (fixupAddr + 4), so stub = fixupAddr + 4 + disp
		const U8* stub = reinterpret_cast<const U8*>(fixupAddr + 4 + disp);
		CHECK_EQ(stub[0], U8(0xFF));
		CHECK_EQ(stub[1], U8(0x25));
	}

	// Parameterized helper for REL32_1 through REL32_5.
	static void testCOFFX64Rel32Variant(TEST_STATE_PARAM, U16 relocType, I64 extraAddend)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		CoffRelocation rel = {0, 0, relocType};
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_AMD64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		// REL32_N: value = sym + (inline_addend + N+4), delta = value - (fixupAddr + 4)
		// inline_addend = 0, so delta = sym + extraAddend - fixupAddr - 4
		I32 expected = I32(Iptr(img.nearAddr) + extraAddend - Iptr(fixupAddr) - 4);
		CHECK_EQ(readI32(p), expected);
	}

	static void testCOFFX64Rel32_1(TEST_STATE_PARAM)
	{
		testCOFFX64Rel32Variant(TEST_STATE_ARG, IMAGE_REL_AMD64_REL32_1, 5);
	}
	static void testCOFFX64Rel32_2(TEST_STATE_PARAM)
	{
		testCOFFX64Rel32Variant(TEST_STATE_ARG, IMAGE_REL_AMD64_REL32_2, 6);
	}
	static void testCOFFX64Rel32_3(TEST_STATE_PARAM)
	{
		testCOFFX64Rel32Variant(TEST_STATE_ARG, IMAGE_REL_AMD64_REL32_3, 7);
	}
	static void testCOFFX64Rel32_4(TEST_STATE_PARAM)
	{
		testCOFFX64Rel32Variant(TEST_STATE_ARG, IMAGE_REL_AMD64_REL32_4, 8);
	}
	static void testCOFFX64Rel32_5(TEST_STATE_PARAM)
	{
		testCOFFX64Rel32Variant(TEST_STATE_ARG, IMAGE_REL_AMD64_REL32_5, 9);
	}

	static void testCOFFX64Section(TEST_STATE_PARAM)
	{
		std::vector<U8> code(20, 0x90);                       // 20 NOP bytes
		CoffRelocation rel = {0, 0, IMAGE_REL_AMD64_SECTION}; // targets "target" (sym 0)
		auto obj = makeCOFFObjectWithTarget(IMAGE_FILE_MACHINE_AMD64, code, {rel}, 16);
		HashMap<std::string, Uptr> imports;
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		// "func" is sym 1 at offset 0. Find code via definedSymbols.
		// "target" is in section 1, so SECTION writes 1.
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU16(p), U16(1));
	}

	static void testCOFFX64SecRel(TEST_STATE_PARAM)
	{
		std::vector<U8> code(20, 0);
		CoffRelocation rel = {0, 0, IMAGE_REL_AMD64_SECREL}; // targets "target" (sym 0)
		auto obj = makeCOFFObjectWithTarget(IMAGE_FILE_MACHINE_AMD64, code, {rel}, 16);
		HashMap<std::string, Uptr> imports;
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		// target is at value=16 within section 1, so secrel = 16
		CHECK_EQ(readI32(p), I32(16));
	}

	// =====================================================================
	// COFF ARM64 tests: existing tests
	// =====================================================================

	static constexpr Uptr rawCodeSize = 12;

	static std::vector<U8> makeARM64COFFObject(U16 pageOffsetRelocType, U32 secondInstruction)
	{
		const U32 pointerToRawData = sizeof(CoffFileHeader) + sizeof(CoffSectionHeader);
		const U32 pointerToRelocations = pointerToRawData + rawCodeSize;
		const U32 pointerToSymbolTable = pointerToRelocations + sizeof(CoffRelocation) * 2;

		CoffFileHeader fileHeader = {};
		fileHeader.Machine = IMAGE_FILE_MACHINE_ARM64;
		fileHeader.NumberOfSections = 1;
		fileHeader.PointerToSymbolTable = pointerToSymbolTable;
		fileHeader.NumberOfSymbols = 2;

		CoffSectionHeader sectionHeader = {};
		memcpy(sectionHeader.Name, ".text", 5);
		sectionHeader.SizeOfRawData = rawCodeSize;
		sectionHeader.PointerToRawData = pointerToRawData;
		sectionHeader.PointerToRelocations = pointerToRelocations;
		sectionHeader.NumberOfRelocations = 2;
		sectionHeader.Characteristics = IMAGE_SCN_MEM_EXECUTE;

		CoffRelocation pageBaseReloc = {};
		pageBaseReloc.VirtualAddress = 0;
		pageBaseReloc.SymbolTableIndex = 0;
		pageBaseReloc.Type = IMAGE_REL_ARM64_PAGEBASE_REL21;

		CoffRelocation pageOffsetReloc = {};
		pageOffsetReloc.VirtualAddress = 4;
		pageOffsetReloc.SymbolTableIndex = 0;
		pageOffsetReloc.Type = pageOffsetRelocType;

		CoffSymbol importedSymbol = {};
		memcpy(importedSymbol.Name.ShortName, "imported", 8);
		importedSymbol.SectionNumber = 0;
		importedSymbol.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;

		CoffSymbol functionSymbol = {};
		memcpy(functionSymbol.Name.ShortName, "func", 4);
		functionSymbol.SectionNumber = 1;
		functionSymbol.Type = 0x20;
		functionSymbol.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;

		ArrayOutputStream stream;
		write(stream, fileHeader);
		write(stream, sectionHeader);
		write(stream, adrpX0);
		write(stream, secondInstruction);
		write(stream, ret);
		write(stream, pageBaseReloc);
		write(stream, pageOffsetReloc);
		write(stream, importedSymbol);
		write(stream, functionSymbol);
		write(stream, U32(4)); // Empty COFF string table.
		return stream.getBytes();
	}

	static void testCOFFARM64OutOfRangePageOffset12A(TEST_STATE_PARAM)
	{
		std::vector<U8> objectBytes
			= makeARM64COFFObject(IMAGE_REL_ARM64_PAGEOFFSET_12A, addX0X0Imm0);
		HashMap<std::string, Uptr> imports = makeFarImportMap();

		LinkedImage image;
		image.link(objectBytes.data(), objectBytes.size(), imports);

		CHECK_EQ(image.result.definedSymbols.size(), Uptr(1));
		CHECK_EQ(image.result.definedSymbols[0].name, std::string("func"));

		const U8* code = reinterpret_cast<const U8*>(image.result.definedSymbols[0].address);

		// The ADD should have been transformed to a 64-bit LDR from the GOT entry.
		U32 secondInsn = readU32(code + 4);
		CHECK_EQ(secondInsn & 0xFFC00000, U32(0xF9400000));

		// RET should be unchanged.
		CHECK_EQ(readU32(code + 8), ret);
	}

	static void testCOFFARM64OutOfRangePageOffset12L(TEST_STATE_PARAM)
	{
		std::vector<U8> objectBytes
			= makeARM64COFFObject(IMAGE_REL_ARM64_PAGEOFFSET_12L, ldrX0X0Imm0);
		HashMap<std::string, Uptr> imports = makeFarImportMap();

		LinkedImage image;
		image.link(objectBytes.data(), objectBytes.size(), imports);

		CHECK_EQ(image.result.definedSymbols.size(), Uptr(1));
		CHECK_EQ(image.result.definedSymbols[0].name, std::string("func"));

		const U8* code = reinterpret_cast<const U8*>(image.result.definedSymbols[0].address);

		// The ADRP should have been replaced with a B to the data-access stub.
		U32 firstInsn = readU32(code);
		CHECK_EQ(firstInsn & 0xFC000000, U32(0x14000000));

		// The LDR should have been replaced with a NOP.
		CHECK_EQ(readU32(code + 4), nop);

		// RET should be unchanged.
		CHECK_EQ(readU32(code + 8), ret);

		// Follow the B to find the stub and verify its contents.
		I32 branchImm26 = I32(firstInsn << 6) >> 6; // sign-extend 26-bit offset
		const U8* stub = code + I64(branchImm26) * 4;

		// Stub[+0]: LDR X16, [PC, #12]
		CHECK_EQ(readU32(stub), U32(0x58000070));
		// Stub[+4]: LDR X0, [X16, #0] (original LDR with Rn=X16, imm12=0)
		CHECK_EQ(readU32(stub + 4), U32(0xF9400200));
		// Stub[+8]: B back to the RET instruction
		U32 returnInsn = readU32(stub + 8);
		CHECK_EQ(returnInsn & 0xFC000000, U32(0x14000000));
		// Stub[+12]: .quad farImportedAddress
		CHECK_EQ(readU64(stub + 12), farImportedAddress);
	}
	static void testCOFFARM64OutOfRangePageOffset12LWithAddend(TEST_STATE_PARAM)
	{
		// ADRP X0, imported; LDR X0, [X0, #8]: the addend should be folded into the stub literal.
		std::vector<U8> objectBytes
			= makeARM64COFFObject(IMAGE_REL_ARM64_PAGEOFFSET_12L, ldrX0X0Imm8);
		HashMap<std::string, Uptr> imports = makeFarImportMap();

		LinkedImage image;
		image.link(objectBytes.data(), objectBytes.size(), imports);

		const U8* code = reinterpret_cast<const U8*>(image.result.definedSymbols[0].address);

		// Should be rewritten to B stub; NOP; RET.
		U32 firstInsn = readU32(code);
		CHECK_EQ(firstInsn & 0xFC000000, U32(0x14000000));
		CHECK_EQ(readU32(code + 4), nop);

		// The stub's inline literal should be farImportedAddress + 8.
		I32 branchImm26 = I32(firstInsn << 6) >> 6;
		const U8* stub = code + I64(branchImm26) * 4;
		CHECK_EQ(readU64(stub + 12), farImportedAddress + 8);

		// The stub's LDR should use X16 as base with imm12=0.
		CHECK_EQ(readU32(stub + 4), U32(0xF9400200));
	}

	static void testCOFFARM64OutOfRangePageOffset12LStore(TEST_STATE_PARAM)
	{
		// ADRP X0, imported; STR X1, [X0, #0]: store via data-access stub.
		std::vector<U8> objectBytes
			= makeARM64COFFObject(IMAGE_REL_ARM64_PAGEOFFSET_12L, strX1X0Imm0);
		HashMap<std::string, Uptr> imports = makeFarImportMap();

		LinkedImage image;
		image.link(objectBytes.data(), objectBytes.size(), imports);

		const U8* code = reinterpret_cast<const U8*>(image.result.definedSymbols[0].address);

		U32 firstInsn = readU32(code);
		CHECK_EQ(firstInsn & 0xFC000000, U32(0x14000000));
		CHECK_EQ(readU32(code + 4), nop);

		I32 branchImm26 = I32(firstInsn << 6) >> 6;
		const U8* stub = code + I64(branchImm26) * 4;
		CHECK_EQ(readU64(stub + 12), farImportedAddress);

		// The stub's modified STR should be STR X1, [X16, #0]:
		CHECK_EQ(readU32(stub + 4), U32(0xF9000201));
	}

	static void testCOFFARM64OutOfRangePageOffset12L32Bit(TEST_STATE_PARAM)
	{
		// ADRP X0, imported; LDR W0, [X0, #0]: 32-bit load via data-access stub.
		std::vector<U8> objectBytes
			= makeARM64COFFObject(IMAGE_REL_ARM64_PAGEOFFSET_12L, ldrW0X0Imm0);
		HashMap<std::string, Uptr> imports = makeFarImportMap();

		LinkedImage image;
		image.link(objectBytes.data(), objectBytes.size(), imports);

		const U8* code = reinterpret_cast<const U8*>(image.result.definedSymbols[0].address);

		U32 firstInsn = readU32(code);
		CHECK_EQ(firstInsn & 0xFC000000, U32(0x14000000));
		CHECK_EQ(readU32(code + 4), nop);

		I32 branchImm26 = I32(firstInsn << 6) >> 6;
		const U8* stub = code + I64(branchImm26) * 4;
		CHECK_EQ(readU64(stub + 12), farImportedAddress);

		// The stub's modified LDR should be LDR W0, [X16, #0]:
		CHECK_EQ(readU32(stub + 4), U32(0xB9400200));
	}

	// =====================================================================
	// COFF ARM64 tests: new tests
	// =====================================================================

	static void testCOFFARM64Addr64(TEST_STATE_PARAM)
	{
		std::vector<U8> code(8, 0);
		code.push_back(0); // padding
		code.push_back(0);
		code.push_back(0);
		code.push_back(0);
		CoffRelocation rel = {0, 0, IMAGE_REL_ARM64_ADDR64};
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_ARM64, code, {rel}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU64(p), U64(nearImportedAddress));
	}

	static void testCOFFARM64Addr32NB(TEST_STATE_PARAM)
	{
		auto code = arm64Code({0, 0, 0, 0, 0});                // 20 bytes
		CoffRelocation rel = {0, 0, IMAGE_REL_ARM64_ADDR32NB}; // targets "target" (sym 0)
		auto obj = makeCOFFObjectWithTarget(IMAGE_FILE_MACHINE_ARM64, code, {rel}, 16);
		HashMap<std::string, Uptr> imports;
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr imageBase = Uptr(img.result.imageBase);
		Uptr targetAddr = img.result.definedSymbols[0].address + 16;
		I32 expected = I32(Iptr(targetAddr) - Iptr(imageBase));
		CHECK_EQ(readI32(p), expected);
	}

	static void testCOFFARM64Branch26Near(TEST_STATE_PARAM)
	{
		auto code = arm64Code({blTarget0, ret});
		CoffRelocation rel = {0, 0, IMAGE_REL_ARM64_BRANCH26};
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_ARM64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		// Verify it's still a BL (opcode preserved)
		CHECK_EQ(insn & 0xFC000000, U32(0x94000000));
		// Extract imm26 and verify target
		I32 imm26 = I32(insn << 6) >> 6;
		Uptr target = img.result.definedSymbols[0].address + Uptr(I64(imm26) * 4);
		CHECK_EQ(target, img.nearAddr);
	}

	static void testCOFFARM64Branch26Far(TEST_STATE_PARAM)
	{
		auto code = arm64Code({blTarget0, ret});
		CoffRelocation rel = {0, 0, IMAGE_REL_ARM64_BRANCH26};
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_ARM64, code, {rel}, {"imported"});
		auto imports = makeFarImportMap();
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		// Should still be a BL but targeting a stub
		CHECK_EQ(insn & 0xFC000000, U32(0x94000000));
		// Follow to stub
		I32 imm26 = I32(insn << 6) >> 6;
		const U8* stub = p + I64(imm26) * 4;
		// AArch64 stub: LDR X16, [PC, #8]; BR X16; .quad addr
		CHECK_EQ(readU32(stub), U32(0x58000050));
		CHECK_EQ(readU32(stub + 4), U32(0xD61F0200));
		CHECK_EQ(readU64(stub + 8), farImportedAddress);
	}

	static void testCOFFARM64PagebaseAndAddNear(TEST_STATE_PARAM)
	{
		auto code = arm64Code({adrpX0, addX0X0Imm0, ret});
		CoffRelocation rel0 = {0, 0, IMAGE_REL_ARM64_PAGEBASE_REL21};
		CoffRelocation rel1 = {4, 0, IMAGE_REL_ARM64_PAGEOFFSET_12A};
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_ARM64, code, {rel0, rel1}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		// Verify ADD imm12 == (img.nearAddr & 0xFFF)
		U32 addInsn = readU32(p + 4);
		U32 imm12 = (addInsn >> 10) & 0xFFF;
		CHECK_EQ(imm12, U32(img.nearAddr & 0xFFF));
	}

	static void testCOFFARM64PagebaseAndLdstNear(TEST_STATE_PARAM)
	{
		auto code = arm64Code({adrpX0, ldrX0X0Imm0, ret});
		CoffRelocation rel0 = {0, 0, IMAGE_REL_ARM64_PAGEBASE_REL21};
		CoffRelocation rel1 = {4, 0, IMAGE_REL_ARM64_PAGEOFFSET_12L};
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_ARM64, code, {rel0, rel1}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		// For 64-bit LDR, shift=3, imm12 = (addr & 0xFFF) >> 3
		U32 ldrInsn = readU32(p + 4);
		U32 imm12 = (ldrInsn >> 10) & 0xFFF;
		CHECK_EQ(imm12, U32((img.nearAddr & 0xFFF) >> 3));
	}

	static void testCOFFARM64SecRel(TEST_STATE_PARAM)
	{
		auto code = arm64Code({0, 0, 0, 0, 0}); // 20 bytes
		CoffRelocation rel = {0, 0, IMAGE_REL_ARM64_SECREL};
		auto obj = makeCOFFObjectWithTarget(IMAGE_FILE_MACHINE_ARM64, code, {rel}, 16);
		HashMap<std::string, Uptr> imports;
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readI32(p), I32(16));
	}

	static void testCOFFARM64Section(TEST_STATE_PARAM)
	{
		auto code = arm64Code({0, 0, 0, 0, 0});
		CoffRelocation rel = {0, 0, IMAGE_REL_ARM64_SECTION};
		auto obj = makeCOFFObjectWithTarget(IMAGE_FILE_MACHINE_ARM64, code, {rel}, 16);
		HashMap<std::string, Uptr> imports;
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU16(p), U16(1));
	}

	static void testCOFFARM64SecRelLow12A(TEST_STATE_PARAM)
	{
		// ADD X0, X0, #0 at offset 0, target at secrel offset 0x123.
		auto code = arm64Code({addX0X0Imm0, ret, 0, 0, 0}); // pad to 20 bytes
		CoffRelocation rel = {0, 0, IMAGE_REL_ARM64_SECREL_LOW12A};
		auto obj = makeCOFFObjectWithTarget(IMAGE_FILE_MACHINE_ARM64, code, {rel}, 0x123);
		HashMap<std::string, Uptr> imports;
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		U32 imm12 = (insn >> 10) & 0xFFF;
		CHECK_EQ(imm12, U32(0x123));
	}

	static void testCOFFARM64SecRelHigh12A(TEST_STATE_PARAM)
	{
		auto code = arm64Code({addX0X0Imm0, ret, 0, 0, 0});
		CoffRelocation rel = {0, 0, IMAGE_REL_ARM64_SECREL_HIGH12A};
		// Target at offset 0x12345 within section. high12 = (0x12345 >> 12) & 0xFFF = 0x12.
		auto obj = makeCOFFObjectWithTarget(IMAGE_FILE_MACHINE_ARM64, code, {rel}, 0x12345);
		HashMap<std::string, Uptr> imports;
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		U32 imm12 = (insn >> 10) & 0xFFF;
		CHECK_EQ(imm12, U32(0x12));
	}

	static void testCOFFARM64SecRelLow12L(TEST_STATE_PARAM)
	{
		// LDR X0, [X0, #0]: 64-bit load, size field=3, shift=3.
		// Target at secrel offset 0x18. imm12 = (0x18 & 0xFFF) >> 3 = 3.
		auto code = arm64Code({ldrX0X0Imm0, ret, 0, 0, 0});
		CoffRelocation rel = {0, 0, IMAGE_REL_ARM64_SECREL_LOW12L};
		auto obj = makeCOFFObjectWithTarget(IMAGE_FILE_MACHINE_ARM64, code, {rel}, 0x18);
		HashMap<std::string, Uptr> imports;
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		U32 imm12 = (insn >> 10) & 0xFFF;
		CHECK_EQ(imm12, U32(3));
	}

	// =====================================================================
	// ELF x86-64 tests
	// =====================================================================

	static void testELFX64Abs64(TEST_STATE_PARAM)
	{
		std::vector<U8> code(8, 0);
		code.push_back(0xC3);
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_64, 0};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU64(p), U64(nearImportedAddress));
	}

	static void testELFX64Abs32(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_32, 0};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU32(p), U32(nearImportedAddress));
	}

	static void testELFX64Abs32S(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_32S, 0};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readI32(p), I32(nearImportedAddress));
	}

	static void testELFX64PC32Near(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		// r_addend = -4 is standard for PC32 (displacement from end of field)
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_PC32, -4};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		// relocRel32: value = sym + addend = nearImport + (-4), delta = value - fixupAddr
		// But with stub fallback. For near, delta fits.
		I32 expected = I32(Iptr(img.nearAddr) - 4 - Iptr(fixupAddr));
		CHECK_EQ(readI32(p), expected);
	}

	static void testELFX64PC32Far(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_PC32, -4};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		auto imports = makeFarImportMap();
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		// Far import triggers stub. Follow rel32 to stub.
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		// stub value = fixupAddr + disp + addend_correction... just verify stub pattern
		const U8* stub = reinterpret_cast<const U8*>(fixupAddr + disp + 4 - 4);
		CHECK_EQ(stub[0], U8(0xFF));
		CHECK_EQ(stub[1], U8(0x25));
	}

	static void testELFX64PLT32(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_PLT32, -4};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		I32 expected = I32(Iptr(img.nearAddr) - 4 - Iptr(fixupAddr));
		CHECK_EQ(readI32(p), expected);
	}

	static void testELFX64PC64(TEST_STATE_PARAM)
	{
		std::vector<U8> code(8, 0);
		code.push_back(0xC3);
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_PC64, 0};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		I64 expected = I64(Iptr(img.nearAddr) - Iptr(fixupAddr));
		CHECK_EQ(readI64(p), expected);
	}

	static void testELFX64GotPcRel(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_GOTPCREL, -4};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		// Follow the rel32 to the GOT entry and verify it contains the symbol value.
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		// GOT entry is at fixupAddr + disp + 4 (accounting for addend = -4 in the reloc)
		// Actually: value = gotEntry + (-4), delta = value - fixupAddr
		// so gotEntry = fixupAddr + disp + 4
		const U8* gotEntry = reinterpret_cast<const U8*>(fixupAddr + disp + 4);
		CHECK_EQ(readU64(gotEntry), U64(img.nearAddr));
	}

	static void testELFX64GotPcRelX(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_GOTPCRELX, -4};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		const U8* gotEntry = reinterpret_cast<const U8*>(fixupAddr + disp + 4);
		CHECK_EQ(readU64(gotEntry), U64(img.nearAddr));
	}

	static void testELFX64RexGotPcRelX(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_X86_64_REX_GOTPCRELX, -4};
		auto obj = makeELFObject(EM_X86_64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		const U8* gotEntry = reinterpret_cast<const U8*>(fixupAddr + disp + 4);
		CHECK_EQ(readU64(gotEntry), U64(img.nearAddr));
	}

	// =====================================================================
	// ELF AArch64 tests
	// =====================================================================

	static void testELFARM64Abs64(TEST_STATE_PARAM)
	{
		std::vector<U8> code(8, 0);
		auto padded = arm64Code({ret});
		code.insert(code.end(), padded.begin(), padded.end());
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_AARCH64_ABS64, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU64(p), U64(nearImportedAddress));
	}

	static void testELFARM64Abs32(TEST_STATE_PARAM)
	{
		auto code = arm64Code({0, ret});
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_AARCH64_ABS32, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU32(p), U32(nearImportedAddress));
	}

	static void testELFARM64Prel64(TEST_STATE_PARAM)
	{
		std::vector<U8> code(8, 0);
		auto padded = arm64Code({ret});
		code.insert(code.end(), padded.begin(), padded.end());
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_AARCH64_PREL64, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		I64 expected = I64(Iptr(img.nearAddr) - Iptr(fixupAddr));
		CHECK_EQ(readI64(p), expected);
	}

	static void testELFARM64Prel32(TEST_STATE_PARAM)
	{
		auto code = arm64Code({0, ret});
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_AARCH64_PREL32, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		I32 expected = I32(Iptr(img.nearAddr) - Iptr(fixupAddr));
		CHECK_EQ(readI32(p), expected);
	}

	static void testELFARM64Call26Near(TEST_STATE_PARAM)
	{
		auto code = arm64Code({blTarget0, ret});
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_AARCH64_CALL26, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		CHECK_EQ(insn & 0xFC000000, U32(0x94000000));
		I32 imm26 = I32(insn << 6) >> 6;
		Uptr target = img.result.definedSymbols[0].address + Uptr(I64(imm26) * 4);
		CHECK_EQ(target, img.nearAddr);
	}

	static void testELFARM64Call26Far(TEST_STATE_PARAM)
	{
		auto code = arm64Code({blTarget0, ret});
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_AARCH64_CALL26, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela}, {"imported"});
		auto imports = makeFarImportMap();
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		CHECK_EQ(insn & 0xFC000000, U32(0x94000000));
		I32 imm26 = I32(insn << 6) >> 6;
		const U8* stub = p + I64(imm26) * 4;
		CHECK_EQ(readU32(stub), U32(0x58000050));     // LDR X16, [PC, #8]
		CHECK_EQ(readU32(stub + 4), U32(0xD61F0200)); // BR X16
		CHECK_EQ(readU64(stub + 8), farImportedAddress);
	}

	static void testELFARM64Jump26Near(TEST_STATE_PARAM)
	{
		auto code = arm64Code({bTarget0, ret});
		Elf64_Rela rela = {0, ((U64)1 << 32) | R_AARCH64_JUMP26, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		CHECK_EQ(insn & 0xFC000000, U32(0x14000000));
		I32 imm26 = I32(insn << 6) >> 6;
		Uptr target = img.result.definedSymbols[0].address + Uptr(I64(imm26) * 4);
		CHECK_EQ(target, img.nearAddr);
	}

	static void testELFARM64AdrpAndAddLo12(TEST_STATE_PARAM)
	{
		auto code = arm64Code({adrpX0, addX0X0Imm0, ret});
		Elf64_Rela rela0 = {0, ((U64)1 << 32) | R_AARCH64_ADR_PREL_PG_HI21, 0};
		Elf64_Rela rela1 = {4, ((U64)1 << 32) | R_AARCH64_ADD_ABS_LO12_NC, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela0, rela1}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 addInsn = readU32(p + 4);
		U32 imm12 = (addInsn >> 10) & 0xFFF;
		CHECK_EQ(imm12, U32(img.nearAddr & 0xFFF));
	}

	// Parameterized LDST test
	static void testELFARM64LdstVariant(TEST_STATE_PARAM, U32 relocType, U32 insn, U32 shift)
	{
		auto code = arm64Code({insn, ret});
		Elf64_Rela rela = {0, ((U64)1 << 32) | relocType, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 patchedInsn = readU32(p);
		U32 imm12 = (patchedInsn >> 10) & 0xFFF;
		CHECK_EQ(imm12, U32((img.nearAddr & 0xFFF) >> shift));
	}

	static void testELFARM64Ldst8Lo12(TEST_STATE_PARAM)
	{
		testELFARM64LdstVariant(TEST_STATE_ARG, R_AARCH64_LDST8_ABS_LO12_NC, ldrbW0X0Imm0, 0);
	}
	static void testELFARM64Ldst16Lo12(TEST_STATE_PARAM)
	{
		testELFARM64LdstVariant(TEST_STATE_ARG, R_AARCH64_LDST16_ABS_LO12_NC, ldrhW0X0Imm0, 1);
	}
	static void testELFARM64Ldst32Lo12(TEST_STATE_PARAM)
	{
		testELFARM64LdstVariant(TEST_STATE_ARG, R_AARCH64_LDST32_ABS_LO12_NC, ldrW0X0Imm0, 2);
	}
	static void testELFARM64Ldst64Lo12(TEST_STATE_PARAM)
	{
		testELFARM64LdstVariant(TEST_STATE_ARG, R_AARCH64_LDST64_ABS_LO12_NC, ldrX0X0Imm0, 3);
	}
	static void testELFARM64Ldst128Lo12(TEST_STATE_PARAM)
	{
		testELFARM64LdstVariant(TEST_STATE_ARG, R_AARCH64_LDST128_ABS_LO12_NC, ldrQ0X0Imm0, 4);
	}

	// Parameterized MOVW test. Uses import 0x1234567890ABCDEF.
	static void testELFARM64MovwVariant(TEST_STATE_PARAM, U32 relocType, U32 insn, U32 shift)
	{
		auto code = arm64Code({insn, ret});
		Elf64_Rela rela = {0, ((U64)1 << 32) | relocType, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela}, {"imported"});
		Uptr addr = 0x1234567890ABCDEF;
		auto imports = makeImportMap({{"imported", addr}});
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 patchedInsn = readU32(p);
		U16 imm16 = U16((patchedInsn >> 5) & 0xFFFF);
		U16 expected = U16((addr >> shift) & 0xFFFF);
		CHECK_EQ(imm16, expected);
	}

	static void testELFARM64MovwG0(TEST_STATE_PARAM)
	{
		testELFARM64MovwVariant(TEST_STATE_ARG, R_AARCH64_MOVW_UABS_G0, movzX0Imm0, 0);
	}
	static void testELFARM64MovwG0NC(TEST_STATE_PARAM)
	{
		testELFARM64MovwVariant(TEST_STATE_ARG, R_AARCH64_MOVW_UABS_G0_NC, movzX0Imm0, 0);
	}
	static void testELFARM64MovwG1(TEST_STATE_PARAM)
	{
		testELFARM64MovwVariant(TEST_STATE_ARG, R_AARCH64_MOVW_UABS_G1, movkX0Imm0Lsl16, 16);
	}
	static void testELFARM64MovwG1NC(TEST_STATE_PARAM)
	{
		testELFARM64MovwVariant(TEST_STATE_ARG, R_AARCH64_MOVW_UABS_G1_NC, movkX0Imm0Lsl16, 16);
	}
	static void testELFARM64MovwG2(TEST_STATE_PARAM)
	{
		testELFARM64MovwVariant(TEST_STATE_ARG, R_AARCH64_MOVW_UABS_G2, movkX0Imm0Lsl32, 32);
	}
	static void testELFARM64MovwG2NC(TEST_STATE_PARAM)
	{
		testELFARM64MovwVariant(TEST_STATE_ARG, R_AARCH64_MOVW_UABS_G2_NC, movkX0Imm0Lsl32, 32);
	}
	static void testELFARM64MovwG3(TEST_STATE_PARAM)
	{
		testELFARM64MovwVariant(TEST_STATE_ARG, R_AARCH64_MOVW_UABS_G3, movkX0Imm0Lsl48, 48);
	}

	static void testELFARM64GotPage(TEST_STATE_PARAM)
	{
		auto code = arm64Code({adrpX0, ldrX0X0Imm0, ret});
		Elf64_Rela rela0 = {0, ((U64)1 << 32) | R_AARCH64_ADR_GOT_PAGE, 0};
		Elf64_Rela rela1 = {4, ((U64)1 << 32) | R_AARCH64_LD64_GOT_LO12_NC, 0};
		auto obj = makeELFObject(EM_AARCH64, code, {rela0, rela1}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		// Just verify it linked without error and produced a symbol.
		CHECK_EQ(img.result.definedSymbols.size(), Uptr(1));
	}
	// =====================================================================
	// Mach-O x86-64 tests
	// =====================================================================

	static void testMachOX64Unsigned64(TEST_STATE_PARAM)
	{
		std::vector<U8> code(8, 0);
		code.push_back(0xC3);
		TestMachOReloc rel = {0, makeMachORelocInfo(0, false, 3, true, X86_64_RELOC_UNSIGNED)};
		auto obj = makeMachOObject(CPU_TYPE_X86_64, code, {rel}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU64(p), U64(nearImportedAddress));
	}

	static void testMachOX64Unsigned32(TEST_STATE_PARAM)
	{
		std::vector<U8> code(4, 0);
		code.push_back(0xC3);
		TestMachOReloc rel = {0, makeMachORelocInfo(0, false, 2, true, X86_64_RELOC_UNSIGNED)};
		auto obj = makeMachOObject(CPU_TYPE_X86_64, code, {rel}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU32(p), U32(nearImportedAddress));
	}

	static void testMachOX64SignedNear(TEST_STATE_PARAM)
	{
		// Code bytes at fixup: I32 addend. Implementation: addend + 4.
		// Set to -4 (0xFFFFFFFC) so net addend = 0.
		std::vector<U8> code = {0xFC, 0xFF, 0xFF, 0xFF, 0xC3};
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, X86_64_RELOC_SIGNED)};
		auto obj = makeMachOObject(CPU_TYPE_X86_64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		// value = sym + addend + 4 = sym + (-4) + 4 = sym
		// delta = value - (fixupAddr + 4) = sym - fixupAddr - 4
		I32 expected = I32(Iptr(img.nearAddr) - Iptr(fixupAddr) - 4);
		CHECK_EQ(readI32(p), expected);
	}

	static void testMachOX64SignedFar(TEST_STATE_PARAM)
	{
		std::vector<U8> code = {0xFC, 0xFF, 0xFF, 0xFF, 0xC3};
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, X86_64_RELOC_SIGNED)};
		auto obj = makeMachOObject(CPU_TYPE_X86_64, code, {rel}, {"imported"});
		auto imports = makeFarImportMap();
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		// Verify stub was created (FF 25 pattern)
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		const U8* stub = reinterpret_cast<const U8*>(fixupAddr + 4 + disp);
		CHECK_EQ(stub[0], U8(0xFF));
		CHECK_EQ(stub[1], U8(0x25));
	}

	static void testMachOX64Branch(TEST_STATE_PARAM)
	{
		std::vector<U8> code = {0xFC, 0xFF, 0xFF, 0xFF, 0xC3};
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, X86_64_RELOC_BRANCH)};
		auto obj = makeMachOObject(CPU_TYPE_X86_64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		I32 expected = I32(Iptr(img.nearAddr) - Iptr(fixupAddr) - 4);
		CHECK_EQ(readI32(p), expected);
	}

	static void testMachOX64GotLoad(TEST_STATE_PARAM)
	{
		// Code GOT: addend = 0, RIP-relative. GOT entry at fixupAddr + 4 + disp.
		std::vector<U8> code = {0x00, 0x00, 0x00, 0x00, 0xC3};
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, X86_64_RELOC_GOT_LOAD)};
		auto obj = makeMachOObject(CPU_TYPE_X86_64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		const U8* gotEntry = reinterpret_cast<const U8*>(fixupAddr + 4 + disp);
		CHECK_EQ(readU64(gotEntry), U64(img.nearAddr));
	}

	static void testMachOX64Got(TEST_STATE_PARAM)
	{
		// Code GOT: addend = 0, RIP-relative. GOT entry at fixupAddr + 4 + disp.
		std::vector<U8> code = {0x00, 0x00, 0x00, 0x00, 0xC3};
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, X86_64_RELOC_GOT)};
		auto obj = makeMachOObject(CPU_TYPE_X86_64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		const U8* gotEntry = reinterpret_cast<const U8*>(fixupAddr + 4 + disp);
		CHECK_EQ(readU64(gotEntry), U64(img.nearAddr));
	}

	static void testMachOX64GotDataPcRel(TEST_STATE_PARAM)
	{
		// Data GOT (e.g. eh_frame personality pointer): addend = 4 (LLVM's pcrel compensation),
		// pure pcrel (not RIP-relative). GOT entry at fixupAddr + disp (no +4).
		std::vector<U8> code = {0x04, 0x00, 0x00, 0x00, 0xC3};
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, X86_64_RELOC_GOT)};
		auto obj = makeMachOObject(CPU_TYPE_X86_64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		// For data pcrel (DW_EH_PE_pcrel), the reference point is the fixup itself (no +4).
		const U8* gotEntry = reinterpret_cast<const U8*>(fixupAddr + disp);
		CHECK_EQ(readU64(gotEntry), U64(img.nearAddr));
	}

	// Parameterized SIGNED_1/2/4 test
	static void testMachOX64SignedVariant(TEST_STATE_PARAM, U32 relocType, I64 extraAddend)
	{
		// Inline addend = -(extraAddend) so that net = 0
		I32 inlineAddend = I32(-extraAddend);
		std::vector<U8> code(4, 0);
		memcpy(code.data(), &inlineAddend, 4);
		code.push_back(0xC3);
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, relocType)};
		auto obj = makeMachOObject(CPU_TYPE_X86_64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		// value = sym + inlineAddend + extraAddend = sym + 0 = sym
		// delta = value - (fixupAddr + 4) = sym - fixupAddr - 4
		I32 expected = I32(Iptr(img.nearAddr) - Iptr(fixupAddr) - 4);
		CHECK_EQ(readI32(p), expected);
	}

	static void testMachOX64Signed1(TEST_STATE_PARAM)
	{
		testMachOX64SignedVariant(TEST_STATE_ARG, X86_64_RELOC_SIGNED_1, 5);
	}
	static void testMachOX64Signed2(TEST_STATE_PARAM)
	{
		testMachOX64SignedVariant(TEST_STATE_ARG, X86_64_RELOC_SIGNED_2, 6);
	}
	static void testMachOX64Signed4(TEST_STATE_PARAM)
	{
		testMachOX64SignedVariant(TEST_STATE_ARG, X86_64_RELOC_SIGNED_4, 8);
	}

	// =====================================================================
	// Mach-O AArch64 tests
	// =====================================================================

	static void testMachOARM64Unsigned64(TEST_STATE_PARAM)
	{
		std::vector<U8> code(8, 0);
		auto padded = arm64Code({ret});
		code.insert(code.end(), padded.begin(), padded.end());
		TestMachOReloc rel = {0, makeMachORelocInfo(0, false, 3, true, ARM64_RELOC_UNSIGNED)};
		auto obj = makeMachOObject(CPU_TYPE_ARM64, code, {rel}, {"imported"});
		auto imports = makeNearImportMap(nearImportedAddress);
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		CHECK_EQ(readU64(p), U64(nearImportedAddress));
	}

	static void testMachOARM64Branch26Near(TEST_STATE_PARAM)
	{
		auto code = arm64Code({blTarget0, ret});
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, ARM64_RELOC_BRANCH26)};
		auto obj = makeMachOObject(CPU_TYPE_ARM64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		CHECK_EQ(insn & 0xFC000000, U32(0x94000000));
		I32 imm26 = I32(insn << 6) >> 6;
		Uptr target = img.result.definedSymbols[0].address + Uptr(I64(imm26) * 4);
		CHECK_EQ(target, img.nearAddr);
	}

	static void testMachOARM64Branch26Far(TEST_STATE_PARAM)
	{
		auto code = arm64Code({blTarget0, ret});
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, ARM64_RELOC_BRANCH26)};
		auto obj = makeMachOObject(CPU_TYPE_ARM64, code, {rel}, {"imported"});
		auto imports = makeFarImportMap();
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		I32 imm26 = I32(insn << 6) >> 6;
		const U8* stub = p + I64(imm26) * 4;
		CHECK_EQ(readU32(stub), U32(0x58000050));     // LDR X16, [PC, #8]
		CHECK_EQ(readU32(stub + 4), U32(0xD61F0200)); // BR X16
		CHECK_EQ(readU64(stub + 8), farImportedAddress);
	}

	static void testMachOARM64Page21AndPageOff12Add(TEST_STATE_PARAM)
	{
		auto code = arm64Code({adrpX0, addX0X0Imm0, ret});
		TestMachOReloc rel0 = {0, makeMachORelocInfo(0, true, 2, true, ARM64_RELOC_PAGE21)};
		TestMachOReloc rel1 = {4, makeMachORelocInfo(0, false, 2, true, ARM64_RELOC_PAGEOFF12)};
		auto obj = makeMachOObject(CPU_TYPE_ARM64, code, {rel0, rel1}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 addInsn = readU32(p + 4);
		U32 imm12 = (addInsn >> 10) & 0xFFF;
		CHECK_EQ(imm12, U32(img.nearAddr & 0xFFF));
	}

	static void testMachOARM64Page21AndPageOff12Ldr(TEST_STATE_PARAM)
	{
		auto code = arm64Code({adrpX0, ldrX0X0Imm0, ret});
		TestMachOReloc rel0 = {0, makeMachORelocInfo(0, true, 2, true, ARM64_RELOC_PAGE21)};
		TestMachOReloc rel1 = {4, makeMachORelocInfo(0, false, 2, true, ARM64_RELOC_PAGEOFF12)};
		auto obj = makeMachOObject(CPU_TYPE_ARM64, code, {rel0, rel1}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 ldrInsn = readU32(p + 4);
		U32 imm12 = (ldrInsn >> 10) & 0xFFF;
		CHECK_EQ(imm12, U32((img.nearAddr & 0xFFF) >> 3));
	}

	static void testMachOARM64GotLoadPage21AndPageOff12(TEST_STATE_PARAM)
	{
		auto code = arm64Code({adrpX0, ldrX0X0Imm0, ret});
		TestMachOReloc rel0
			= {0, makeMachORelocInfo(0, true, 2, true, ARM64_RELOC_GOT_LOAD_PAGE21)};
		TestMachOReloc rel1
			= {4, makeMachORelocInfo(0, false, 2, true, ARM64_RELOC_GOT_LOAD_PAGEOFF12)};
		auto obj = makeMachOObject(CPU_TYPE_ARM64, code, {rel0, rel1}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		CHECK_EQ(img.result.definedSymbols.size(), Uptr(1));
	}

	static void testMachOARM64PointerToGotPcRel(TEST_STATE_PARAM)
	{
		auto code = arm64Code({0, ret});
		TestMachOReloc rel = {0, makeMachORelocInfo(0, true, 2, true, ARM64_RELOC_POINTER_TO_GOT)};
		auto obj = makeMachOObject(CPU_TYPE_ARM64, code, {rel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		// I32 at fixup = gotEntry - fixupAddr
		I32 disp = readI32(p);
		Uptr fixupAddr = img.result.definedSymbols[0].address;
		const U8* gotEntry = reinterpret_cast<const U8*>(fixupAddr + disp);
		CHECK_EQ(readU64(gotEntry), U64(img.nearAddr));
	}

	static void testMachOARM64Addend(TEST_STATE_PARAM)
	{
		auto code = arm64Code({blTarget0, ret});
		// ADDEND reloc provides addend for next reloc. r_info symbolnum field = addend value.
		TestMachOReloc addendRel = {0, makeMachORelocInfo(4, false, 0, false, ARM64_RELOC_ADDEND)};
		TestMachOReloc branchRel = {0, makeMachORelocInfo(0, true, 2, true, ARM64_RELOC_BRANCH26)};
		auto obj = makeMachOObject(CPU_TYPE_ARM64, code, {addendRel, branchRel}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		U32 insn = readU32(p);
		I32 imm26 = I32(insn << 6) >> 6;
		Uptr target = img.result.definedSymbols[0].address + Uptr(I64(imm26) * 4);
		// Target should be img.nearAddr + 4 (the addend)
		CHECK_EQ(target, img.nearAddr + 4);
	}
	// =====================================================================
	// Mach-O compact unwind multi-page test
	// =====================================================================

	// Build a Mach-O object with __text and __LD,__compact_unwind sections.
	// Each compact_unwind entry is 32 bytes: {U64 funcStart, U32 length, U32 encoding,
	// U64 personality, U64 lsda}. The funcStart fields need X86_64_RELOC_UNSIGNED
	// relocations to resolve to the function addresses in __text.
	static std::vector<U8> makeMachOObjectWithCompactUnwind(U32 cputype,
															const std::vector<U8>& code,
															U32 numFunctions,
															U32 funcSize)
	{
		U32 codeSize = U32(code.size());
		U32 cuEntrySize = 32;
		U32 cuDataSize = numFunctions * cuEntrySize;
		U32 numCuRelocs = numFunctions; // one UNSIGNED reloc per entry (for funcStart)

		// String table: "\0_func\0"
		std::vector<U8> strtab;
		strtab.push_back(0);
		U32 funcStrOffset = U32(strtab.size());
		for(char c : std::string("_func")) { strtab.push_back(U8(c)); }
		strtab.push_back(0);

		U32 numSymbols = 1; // just the func symbol
		U32 cuRelocSize = numCuRelocs * U32(sizeof(TestMachOReloc));
		U32 symtabSize = numSymbols * U32(sizeof(NList64));
		U32 strtabSize = U32(strtab.size());

		U32 numSections = 2; // __text + __compact_unwind
		U32 dataStart = U32(sizeof(MachHeader64) + sizeof(SegmentCommand64)
							+ numSections * sizeof(TestMachOSection64) + sizeof(TestSymtabCommand));
		U32 codeOff = dataStart;
		U32 cuDataOff = codeOff + codeSize;
		U32 cuRelocOff = cuDataOff + cuDataSize;
		U32 symtabOff = cuRelocOff + cuRelocSize;
		U32 strtabOff = symtabOff + symtabSize;

		ArrayOutputStream stream;

		// Mach-O header
		MachHeader64 hdr = {};
		hdr.magic = MH_MAGIC_64;
		hdr.cputype = cputype;
		hdr.filetype = 1; // MH_OBJECT
		hdr.ncmds = 2;    // LC_SEGMENT_64 + LC_SYMTAB
		hdr.sizeofcmds = U32(sizeof(SegmentCommand64) + numSections * sizeof(TestMachOSection64)
							 + sizeof(TestSymtabCommand));
		write(stream, hdr);

		// LC_SEGMENT_64
		SegmentCommand64 seg = {};
		seg.cmd = LC_SEGMENT_64;
		seg.cmdsize = U32(sizeof(SegmentCommand64) + numSections * sizeof(TestMachOSection64));
		seg.vmsize = codeSize + cuDataSize;
		seg.fileoff = codeOff;
		seg.filesize = codeSize + cuDataSize;
		seg.maxprot = 7;
		seg.initprot = 7;
		seg.nsects = numSections;
		write(stream, seg);

		// Section 0: __text
		TestMachOSection64 textSect = {};
		memcpy(textSect.sectname, "__text", 6);
		memcpy(textSect.segname, "__TEXT", 6);
		textSect.size = codeSize;
		textSect.offset = codeOff;
		textSect.align = 2;
		textSect.flags = S_ATTR_PURE_INSTRUCTIONS;
		write(stream, textSect);

		// Section 1: __compact_unwind (exactly 16 chars, fills the field)
		TestMachOSection64 cuSect = {};
		memcpy(cuSect.sectname, "__compact_unwind", 16);
		memcpy(cuSect.segname, "__LD\0\0\0\0\0\0\0\0\0\0\0\0", 16);
		cuSect.addr = codeSize; // starts after __text in VM
		cuSect.size = cuDataSize;
		cuSect.offset = cuDataOff;
		cuSect.align = 3; // 2^3 = 8 byte alignment
		cuSect.reloff = cuRelocOff;
		cuSect.nreloc = numCuRelocs;
		write(stream, cuSect);

		// LC_SYMTAB
		TestSymtabCommand symcmd = {};
		symcmd.cmd = LC_SYMTAB;
		symcmd.cmdsize = sizeof(TestSymtabCommand);
		symcmd.symoff = symtabOff;
		symcmd.nsyms = numSymbols;
		symcmd.stroff = strtabOff;
		symcmd.strsize = strtabSize;
		write(stream, symcmd);

		// Code bytes
		serializeBytes(stream, code.data(), code.size());

		// Compact unwind entries
		U32 dwarfEncoding = (cputype == CPU_TYPE_ARM64) ? 0x03000000 : 0x04000000;
		for(U32 i = 0; i < numFunctions; ++i)
		{
			U64 funcStart = U64(i) * funcSize; // section-relative, will be relocated
			U32 funcLength = funcSize;
			U32 encoding = dwarfEncoding;
			U64 personality = 0;
			U64 lsda = 0;
			write(stream, funcStart);
			write(stream, funcLength);
			write(stream, encoding);
			write(stream, personality);
			write(stream, lsda);
		}

		// Relocations for compact unwind entries (X86_64_RELOC_UNSIGNED on funcStart field)
		U32 relocType = (cputype == CPU_TYPE_ARM64) ? ARM64_RELOC_UNSIGNED : X86_64_RELOC_UNSIGNED;
		for(U32 i = 0; i < numFunctions; ++i)
		{
			TestMachOReloc rel;
			rel.r_address = I32(i * cuEntrySize);
			// length=3 (8 bytes), pcrel=false, extern=false, symbolnum=section 1 (1-based __text)
			rel.r_info = makeMachORelocInfo(1, false, 3, false, relocType);
			write(stream, rel);
		}

		// Symbol table (one defined symbol)
		NList64 funcSym = {};
		funcSym.n_strx = funcStrOffset;
		funcSym.n_type = N_SECT | N_EXT;
		funcSym.n_sect = 1; // 1-based section index for __text
		write(stream, funcSym);

		// String table
		serializeBytes(stream, strtab.data(), strtab.size());

		return stream.getBytes();
	}

	static void testMachOCompactUnwindMultiPage(TEST_STATE_PARAM)
	{
		// Create a Mach-O object with 600 functions (> 511 max per regular page).
		constexpr U32 numFunctions = 600;
		constexpr U32 funcSize = 4; // 4 bytes per function
		U32 codeSize = numFunctions * funcSize;

		// Fill code with RET instructions (0xC3 for x86-64, padded to funcSize).
		std::vector<U8> code(codeSize, 0xCC); // INT3 padding
		for(U32 i = 0; i < numFunctions; ++i) { code[i * funcSize] = 0xC3; }

		auto obj = makeMachOObjectWithCompactUnwind(CPU_TYPE_X86_64, code, numFunctions, funcSize);

		LinkedImage img;
		HashMap<std::string, Uptr> imports;
		img.link(obj.data(), obj.size(), imports);

		// Verify compact unwind info was synthesized.
		const U8* unwindInfo = img.result.unwindInfo.compactUnwind;
		CHECK_EQ(unwindInfo != nullptr, true);
		if(!unwindInfo) { return; }
		Uptr unwindInfoSize = img.result.unwindInfo.compactUnwindNumBytes;
		CHECK_EQ(unwindInfoSize > 0, true);

		// Parse header.
		CHECK_EQ(unwindInfoSize >= 28, true);
		U32 version = readU32(unwindInfo + 0);
		CHECK_EQ(version, U32(1));

		U32 indexOff = readU32(unwindInfo + 20);
		U32 indexCount = readU32(unwindInfo + 24);

		// With 600 entries and max 511 per page, we need at least 2 pages + sentinel = 3.
		CHECK_EQ(indexCount >= U32(3), true);

		// Verify each second-level page has kind=2 (REGULAR) and entryCount <= 511.
		U32 totalEntries = 0;
		for(U32 i = 0; i + 1 < indexCount; ++i)
		{
			U32 pageOff = readU32(unwindInfo + indexOff + i * 12 + 4);
			CHECK_EQ(pageOff > 0, true);
			if(pageOff == 0 || pageOff + 8 > unwindInfoSize) { continue; }

			U32 pageKind = readU32(unwindInfo + pageOff);
			CHECK_EQ(pageKind, U32(2)); // REGULAR

			U16 entryCount = readU16(unwindInfo + pageOff + 6);
			CHECK_EQ(entryCount <= U16(511), true);
			totalEntries += entryCount;
		}

		// All entries accounted for.
		CHECK_EQ(totalEntries, numFunctions);

		// Sentinel entry should have pageOff=0.
		U32 sentinelPageOff = readU32(unwindInfo + indexOff + (indexCount - 1) * 12 + 4);
		CHECK_EQ(sentinelPageOff, U32(0));
	}

	// =====================================================================
	// Stub/GOT stress tests
	// =====================================================================

	static void testX64StubCachingManyRelocs(TEST_STATE_PARAM)
	{
		// 1 far import, 8 REL32 relocs all to same target. All should share 1 stub.
		std::vector<U8> code(8 * 4 + 1, 0); // 8 fixup slots + ret
		code.back() = 0xC3;
		std::vector<CoffRelocation> relocs;
		for(U32 i = 0; i < 8; ++i) { relocs.push_back({i * 4, 0, IMAGE_REL_AMD64_REL32}); }
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_AMD64, code, relocs, {"imported"});
		auto imports = makeFarImportMap();
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr baseAddr = img.result.definedSymbols[0].address;
		// All 8 should resolve to the same stub. Verify first two match.
		I32 disp0 = readI32(p);
		I32 disp1 = readI32(p + 4);
		// stub address from slot 0: baseAddr + 4 + disp0
		// stub address from slot 1: (baseAddr + 4) + 4 + disp1
		Uptr stub0 = Uptr(Iptr(baseAddr) + 4 + disp0);
		Uptr stub1 = Uptr(Iptr(baseAddr + 4) + 4 + disp1);
		CHECK_EQ(stub0, stub1);
	}

	static void testX64StubMultipleTargets(TEST_STATE_PARAM)
	{
		// 3 far imports, 2 REL32 each. Should create 3 distinct stubs.
		std::vector<U8> code(6 * 4 + 1, 0);
		code.back() = 0xC3;
		std::vector<CoffRelocation> relocs;
		relocs.push_back({0, 0, IMAGE_REL_AMD64_REL32});  // import 0
		relocs.push_back({4, 0, IMAGE_REL_AMD64_REL32});  // import 0
		relocs.push_back({8, 1, IMAGE_REL_AMD64_REL32});  // import 1
		relocs.push_back({12, 1, IMAGE_REL_AMD64_REL32}); // import 1
		relocs.push_back({16, 2, IMAGE_REL_AMD64_REL32}); // import 2
		relocs.push_back({20, 2, IMAGE_REL_AMD64_REL32}); // import 2
		auto obj = makeCOFFObject(IMAGE_FILE_MACHINE_AMD64, code, relocs, {"imp0", "imp1", "imp2"});
		Uptr addr0 = 0x10000000000ull;
		Uptr addr1 = 0x20000000000ull;
		Uptr addr2 = 0x30000000000ull;
		auto imports = makeImportMap({{"imp0", addr0}, {"imp1", addr1}, {"imp2", addr2}});
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr base = img.result.definedSymbols[0].address;
		// Pairs targeting same import should share a stub.
		Uptr s0a = Uptr(Iptr(base + 0) + 4 + readI32(p + 0));
		Uptr s0b = Uptr(Iptr(base + 4) + 4 + readI32(p + 4));
		Uptr s1a = Uptr(Iptr(base + 8) + 4 + readI32(p + 8));
		Uptr s2a = Uptr(Iptr(base + 16) + 4 + readI32(p + 16));
		CHECK_EQ(s0a, s0b); // same import -> same stub
		CHECK_NE(s0a, s1a); // different imports -> different stubs
		CHECK_NE(s1a, s2a);
	}

	static void testARM64StubCachingManyRelocs(TEST_STATE_PARAM)
	{
		// 1 far import, 8 CALL26 relocs.
		std::vector<U32> insns;
		for(int i = 0; i < 8; ++i) { insns.push_back(blTarget0); }
		insns.push_back(ret);
		auto code = arm64Code(insns);

		std::vector<Elf64_Rela> relas;
		for(U32 i = 0; i < 8; ++i)
		{
			relas.push_back({U64(i * 4), ((U64)1 << 32) | R_AARCH64_CALL26, 0});
		}
		auto obj = makeELFObject(EM_AARCH64, code, relas, {"imported"});
		auto imports = makeFarImportMap();
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr base = img.result.definedSymbols[0].address;
		// All should reach the same stub.
		U32 insn0 = readU32(p);
		U32 insn1 = readU32(p + 4);
		I32 off0 = I32(insn0 << 6) >> 6;
		I32 off1 = I32(insn1 << 6) >> 6;
		Uptr stub0 = base + Uptr(I64(off0) * 4);
		Uptr stub1 = (base + 4) + Uptr(I64(off1) * 4);
		CHECK_EQ(stub0, stub1);
	}

	static void testGotCachingSameSymbol(TEST_STATE_PARAM)
	{
		// 1 import, 2 GOTPCREL relocs. Should share 1 GOT entry.
		std::vector<U8> code(8, 0);
		code.push_back(0xC3);
		Elf64_Rela rela0 = {0, ((U64)1 << 32) | R_X86_64_GOTPCREL, -4};
		Elf64_Rela rela1 = {4, ((U64)1 << 32) | R_X86_64_GOTPCREL, -4};
		auto obj = makeELFObject(EM_X86_64, code, {rela0, rela1}, {"imported"});
		LinkedImage img;
		auto imports = makeNearImportMap(img.nearAddr);
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr base = img.result.definedSymbols[0].address;
		Uptr got0 = Uptr(Iptr(base + 0) + 4 + readI32(p + 0));
		Uptr got1 = Uptr(Iptr(base + 4) + 4 + readI32(p + 4));
		CHECK_EQ(got0, got1); // same symbol -> same GOT entry
	}

	static void testGotMultipleSymbols(TEST_STATE_PARAM)
	{
		// 3 imports, 1 GOTPCREL each. 3 distinct GOT entries.
		std::vector<U8> code(12, 0);
		code.push_back(0xC3);
		// sym indices: imp0=1, imp1=2, imp2=3 (0=null)
		Elf64_Rela rela0 = {0, ((U64)1 << 32) | R_X86_64_GOTPCREL, -4};
		Elf64_Rela rela1 = {4, ((U64)2 << 32) | R_X86_64_GOTPCREL, -4};
		Elf64_Rela rela2 = {8, ((U64)3 << 32) | R_X86_64_GOTPCREL, -4};
		auto obj = makeELFObject(EM_X86_64, code, {rela0, rela1, rela2}, {"imp0", "imp1", "imp2"});
		auto imports = makeImportMap(
			{{"imp0", Uptr(0x1000)}, {"imp1", Uptr(0x2000)}, {"imp2", Uptr(0x3000)}});
		LinkedImage img;
		img.link(obj.data(), obj.size(), imports);
		const U8* p = getCodePtr(img);
		Uptr base = img.result.definedSymbols[0].address;
		Uptr got0 = Uptr(Iptr(base + 0) + 4 + readI32(p + 0));
		Uptr got1 = Uptr(Iptr(base + 4) + 4 + readI32(p + 4));
		Uptr got2 = Uptr(Iptr(base + 8) + 4 + readI32(p + 8));
		CHECK_NE(got0, got1);
		CHECK_NE(got1, got2);
		CHECK_EQ(readU64(reinterpret_cast<const U8*>(got0)), U64(0x1000));
		CHECK_EQ(readU64(reinterpret_cast<const U8*>(got1)), U64(0x2000));
		CHECK_EQ(readU64(reinterpret_cast<const U8*>(got2)), U64(0x3000));
	}

} // anonymous namespace

I32 execObjectLinkerTest(int argc, char** argv)
{
	WAVM_SUPPRESS_UNUSED(argc);
	WAVM_SUPPRESS_UNUSED(argv);

	TEST_STATE_LOCAL;
	Timing::Timer timer;

	// COFF x86-64
	testCOFFX64Addr64(TEST_STATE_ARG);
	testCOFFX64Addr32NB(TEST_STATE_ARG);
	testCOFFX64Rel32Near(TEST_STATE_ARG);
	testCOFFX64Rel32Far(TEST_STATE_ARG);
	testCOFFX64Rel32_1(TEST_STATE_ARG);
	testCOFFX64Rel32_2(TEST_STATE_ARG);
	testCOFFX64Rel32_3(TEST_STATE_ARG);
	testCOFFX64Rel32_4(TEST_STATE_ARG);
	testCOFFX64Rel32_5(TEST_STATE_ARG);
	testCOFFX64Section(TEST_STATE_ARG);
	testCOFFX64SecRel(TEST_STATE_ARG);

	// COFF ARM64 (existing)
	testCOFFARM64OutOfRangePageOffset12A(TEST_STATE_ARG);
	testCOFFARM64OutOfRangePageOffset12L(TEST_STATE_ARG);
	testCOFFARM64OutOfRangePageOffset12LWithAddend(TEST_STATE_ARG);
	testCOFFARM64OutOfRangePageOffset12LStore(TEST_STATE_ARG);
	testCOFFARM64OutOfRangePageOffset12L32Bit(TEST_STATE_ARG);

	// COFF ARM64 (new)
	testCOFFARM64Addr64(TEST_STATE_ARG);
	testCOFFARM64Addr32NB(TEST_STATE_ARG);
	testCOFFARM64Branch26Near(TEST_STATE_ARG);
	testCOFFARM64Branch26Far(TEST_STATE_ARG);
	testCOFFARM64PagebaseAndAddNear(TEST_STATE_ARG);
	testCOFFARM64PagebaseAndLdstNear(TEST_STATE_ARG);
	testCOFFARM64SecRel(TEST_STATE_ARG);
	testCOFFARM64Section(TEST_STATE_ARG);
	testCOFFARM64SecRelLow12A(TEST_STATE_ARG);
	testCOFFARM64SecRelHigh12A(TEST_STATE_ARG);
	testCOFFARM64SecRelLow12L(TEST_STATE_ARG);

	// ELF x86-64
	testELFX64Abs64(TEST_STATE_ARG);
	testELFX64Abs32(TEST_STATE_ARG);
	testELFX64Abs32S(TEST_STATE_ARG);
	testELFX64PC32Near(TEST_STATE_ARG);
	testELFX64PC32Far(TEST_STATE_ARG);
	testELFX64PLT32(TEST_STATE_ARG);
	testELFX64PC64(TEST_STATE_ARG);
	testELFX64GotPcRel(TEST_STATE_ARG);
	testELFX64GotPcRelX(TEST_STATE_ARG);
	testELFX64RexGotPcRelX(TEST_STATE_ARG);

	// ELF AArch64
	testELFARM64Abs64(TEST_STATE_ARG);
	testELFARM64Abs32(TEST_STATE_ARG);
	testELFARM64Prel64(TEST_STATE_ARG);
	testELFARM64Prel32(TEST_STATE_ARG);
	testELFARM64Call26Near(TEST_STATE_ARG);
	testELFARM64Call26Far(TEST_STATE_ARG);
	testELFARM64Jump26Near(TEST_STATE_ARG);
	testELFARM64AdrpAndAddLo12(TEST_STATE_ARG);
	testELFARM64Ldst8Lo12(TEST_STATE_ARG);
	testELFARM64Ldst16Lo12(TEST_STATE_ARG);
	testELFARM64Ldst32Lo12(TEST_STATE_ARG);
	testELFARM64Ldst64Lo12(TEST_STATE_ARG);
	testELFARM64Ldst128Lo12(TEST_STATE_ARG);
	testELFARM64MovwG0(TEST_STATE_ARG);
	testELFARM64MovwG0NC(TEST_STATE_ARG);
	testELFARM64MovwG1(TEST_STATE_ARG);
	testELFARM64MovwG1NC(TEST_STATE_ARG);
	testELFARM64MovwG2(TEST_STATE_ARG);
	testELFARM64MovwG2NC(TEST_STATE_ARG);
	testELFARM64MovwG3(TEST_STATE_ARG);
	testELFARM64GotPage(TEST_STATE_ARG);

	// Mach-O x86-64
	testMachOX64Unsigned64(TEST_STATE_ARG);
	testMachOX64Unsigned32(TEST_STATE_ARG);
	testMachOX64SignedNear(TEST_STATE_ARG);
	testMachOX64SignedFar(TEST_STATE_ARG);
	testMachOX64Branch(TEST_STATE_ARG);
	testMachOX64GotLoad(TEST_STATE_ARG);
	testMachOX64Got(TEST_STATE_ARG);
	testMachOX64GotDataPcRel(TEST_STATE_ARG);
	testMachOX64Signed1(TEST_STATE_ARG);
	testMachOX64Signed2(TEST_STATE_ARG);
	testMachOX64Signed4(TEST_STATE_ARG);

	// Mach-O AArch64
	testMachOARM64Unsigned64(TEST_STATE_ARG);
	testMachOARM64Branch26Near(TEST_STATE_ARG);
	testMachOARM64Branch26Far(TEST_STATE_ARG);
	testMachOARM64Page21AndPageOff12Add(TEST_STATE_ARG);
	testMachOARM64Page21AndPageOff12Ldr(TEST_STATE_ARG);
	testMachOARM64GotLoadPage21AndPageOff12(TEST_STATE_ARG);
	testMachOARM64PointerToGotPcRel(TEST_STATE_ARG);
	testMachOARM64Addend(TEST_STATE_ARG);

	// Mach-O compact unwind
	testMachOCompactUnwindMultiPage(TEST_STATE_ARG);

	// Stub/GOT stress tests
	testX64StubCachingManyRelocs(TEST_STATE_ARG);
	testX64StubMultipleTargets(TEST_STATE_ARG);
	testARM64StubCachingManyRelocs(TEST_STATE_ARG);
	testGotCachingSameSymbol(TEST_STATE_ARG);
	testGotMultipleSymbols(TEST_STATE_ARG);

	Timing::logTimer("ObjectLinkerTest", timer);
	return testState.exitCode();
}
