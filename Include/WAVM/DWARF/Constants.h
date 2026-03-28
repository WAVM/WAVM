#pragma once

#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace DWARF {

	// DWARF tag constants.
	inline constexpr U16 DW_TAG_compile_unit = 0x11;
	inline constexpr U16 DW_TAG_subprogram = 0x2E;
	inline constexpr U16 DW_TAG_lexical_block = 0x0B;
	inline constexpr U16 DW_TAG_inlined_subroutine = 0x1D;

	// DWARF attribute constants.
	inline constexpr U16 DW_AT_name = 0x03;
	inline constexpr U16 DW_AT_stmt_list = 0x10;
	inline constexpr U16 DW_AT_low_pc = 0x11;
	inline constexpr U16 DW_AT_high_pc = 0x12;
	inline constexpr U16 DW_AT_abstract_origin = 0x31;
	inline constexpr U16 DW_AT_decl_line = 0x3B;
	inline constexpr U16 DW_AT_call_line = 0x59;
	inline constexpr U16 DW_AT_linkage_name = 0x6E;
	inline constexpr U16 DW_AT_str_offsets_base = 0x72;
	inline constexpr U16 DW_AT_addr_base = 0x73;

	// DWARF form constants.
	inline constexpr U8 DW_FORM_addr = 0x01;
	inline constexpr U8 DW_FORM_block2 = 0x03;
	inline constexpr U8 DW_FORM_block4 = 0x04;
	inline constexpr U8 DW_FORM_data2 = 0x05;
	inline constexpr U8 DW_FORM_data4 = 0x06;
	inline constexpr U8 DW_FORM_data8 = 0x07;
	inline constexpr U8 DW_FORM_string = 0x08;
	inline constexpr U8 DW_FORM_block = 0x09;
	inline constexpr U8 DW_FORM_block1 = 0x0A;
	inline constexpr U8 DW_FORM_data1 = 0x0B;
	inline constexpr U8 DW_FORM_flag = 0x0C;
	inline constexpr U8 DW_FORM_sdata = 0x0D;
	inline constexpr U8 DW_FORM_strp = 0x0E;
	inline constexpr U8 DW_FORM_udata = 0x0F;
	inline constexpr U8 DW_FORM_ref_addr = 0x10;
	inline constexpr U8 DW_FORM_ref1 = 0x11;
	inline constexpr U8 DW_FORM_ref2 = 0x12;
	inline constexpr U8 DW_FORM_ref4 = 0x13;
	inline constexpr U8 DW_FORM_ref8 = 0x14;
	inline constexpr U8 DW_FORM_ref_udata = 0x15;
	inline constexpr U8 DW_FORM_indirect = 0x16;
	inline constexpr U8 DW_FORM_sec_offset = 0x17;
	inline constexpr U8 DW_FORM_exprloc = 0x18;
	inline constexpr U8 DW_FORM_flag_present = 0x19;
	inline constexpr U8 DW_FORM_strx = 0x1A;
	inline constexpr U8 DW_FORM_addrx = 0x1B;
	inline constexpr U8 DW_FORM_data16 = 0x1E;
	inline constexpr U8 DW_FORM_line_strp = 0x1F;
	inline constexpr U8 DW_FORM_ref_sig8 = 0x20;
	inline constexpr U8 DW_FORM_implicit_const = 0x21;
	inline constexpr U8 DW_FORM_loclistx = 0x22;
	inline constexpr U8 DW_FORM_rnglistx = 0x23;
	inline constexpr U8 DW_FORM_strx1 = 0x25;
	inline constexpr U8 DW_FORM_strx2 = 0x26;
	inline constexpr U8 DW_FORM_strx4 = 0x28;
	inline constexpr U8 DW_FORM_addrx1 = 0x29;
	inline constexpr U8 DW_FORM_addrx2 = 0x2A;
	inline constexpr U8 DW_FORM_addrx4 = 0x2C;

	// DWARF unit type constants.
	inline constexpr U8 DW_UT_compile = 0x01;

	// DWARF line number standard opcode constants.
	inline constexpr U8 DW_LNS_copy = 1;
	inline constexpr U8 DW_LNS_advance_pc = 2;
	inline constexpr U8 DW_LNS_advance_line = 3;
	inline constexpr U8 DW_LNS_const_add_pc = 8;
	inline constexpr U8 DW_LNS_fixed_advance_pc = 9;

	// DWARF line number extended opcode constants.
	inline constexpr U8 DW_LNE_end_sequence = 1;
	inline constexpr U8 DW_LNE_set_address = 2;

	// Exception handling pointer encoding constants (used in .eh_frame CIE/FDE).
	inline constexpr U8 DW_EH_PE_absptr = 0x00;
	inline constexpr U8 DW_EH_PE_uleb128 = 0x01;
	inline constexpr U8 DW_EH_PE_udata2 = 0x02;
	inline constexpr U8 DW_EH_PE_udata4 = 0x03;
	inline constexpr U8 DW_EH_PE_udata8 = 0x04;
	inline constexpr U8 DW_EH_PE_signed = 0x08;
	inline constexpr U8 DW_EH_PE_sleb128 = 0x09;
	inline constexpr U8 DW_EH_PE_sdata2 = 0x0A;
	inline constexpr U8 DW_EH_PE_sdata4 = 0x0B;
	inline constexpr U8 DW_EH_PE_sdata8 = 0x0C;
	inline constexpr U8 DW_EH_PE_pcrel = 0x10;
	inline constexpr U8 DW_EH_PE_indirect = 0x80;
	inline constexpr U8 DW_EH_PE_omit = 0xFF;

	// Get the byte size of a DW_EH_PE pointer encoding (0 for variable-length encodings).
	inline Uptr getEncodedPointerSize(U8 encoding)
	{
		switch(encoding & 0x0F)
		{
		case DW_EH_PE_absptr: return sizeof(void*);
		case DW_EH_PE_uleb128:
		case DW_EH_PE_sleb128: return 0;
		case DW_EH_PE_udata2:
		case DW_EH_PE_sdata2: return 2;
		case DW_EH_PE_udata4:
		case DW_EH_PE_sdata4: return 4;
		case DW_EH_PE_signed: return sizeof(void*);
		case DW_EH_PE_udata8:
		case DW_EH_PE_sdata8: return 8;
		default: return 0;
		}
	}

}}
