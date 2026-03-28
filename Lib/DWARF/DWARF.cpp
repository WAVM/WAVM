// Signal-safe, zero-allocation DWARF v4/v5 parser for source location lookup.
// Parses .debug_info, .debug_abbrev, and .debug_str on-the-fly to resolve
// an instruction address to a chain of source locations (for inline functions).

#include "WAVM/DWARF/DWARF.h"
#include <cinttypes>
#include <cstring>
#include "WAVM/DWARF/Constants.h"
#include "WAVM/DWARF/Sections.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/LEB128.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Logging/Logging.h"

using namespace WAVM;
using namespace WAVM::DWARF;
using namespace WAVM::Serialization;

static const char* getFormName(U8 form)
{
	switch(form)
	{
	case DW_FORM_addr: return "addr";
	case DW_FORM_block2: return "block2";
	case DW_FORM_block4: return "block4";
	case DW_FORM_data2: return "data2";
	case DW_FORM_data4: return "data4";
	case DW_FORM_data8: return "data8";
	case DW_FORM_string: return "string";
	case DW_FORM_block: return "block";
	case DW_FORM_block1: return "block1";
	case DW_FORM_data1: return "data1";
	case DW_FORM_flag: return "flag";
	case DW_FORM_sdata: return "sdata";
	case DW_FORM_strp: return "strp";
	case DW_FORM_udata: return "udata";
	case DW_FORM_ref_addr: return "ref_addr";
	case DW_FORM_ref1: return "ref1";
	case DW_FORM_ref2: return "ref2";
	case DW_FORM_ref4: return "ref4";
	case DW_FORM_ref8: return "ref8";
	case DW_FORM_ref_udata: return "ref_udata";
	case DW_FORM_indirect: return "indirect";
	case DW_FORM_sec_offset: return "sec_offset";
	case DW_FORM_exprloc: return "exprloc";
	case DW_FORM_flag_present: return "flag_present";
	case DW_FORM_ref_sig8: return "ref_sig8";
	case DW_FORM_strx: return "strx";
	case DW_FORM_addrx: return "addrx";
	case DW_FORM_line_strp: return "line_strp";
	case DW_FORM_implicit_const: return "implicit_const";
	case DW_FORM_strx1: return "strx1";
	case DW_FORM_strx2: return "strx2";
	case DW_FORM_strx4: return "strx4";
	case DW_FORM_addrx1: return "addrx1";
	case DW_FORM_addrx2: return "addrx2";
	case DW_FORM_addrx4: return "addrx4";
	case DW_FORM_data16: return "data16";
	case DW_FORM_rnglistx: return "rnglistx";
	case DW_FORM_loclistx: return "loclistx";
	default: return "?";
	}
}

static const char* getTagName(U16 tag)
{
	switch(tag)
	{
	case DW_TAG_compile_unit: return "compile_unit";
	case DW_TAG_subprogram: return "subprogram";
	case DW_TAG_inlined_subroutine: return "inlined_subroutine";
	default: return "?";
	}
}

static const char* getAttrName(U16 attr)
{
	switch(attr)
	{
	case DW_AT_name: return "name";
	case DW_AT_low_pc: return "low_pc";
	case DW_AT_high_pc: return "high_pc";
	case DW_AT_stmt_list: return "stmt_list";
	case DW_AT_call_line: return "call_line";
	case DW_AT_linkage_name: return "linkage_name";
	case DW_AT_abstract_origin: return "abstract_origin";
	case DW_AT_decl_line: return "decl_line";
	case DW_AT_str_offsets_base: return "str_offsets_base";
	case DW_AT_addr_base: return "addr_base";
	default: return "?";
	}
}

// Skip a DIE attribute value of the given form.
// Returns false if the form is unknown.
static bool skipForm(U8 form,
					 InputStream& stream,
					 U8 addrSize,
					 U8 dwarfFormat, // 4 or 8 (DWARF32 vs DWARF64)
					 I64 implicitConst)
{
	U64 tmp;
	I64 stmp;
	(void)implicitConst;
	switch(form)
	{
	case DW_FORM_addr:
		if(!stream.tryAdvance(addrSize)) { return false; }
		break;
	case DW_FORM_data1:
	case DW_FORM_ref1:
	case DW_FORM_flag:
	case DW_FORM_strx1:
	case DW_FORM_addrx1:
		if(!stream.tryAdvance(1)) { return false; }
		break;
	case DW_FORM_data2:
	case DW_FORM_ref2:
	case DW_FORM_strx2:
	case DW_FORM_addrx2:
		if(!stream.tryAdvance(2)) { return false; }
		break;
	case DW_FORM_data4:
	case DW_FORM_ref4:
	case DW_FORM_strx4:
	case DW_FORM_addrx4:
		if(!stream.tryAdvance(4)) { return false; }
		break;
	case DW_FORM_data8:
	case DW_FORM_ref8:
	case DW_FORM_ref_sig8:
		if(!stream.tryAdvance(8)) { return false; }
		break;
	case DW_FORM_data16:
		if(!stream.tryAdvance(16)) { return false; }
		break;
	case DW_FORM_sdata: return trySerializeVarSInt64(stream, stmp);
	case DW_FORM_udata:
	case DW_FORM_ref_udata:
	case DW_FORM_strx:
	case DW_FORM_addrx:
	case DW_FORM_rnglistx:
	case DW_FORM_loclistx: return trySerializeVarUInt64(stream, tmp);
	case DW_FORM_string: {
		// Scan for null terminator.
		const U8* bytePtr;
		do
		{
			bytePtr = stream.tryAdvance(1);
			if(!bytePtr) { return false; }
		} while(*bytePtr != 0);
		break;
	}
	case DW_FORM_strp:
	case DW_FORM_sec_offset:
	case DW_FORM_line_strp:
	case DW_FORM_ref_addr:
		if(!stream.tryAdvance(dwarfFormat == 8 ? 8 : 4)) { return false; }
		break;
	case DW_FORM_block1: {
		U8 len;
		if(!tryRead(stream, len)) { return false; }
		if(!stream.tryAdvance(len)) { return false; }
		break;
	}
	case DW_FORM_block2: {
		U16 len;
		if(!tryRead(stream, len)) { return false; }
		if(!stream.tryAdvance(len)) { return false; }
		break;
	}
	case DW_FORM_block4: {
		U32 len;
		if(!tryRead(stream, len)) { return false; }
		if(!stream.tryAdvance(len)) { return false; }
		break;
	}
	case DW_FORM_block:
	case DW_FORM_exprloc: {
		U64 len;
		if(!trySerializeVarUInt64(stream, len)) { return false; }
		if(!stream.tryAdvance(Uptr(len))) { return false; }
		break;
	}
	case DW_FORM_flag_present: break;   // zero size
	case DW_FORM_implicit_const: break; // value is in the abbreviation, not in .debug_info
	case DW_FORM_indirect: {
		U64 actualForm;
		if(!trySerializeVarUInt64(stream, actualForm)) { return false; }
		return skipForm(U8(actualForm), stream, addrSize, dwarfFormat, 0);
	}
	default: return false;
	}
	return true;
}

// Look up an address by index in .debug_addr.
// addrBase points past the section header to the first entry.
static Uptr lookupAddrx(Uptr index,
						U8 addrSize,
						const U8* debugAddr,
						Uptr debugAddrSize,
						Uptr addrBase)
{
	if(!debugAddr) { return 0; }
	Uptr offset = addrBase + index * addrSize;
	if(offset + addrSize > debugAddrSize) { return 0; }
	if(addrSize == 8)
	{
		U64 v;
		memcpy(&v, debugAddr + offset, 8);
		return Uptr(v);
	}
	else if(addrSize == 4)
	{
		U32 v;
		memcpy(&v, debugAddr + offset, 4);
		return Uptr(v);
	}
	return 0;
}

// Read an attribute value as a Uptr, for forms that represent addresses or references.
static Uptr readAddrFormValue(U8 form,
							  InputStream& stream,
							  U8 addrSize,
							  U8 dwarfFormat,
							  const U8* debugAddr,
							  Uptr debugAddrSize,
							  Uptr addrBase)
{
	switch(form)
	{
	case DW_FORM_addr: {
		if(addrSize == 8)
		{
			U64 v;
			if(!tryRead(stream, v)) { return 0; }
			return Uptr(v);
		}
		else if(addrSize == 4)
		{
			U32 v;
			if(!tryRead(stream, v)) { return 0; }
			return Uptr(v);
		}
		return 0;
	}
	case DW_FORM_addrx: {
		U64 index;
		if(!trySerializeVarUInt64(stream, index)) { return 0; }
		return lookupAddrx(Uptr(index), addrSize, debugAddr, debugAddrSize, addrBase);
	}
	case DW_FORM_addrx1: {
		U8 index;
		if(!tryRead(stream, index)) { return 0; }
		return lookupAddrx(Uptr(index), addrSize, debugAddr, debugAddrSize, addrBase);
	}
	case DW_FORM_addrx2: {
		U16 index;
		if(!tryRead(stream, index)) { return 0; }
		return lookupAddrx(Uptr(index), addrSize, debugAddr, debugAddrSize, addrBase);
	}
	case DW_FORM_addrx4: {
		U32 index;
		if(!tryRead(stream, index)) { return 0; }
		return lookupAddrx(Uptr(index), addrSize, debugAddr, debugAddrSize, addrBase);
	}
	case DW_FORM_data1: {
		U8 v;
		if(!tryRead(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_data2: {
		U16 v;
		if(!tryRead(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_data4: {
		U32 v;
		if(!tryRead(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_data8: {
		U64 v;
		if(!tryRead(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_udata: {
		U64 v;
		if(!trySerializeVarUInt64(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_sdata: {
		I64 v;
		if(!trySerializeVarSInt64(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_sec_offset: {
		if(dwarfFormat == 8)
		{
			U64 v;
			if(!tryRead(stream, v)) { return 0; }
			return Uptr(v);
		}
		else
		{
			U32 v;
			if(!tryRead(stream, v)) { return 0; }
			return Uptr(v);
		}
	}
	default:
		// Not an address form; skip it.
		skipForm(form, stream, addrSize, dwarfFormat, 0);
		return 0;
	}
}

// Look up a string by index in .debug_str_offsets, then .debug_str.
// strOffsetsBase points past the section header to the first offset entry.
static const char* lookupStrx(Uptr index,
							  U8 dwarfFormat,
							  const U8* debugStrOffsets,
							  Uptr debugStrOffsetsSize,
							  Uptr strOffsetsBase,
							  const U8* debugStr,
							  Uptr debugStrSize)
{
	if(!debugStrOffsets || !debugStr) { return nullptr; }
	U8 offsetSize = (dwarfFormat == 8) ? 8 : 4;
	Uptr entryOffset = strOffsetsBase + index * offsetSize;
	if(entryOffset + offsetSize > debugStrOffsetsSize) { return nullptr; }

	Uptr strOffset;
	if(offsetSize == 8)
	{
		U64 v;
		memcpy(&v, debugStrOffsets + entryOffset, 8);
		strOffset = Uptr(v);
	}
	else
	{
		U32 v;
		memcpy(&v, debugStrOffsets + entryOffset, 4);
		strOffset = Uptr(v);
	}

	if(strOffset < debugStrSize) { return reinterpret_cast<const char*>(debugStr + strOffset); }
	return nullptr;
}

// Read a string attribute value (DW_FORM_strp, DW_FORM_string, DW_FORM_strx*).
static const char* readStrpValue(U8 form,
								 InputStream& stream,
								 U8 dwarfFormat,
								 const U8* debugStr,
								 Uptr debugStrSize,
								 const U8* debugStrOffsets,
								 Uptr debugStrOffsetsSize,
								 Uptr strOffsetsBase)
{
	if(form == DW_FORM_strp)
	{
		Uptr offset;
		if(dwarfFormat == 8)
		{
			U64 v;
			if(!tryRead(stream, v)) { return nullptr; }
			offset = Uptr(v);
		}
		else
		{
			U32 v;
			if(!tryRead(stream, v)) { return nullptr; }
			offset = Uptr(v);
		}
		if(debugStr && offset < debugStrSize)
		{
			return reinterpret_cast<const char*>(debugStr + offset);
		}
		return nullptr;
	}
	else if(form == DW_FORM_string)
	{
		// Read the first byte to get a pointer into the underlying buffer.
		const U8* bytePtr = stream.tryAdvance(1);
		if(!bytePtr) { return nullptr; }
		const char* str = reinterpret_cast<const char*>(bytePtr);
		// Scan past remaining bytes until null terminator.
		while(*bytePtr != 0)
		{
			bytePtr = stream.tryAdvance(1);
			if(!bytePtr) { return str; }
		}
		return str;
	}
	else if(form == DW_FORM_strx)
	{
		U64 index;
		if(!trySerializeVarUInt64(stream, index)) { return nullptr; }
		return lookupStrx(Uptr(index),
						  dwarfFormat,
						  debugStrOffsets,
						  debugStrOffsetsSize,
						  strOffsetsBase,
						  debugStr,
						  debugStrSize);
	}
	else if(form == DW_FORM_strx1)
	{
		U8 index;
		if(!tryRead(stream, index)) { return nullptr; }
		return lookupStrx(Uptr(index),
						  dwarfFormat,
						  debugStrOffsets,
						  debugStrOffsetsSize,
						  strOffsetsBase,
						  debugStr,
						  debugStrSize);
	}
	else if(form == DW_FORM_strx2)
	{
		U16 index;
		if(!tryRead(stream, index)) { return nullptr; }
		return lookupStrx(Uptr(index),
						  dwarfFormat,
						  debugStrOffsets,
						  debugStrOffsetsSize,
						  strOffsetsBase,
						  debugStr,
						  debugStrSize);
	}
	else if(form == DW_FORM_strx4)
	{
		U32 index;
		if(!tryRead(stream, index)) { return nullptr; }
		return lookupStrx(Uptr(index),
						  dwarfFormat,
						  debugStrOffsets,
						  debugStrOffsetsSize,
						  strOffsetsBase,
						  debugStr,
						  debugStrSize);
	}
	skipForm(form, stream, 8, dwarfFormat, 0);
	return nullptr;
}

// Read a reference (offset within the CU) from a DW_FORM_ref* attribute.
static Uptr readRefValue(U8 form, InputStream& stream, U8 dwarfFormat)
{
	switch(form)
	{
	case DW_FORM_ref1: {
		U8 v;
		if(!tryRead(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_ref2: {
		U16 v;
		if(!tryRead(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_ref4: {
		U32 v;
		if(!tryRead(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_ref8: {
		U64 v;
		if(!tryRead(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_ref_udata: {
		U64 v;
		if(!trySerializeVarUInt64(stream, v)) { return 0; }
		return Uptr(v);
	}
	case DW_FORM_sec_offset:
	case DW_FORM_ref_addr: {
		if(dwarfFormat == 8)
		{
			U64 v;
			if(!tryRead(stream, v)) { return 0; }
			return Uptr(v);
		}
		else
		{
			U32 v;
			if(!tryRead(stream, v)) { return 0; }
			return Uptr(v);
		}
	}
	default: skipForm(form, stream, 8, dwarfFormat, 0); return 0;
	}
}

static constexpr Uptr maxAttrsPerAbbrev = 32;

struct AbbrevAttr
{
	U16 name;
	U8 form;
	I64 implicitConst;
};

struct AbbrevEntry
{
	U64 code;
	U16 tag;
	bool hasChildren;
	AbbrevAttr attrs[maxAttrsPerAbbrev];
	Uptr numAttrs;
};

// Find an abbreviation entry by code directly in .debug_abbrev, decoding only the matching entry.
// Returns true if found, filling outEntry.
static bool findAbbrev(const U8* debugAbbrev,
					   Uptr debugAbbrevSize,
					   Uptr abbrevOffset,
					   U64 targetCode,
					   AbbrevEntry& outEntry)
{
	if(!debugAbbrev || abbrevOffset >= debugAbbrevSize) { return false; }

	MemoryInputStream stream(debugAbbrev + abbrevOffset, debugAbbrevSize - abbrevOffset);

	while(stream.capacity())
	{
		U64 code;
		if(!trySerializeVarUInt64(stream, code)) { break; }
		if(code == 0) { break; }

		U64 tagVal;
		if(!trySerializeVarUInt64(stream, tagVal)) { break; }
		U16 tag = U16(tagVal);
		U8 childrenByte;
		if(!tryRead(stream, childrenByte)) { break; }
		bool hasChildren = (childrenByte != 0);

		if(code == targetCode)
		{
			outEntry.code = code;
			outEntry.tag = tag;
			outEntry.hasChildren = hasChildren;
			outEntry.numAttrs = 0;

			while(stream.capacity())
			{
				U64 attrName, attrForm;
				if(!trySerializeVarUInt64(stream, attrName)) { break; }
				if(!trySerializeVarUInt64(stream, attrForm)) { break; }
				if(attrName == 0 && attrForm == 0) { break; }

				I64 implicitConst = 0;
				if(attrForm == DW_FORM_implicit_const)
				{
					trySerializeVarSInt64(stream, implicitConst);
				}

				if(outEntry.numAttrs < maxAttrsPerAbbrev)
				{
					outEntry.attrs[outEntry.numAttrs].name = U16(attrName);
					outEntry.attrs[outEntry.numAttrs].form = U8(attrForm);
					outEntry.attrs[outEntry.numAttrs].implicitConst = implicitConst;
					++outEntry.numAttrs;
				}
			}
			return true;
		}

		// Skip this entry's attributes.
		while(stream.capacity())
		{
			U64 attrName, attrForm;
			if(!trySerializeVarUInt64(stream, attrName)) { break; }
			if(!trySerializeVarUInt64(stream, attrForm)) { break; }
			if(attrName == 0 && attrForm == 0) { break; }
			if(attrForm == DW_FORM_implicit_const)
			{
				I64 tmp;
				trySerializeVarSInt64(stream, tmp);
			}
		}
	}
	return false;
}

// Read name/linkage_name from a DIE at the given offset within .debug_info.
static void readDIENames(const U8* debugInfo,
						 Uptr debugInfoSize,
						 Uptr dieOffset,
						 const U8* debugAbbrev,
						 Uptr debugAbbrevSize,
						 Uptr abbrevTableOffset,
						 U8 addrSize,
						 U8 dwarfFormat,
						 const U8* debugStr,
						 Uptr debugStrSize,
						 const U8* debugStrOffsets,
						 Uptr debugStrOffsetsSize,
						 Uptr strOffsetsBase,
						 const char*& outName,
						 const char*& outLinkageName)
{
	if(dieOffset >= debugInfoSize) { return; }

	MemoryInputStream stream(debugInfo + dieOffset, debugInfoSize - dieOffset);

	U64 abbrevCode;
	if(!trySerializeVarUInt64(stream, abbrevCode)) { return; }
	if(abbrevCode == 0) { return; }

	AbbrevEntry abbrevBuf;
	if(!findAbbrev(debugAbbrev, debugAbbrevSize, abbrevTableOffset, abbrevCode, abbrevBuf))
	{
		return;
	}
	const AbbrevEntry* abbrev = &abbrevBuf;

	for(Uptr i = 0; i < abbrev->numAttrs && stream.capacity(); ++i)
	{
		U16 attrName = abbrev->attrs[i].name;
		U8 attrForm = abbrev->attrs[i].form;

		if(attrName == DW_AT_name || attrName == DW_AT_linkage_name)
		{
			const char* str = readStrpValue(attrForm,
											stream,
											dwarfFormat,
											debugStr,
											debugStrSize,
											debugStrOffsets,
											debugStrOffsetsSize,
											strOffsetsBase);
			if(str)
			{
				if(attrName == DW_AT_name) { outName = str; }
				else
				{
					outLinkageName = str;
				}
			}
		}
		else
		{
			skipForm(attrForm, stream, addrSize, dwarfFormat, abbrev->attrs[i].implicitConst);
		}
	}
}

// Look up the line number at a given address in the DWARF line number program.
// Returns the line number, or 0 if not found.
// Signal-safe: no allocations.
static Uptr lookupLineAtAddress(const U8* debugLine,
								Uptr debugLineSize,
								Uptr stmtListOffset,
								Uptr targetAddress,
								U8 addrSize)
{
	if(!debugLine || stmtListOffset >= debugLineSize) { return 0; }

	MemoryInputStream lineStream(debugLine + stmtListOffset, debugLineSize - stmtListOffset);

	// Read unit_length.
	U32 initialLength;
	if(!tryRead(lineStream, initialLength)) { return 0; }
	U8 fmt;
	Uptr unitLength;
	if(initialLength == 0xFFFFFFFF)
	{
		U64 len64;
		if(!tryRead(lineStream, len64)) { return 0; }
		fmt = 8;
		unitLength = Uptr(len64);
	}
	else
	{
		fmt = 4;
		unitLength = Uptr(initialLength);
	}

	// Create a sub-stream for this unit's data.
	if(unitLength > lineStream.capacity()) { return 0; }
	MemoryInputStream stream(lineStream.tryAdvance(unitLength), unitLength);

	// Version.
	U16 version;
	if(!tryRead(stream, version)) { return 0; }

	Log::printf(Log::traceDwarf,
				"  Line table: version=%u fmt=%u unitLength=%" WAVM_PRIuPTR "\n",
				version,
				fmt,
				unitLength);

	// DWARF v5 has address_size and segment_selector_size before header_length.
	if(version >= 5)
	{
		if(!stream.tryAdvance(2)) { return 0; } // skip address_size and segment_selector_size
	}

	// header_length.
	Uptr headerLength;
	if(fmt == 8)
	{
		U64 hl;
		if(!tryRead(stream, hl)) { return 0; }
		headerLength = Uptr(hl);
	}
	else
	{
		U32 hl;
		if(!tryRead(stream, hl)) { return 0; }
		headerLength = Uptr(hl);
	}

	Uptr programStartPos = stream.position() + headerLength;
	if(programStartPos > unitLength) { return 0; }

	// Read header fields.
	U8 minInstructionLength;
	if(!tryRead(stream, minInstructionLength)) { return 0; }
	if(version >= 4)
	{
		if(!stream.tryAdvance(1)) { return 0; } // skip max_operations_per_instruction
	}
	if(!stream.tryAdvance(1)) { return 0; } // skip default_is_stmt
	U8 lineBaseU8;
	if(!tryRead(stream, lineBaseU8)) { return 0; }
	I8 lineBase = I8(lineBaseU8);
	U8 lineRange;
	if(!tryRead(stream, lineRange)) { return 0; }
	U8 opcodeBase;
	if(!tryRead(stream, opcodeBase)) { return 0; }

	Log::printf(Log::traceDwarf,
				"  Line header: minInsnLen=%u lineBase=%d lineRange=%u opcodeBase=%u\n",
				minInstructionLength,
				(int)lineBase,
				lineRange,
				opcodeBase);

	// Record standard opcode lengths (pointer into the underlying buffer).
	if(opcodeBase < 1) { return 0; }
	const U8* stdOpLengths = stream.tryAdvance(opcodeBase - 1);
	if(!stdOpLengths) { return 0; }

	// Jump to program start.
	stream.seek(programStartPos);

	// State machine.
	Uptr smAddress = 0;
	I64 smLine = 1;
	Uptr bestLine = 0;
	bool found = false;

	while(stream.capacity())
	{
		U8 opcode;
		if(!tryRead(stream, opcode)) { break; }

		if(opcode == 0)
		{
			// Extended opcode.
			U64 extLen;
			if(!trySerializeVarUInt64(stream, extLen)) { break; }
			if(extLen > stream.capacity()) { break; }
			Uptr extEndPos = stream.position() + Uptr(extLen);
			if(extLen == 0)
			{
				stream.seek(extEndPos);
				continue;
			}
			U8 extOp;
			if(!tryRead(stream, extOp)) { break; }

			switch(extOp)
			{
			case DW_LNE_end_sequence:
				Log::printf(Log::traceDwarf,
							"  Line: end_sequence addr=0x%" WAVM_PRIxPTR " line=%" PRId64
							" found=%d bestLine=%" WAVM_PRIuPTR "\n",
							smAddress,
							smLine,
							(int)found,
							bestLine);
				if(found && targetAddress < smAddress) { return bestLine; }
				// Reset.
				smAddress = 0;
				smLine = 1;
				found = false;
				bestLine = 0;
				break;
			case DW_LNE_set_address: {
				if(addrSize == 8)
				{
					U64 addr;
					if(tryRead(stream, addr)) { smAddress = Uptr(addr); }
				}
				else if(addrSize == 4)
				{
					U32 addr;
					if(tryRead(stream, addr)) { smAddress = Uptr(addr); }
				}
				Log::printf(
					Log::traceDwarf, "  Line: set_address 0x%" WAVM_PRIxPTR "\n", smAddress);
				break;
			}
			default: break;
			}
			stream.seek(extEndPos);
		}
		else if(opcode < opcodeBase)
		{
			// Standard opcode.
			switch(opcode)
			{
			case DW_LNS_copy:
				Log::printf(Log::traceDwarf,
							"  Line: copy addr=0x%" WAVM_PRIxPTR " line=%" PRId64 "\n",
							smAddress,
							smLine);
				if(smAddress <= targetAddress)
				{
					bestLine = Uptr(smLine);
					found = true;
				}
				else if(found) { return bestLine; }
				break;
			case DW_LNS_advance_pc: {
				U64 adv;
				if(!trySerializeVarUInt64(stream, adv)) { break; }
				smAddress += Uptr(adv) * minInstructionLength;
				break;
			}
			case DW_LNS_advance_line: {
				I64 adv;
				if(!trySerializeVarSInt64(stream, adv)) { break; }
				smLine += adv;
				break;
			}
			case DW_LNS_const_add_pc: {
				smAddress += Uptr((255 - opcodeBase) / lineRange) * minInstructionLength;
				break;
			}
			case DW_LNS_fixed_advance_pc: {
				U16 delta;
				if(tryRead(stream, delta)) { smAddress += delta; }
				break;
			}
			default: {
				// Skip operands using standard opcode lengths.
				U8 numOps = (opcode > 0 && opcode <= opcodeBase - 1) ? stdOpLengths[opcode - 1] : 0;
				for(U8 j = 0; j < numOps; j++)
				{
					U64 tmp;
					trySerializeVarUInt64(stream, tmp);
				}
				break;
			}
			}
		}
		else
		{
			// Special opcode: advance address and line, emit row.
			U8 adjusted = opcode - opcodeBase;
			Uptr addrAdv = Uptr(adjusted / lineRange) * minInstructionLength;
			I64 lineAdv = lineBase + (adjusted % lineRange);
			smAddress += addrAdv;
			smLine += lineAdv;
			Log::printf(Log::traceDwarf,
						"  Line: special addr=0x%" WAVM_PRIxPTR " line=%" PRId64 "\n",
						smAddress,
						smLine);
			// Emit row.
			if(smAddress <= targetAddress)
			{
				bestLine = Uptr(smLine);
				found = true;
			}
			else if(found) { return bestLine; }
		}
	}

	Log::printf(Log::traceDwarf,
				"  Line lookup result: found=%d bestLine=%" WAVM_PRIuPTR "\n",
				(int)found,
				bestLine);
	return found ? bestLine : 0;
}

Uptr DWARF::getSourceLocations(const Sections& sections,
							   Uptr address,
							   SourceLocation* outLocations,
							   Uptr maxLocations)
{
	if(!sections.debugInfo || !sections.debugAbbrev || maxLocations == 0) { return 0; }

	Log::printf(Log::traceDwarf,
				"getSourceLocations: address=0x%" WAVM_PRIxPTR " debugInfo=%p(%" WAVM_PRIuPTR
				") debugAbbrev=%p(%" WAVM_PRIuPTR ") debugStr=%p(%" WAVM_PRIuPTR
				") debugLine=%p(%" WAVM_PRIuPTR ")\n",
				address,
				(const void*)sections.debugInfo,
				sections.debugInfoSize,
				(const void*)sections.debugAbbrev,
				sections.debugAbbrevSize,
				(const void*)sections.debugStr,
				sections.debugStrSize,
				(const void*)sections.debugLine,
				sections.debugLineSize);

	MemoryInputStream infoStream(sections.debugInfo, sections.debugInfoSize);

	// Scan CU headers to find the CU containing the address.
	while(infoStream.capacity())
	{
		Uptr cuStartPos = infoStream.position();

		// Parse CU header: initial length.
		U32 initialLength;
		if(!tryRead(infoStream, initialLength)) { break; }
		U8 dwarfFormat = 4; // DWARF32
		U64 cuLength;
		if(initialLength == 0xFFFFFFFF)
		{
			// DWARF64
			U64 len64;
			if(!tryRead(infoStream, len64)) { break; }
			dwarfFormat = 8;
			cuLength = len64;
		}
		else
		{
			cuLength = initialLength;
		}

		// Create a sub-stream for this CU's data (after the length field).
		Uptr clampedLength = Uptr(cuLength);
		if(clampedLength > infoStream.capacity()) { clampedLength = infoStream.capacity(); }
		MemoryInputStream cuStream(infoStream.tryAdvance(clampedLength), clampedLength);

		U16 version;
		if(!tryRead(cuStream, version)) { continue; }

		Log::printf(Log::traceDwarf,
					"  CU at offset %" WAVM_PRIuPTR ": version=%u dwarfFormat=%u length=%" PRIu64
					"\n",
					cuStartPos,
					version,
					dwarfFormat,
					cuLength);

		U8 addrSize;
		Uptr abbrevOffset;

		if(version >= 5)
		{
			// DWARF v5: version, unit_type, address_size, debug_abbrev_offset
			U8 unitType;
			if(!tryRead(cuStream, unitType)) { continue; }
			if(!tryRead(cuStream, addrSize)) { continue; }
			if(dwarfFormat == 8)
			{
				U64 v;
				if(!tryRead(cuStream, v)) { continue; }
				abbrevOffset = Uptr(v);
			}
			else
			{
				U32 v;
				if(!tryRead(cuStream, v)) { continue; }
				abbrevOffset = Uptr(v);
			}
			Log::printf(Log::traceDwarf,
						"  v5 header: unitType=0x%02x addrSize=%u abbrevOffset=%" WAVM_PRIuPTR "\n",
						unitType,
						addrSize,
						abbrevOffset);
		}
		else
		{
			// DWARF v4 and earlier: version, debug_abbrev_offset, address_size
			if(dwarfFormat == 8)
			{
				U64 v;
				if(!tryRead(cuStream, v)) { continue; }
				abbrevOffset = Uptr(v);
			}
			else
			{
				U32 v;
				if(!tryRead(cuStream, v)) { continue; }
				abbrevOffset = Uptr(v);
			}
			if(!tryRead(cuStream, addrSize)) { continue; }
			Log::printf(Log::traceDwarf,
						"  v4 header: addrSize=%u abbrevOffset=%" WAVM_PRIuPTR "\n",
						addrSize,
						abbrevOffset);
		}

		// Read the CU DIE to get the address range.
		U64 cuAbbrevCode;
		if(!trySerializeVarUInt64(cuStream, cuAbbrevCode)) { continue; }
		if(cuAbbrevCode == 0) { continue; }

		AbbrevEntry cuAbbrevBuf;
		if(!findAbbrev(sections.debugAbbrev,
					   sections.debugAbbrevSize,
					   abbrevOffset,
					   cuAbbrevCode,
					   cuAbbrevBuf)
		   || cuAbbrevBuf.tag != DW_TAG_compile_unit)
		{
			Log::printf(Log::traceDwarf,
						"  CU DIE: abbrevCode=%" PRIu64 ", not compile_unit, skipping\n",
						cuAbbrevCode);
			continue;
		}

		Log::printf(Log::traceDwarf,
					"  CU DIE: abbrevCode=%" PRIu64 " tag=compile_unit numAttrs=%" WAVM_PRIuPTR
					"\n",
					cuAbbrevCode,
					cuAbbrevBuf.numAttrs);

		// Log all CU DIE attributes for debugging.
		for(Uptr i = 0; i < cuAbbrevBuf.numAttrs; ++i)
		{
			Log::printf(Log::traceDwarf,
						"    attr[%" WAVM_PRIuPTR "]: %s(0x%04x) form=%s(0x%02x)\n",
						i,
						getAttrName(cuAbbrevBuf.attrs[i].name),
						cuAbbrevBuf.attrs[i].name,
						getFormName(cuAbbrevBuf.attrs[i].form),
						cuAbbrevBuf.attrs[i].form);
		}

		// Pass 1: scan the CU DIE to find DW_AT_addr_base and DW_AT_str_offsets_base.
		// These must be resolved before reading addrx/strx forms in pass 2.
		Uptr cuDieAttrsPos = cuStream.position();
		Uptr addrBase = 0;
		Uptr strOffsetsBase = 0;
		for(Uptr i = 0; i < cuAbbrevBuf.numAttrs && cuStream.capacity(); ++i)
		{
			U16 attrName = cuAbbrevBuf.attrs[i].name;
			U8 attrForm = cuAbbrevBuf.attrs[i].form;
			if(attrName == DW_AT_addr_base)
			{
				addrBase
					= readAddrFormValue(attrForm, cuStream, addrSize, dwarfFormat, nullptr, 0, 0);
				Log::printf(Log::traceDwarf, "    addr_base = %" WAVM_PRIuPTR "\n", addrBase);
			}
			else if(attrName == DW_AT_str_offsets_base)
			{
				strOffsetsBase = readRefValue(attrForm, cuStream, dwarfFormat);
				Log::printf(
					Log::traceDwarf, "    str_offsets_base = %" WAVM_PRIuPTR "\n", strOffsetsBase);
			}
			else
			{
				skipForm(
					attrForm, cuStream, addrSize, dwarfFormat, cuAbbrevBuf.attrs[i].implicitConst);
			}
		}
		cuStream.seek(cuDieAttrsPos);

		// Pass 2: read CU attributes to find low_pc/high_pc and stmt_list.
		Uptr cuLowPC = 0;
		Uptr cuHighPC = 0;
		bool highPCIsOffset = false;
		Uptr stmtListOffset = Uptr(-1);
		for(Uptr i = 0; i < cuAbbrevBuf.numAttrs && cuStream.capacity(); ++i)
		{
			U16 attrName = cuAbbrevBuf.attrs[i].name;
			U8 attrForm = cuAbbrevBuf.attrs[i].form;
			Uptr attrStartPos = cuStream.position();

			if(attrName == DW_AT_low_pc)
			{
				cuLowPC = readAddrFormValue(attrForm,
											cuStream,
											addrSize,
											dwarfFormat,
											sections.debugAddr,
											sections.debugAddrSize,
											addrBase);
				Log::printf(Log::traceDwarf,
							"    low_pc = 0x%" WAVM_PRIxPTR " (form=%s)\n",
							cuLowPC,
							getFormName(attrForm));
			}
			else if(attrName == DW_AT_high_pc)
			{
				// high_pc can be an address or a constant (offset from low_pc).
				cuHighPC = readAddrFormValue(attrForm,
											 cuStream,
											 addrSize,
											 dwarfFormat,
											 sections.debugAddr,
											 sections.debugAddrSize,
											 addrBase);
				if(attrForm != DW_FORM_addr) { highPCIsOffset = true; }
				Log::printf(Log::traceDwarf,
							"    high_pc = 0x%" WAVM_PRIxPTR " (form=%s, isOffset=%d)\n",
							cuHighPC,
							getFormName(attrForm),
							(int)highPCIsOffset);
			}
			else if(attrName == DW_AT_stmt_list)
			{
				stmtListOffset = readRefValue(attrForm, cuStream, dwarfFormat);
				Log::printf(Log::traceDwarf,
							"    stmt_list = %" WAVM_PRIuPTR " (form=%s)\n",
							stmtListOffset,
							getFormName(attrForm));
			}
			else
			{
				skipForm(
					attrForm, cuStream, addrSize, dwarfFormat, cuAbbrevBuf.attrs[i].implicitConst);
			}
			// Safety: if stream didn't advance and the form isn't zero-length, we're stuck.
			if(cuStream.position() == attrStartPos && attrForm != DW_FORM_flag_present
			   && attrForm != DW_FORM_implicit_const)
			{
				break;
			}
		}

		if(highPCIsOffset) { cuHighPC = cuLowPC + cuHighPC; }

		Log::printf(Log::traceDwarf,
					"  CU range: [0x%" WAVM_PRIxPTR ", 0x%" WAVM_PRIxPTR ") target=0x%" WAVM_PRIxPTR
					"\n",
					cuLowPC,
					cuHighPC,
					address);

		// Check if address is in this CU's range.
		if(cuLowPC == 0 && cuHighPC == 0)
		{
			// No range info; skip to next CU. (DW_AT_ranges not supported yet.)
			Log::printf(Log::traceDwarf, "  CU has no range info, skipping\n");
			continue;
		}
		if(address < cuLowPC || address >= cuHighPC)
		{
			Log::printf(Log::traceDwarf, "  Address not in CU range, skipping\n");
			continue;
		}

		Log::printf(Log::traceDwarf, "  Address is in CU range, walking DIEs\n");

		// Walk DIEs within this CU looking for subprogram/inlined_subroutine containing address.
		Uptr numLocations = 0;

		// Stack of function scopes for tracking inline nesting.
		struct ScopeEntry
		{
			const char* linkageName;
			const char* name;
			Uptr callLine;
			Uptr abstractOrigin;
		};
		static constexpr Uptr maxScopeDepth = 16;
		ScopeEntry scopeStack[maxScopeDepth];
		Uptr scopeDepth = 0;

		// Depth tracking for skipping children of non-matching DIEs.
		Uptr skipDepth = 0;
		bool skipping = false;

		// Walk-depth stack tracks what type of nesting we entered at each level
		// (scope vs transparent container), so the null DIE handler can correctly
		// determine whether to break (scope end) or just continue (transparent end).
		enum class WalkNestType : U8
		{
			scope,
			transparent
		};
		static constexpr Uptr maxWalkDepth = 32;
		WalkNestType walkStack[maxWalkDepth];
		Uptr walkDepth = 0;

		// cuStream position is now after the CU DIE attributes, at the first child DIE.
		while(cuStream.capacity())
		{
			U64 abbrevCode;
			if(!trySerializeVarUInt64(cuStream, abbrevCode)) { break; }

			if(abbrevCode == 0)
			{
				// Null DIE: end of children list.
				if(skipping)
				{
					if(skipDepth > 0) { --skipDepth; }
					else
					{
						skipping = false;
					}
				}
				else if(walkDepth > 0)
				{
					WalkNestType type = walkStack[--walkDepth];
					if(type == WalkNestType::scope)
					{
						// Exhausted children of a containing scope without finding
						// a deeper match. The scope itself is the result. Stop walking.
						break;
					}
					// Transparent container end; just continue walking siblings.
				}
				continue;
			}

			AbbrevEntry abbrevBuf;
			if(!findAbbrev(sections.debugAbbrev,
						   sections.debugAbbrevSize,
						   abbrevOffset,
						   abbrevCode,
						   abbrevBuf))
			{
				// Unknown abbreviation; can't continue.
				Log::printf(Log::traceDwarf,
							"  Unknown abbrev code %" PRIu64 ", stopping DIE walk\n",
							abbrevCode);
				break;
			}

			if(skipping)
			{
				// Skip all attributes.
				for(Uptr i = 0; i < abbrevBuf.numAttrs && cuStream.capacity(); ++i)
				{
					skipForm(abbrevBuf.attrs[i].form,
							 cuStream,
							 addrSize,
							 dwarfFormat,
							 abbrevBuf.attrs[i].implicitConst);
				}
				if(abbrevBuf.hasChildren) { ++skipDepth; }
				continue;
			}

			bool isSubprogram = (abbrevBuf.tag == DW_TAG_subprogram);
			bool isInlinedSubroutine = (abbrevBuf.tag == DW_TAG_inlined_subroutine);
			bool isInteresting = isSubprogram || isInlinedSubroutine;

			if(!isInteresting)
			{
				// Skip attributes.
				for(Uptr i = 0; i < abbrevBuf.numAttrs && cuStream.capacity(); ++i)
				{
					skipForm(abbrevBuf.attrs[i].form,
							 cuStream,
							 addrSize,
							 dwarfFormat,
							 abbrevBuf.attrs[i].implicitConst);
				}
				if(abbrevBuf.tag == DW_TAG_lexical_block && abbrevBuf.hasChildren)
				{
					// Transparent container: recurse into children to find
					// inlined subroutines nested under lexical blocks.
					if(walkDepth < maxWalkDepth)
					{
						walkStack[walkDepth++] = WalkNestType::transparent;
					}
				}
				else if(abbrevBuf.hasChildren)
				{
					skipping = true;
					skipDepth = 0;
				}
				continue;
			}

			// Parse interesting DIE attributes.
			Uptr lowPC = 0;
			Uptr highPC = 0;
			bool hpIsOffset = false;
			const char* dieName = nullptr;
			const char* dieLinkageName = nullptr;
			Uptr callLine = 0;
			Uptr abstractOrigin = 0;

			for(Uptr i = 0; i < abbrevBuf.numAttrs && cuStream.capacity(); ++i)
			{
				U16 attrName = abbrevBuf.attrs[i].name;
				U8 attrForm = abbrevBuf.attrs[i].form;

				if(attrName == DW_AT_low_pc)
				{
					lowPC = readAddrFormValue(attrForm,
											  cuStream,
											  addrSize,
											  dwarfFormat,
											  sections.debugAddr,
											  sections.debugAddrSize,
											  addrBase);
				}
				else if(attrName == DW_AT_high_pc)
				{
					highPC = readAddrFormValue(attrForm,
											   cuStream,
											   addrSize,
											   dwarfFormat,
											   sections.debugAddr,
											   sections.debugAddrSize,
											   addrBase);
					if(attrForm != DW_FORM_addr) { hpIsOffset = true; }
				}
				else if(attrName == DW_AT_name)
				{
					dieName = readStrpValue(attrForm,
											cuStream,
											dwarfFormat,
											sections.debugStr,
											sections.debugStrSize,
											sections.debugStrOffsets,
											sections.debugStrOffsetsSize,
											strOffsetsBase);
				}
				else if(attrName == DW_AT_linkage_name)
				{
					dieLinkageName = readStrpValue(attrForm,
												   cuStream,
												   dwarfFormat,
												   sections.debugStr,
												   sections.debugStrSize,
												   sections.debugStrOffsets,
												   sections.debugStrOffsetsSize,
												   strOffsetsBase);
				}
				else if(attrName == DW_AT_call_line)
				{
					callLine = readAddrFormValue(attrForm,
												 cuStream,
												 addrSize,
												 dwarfFormat,
												 sections.debugAddr,
												 sections.debugAddrSize,
												 addrBase);
				}
				else if(attrName == DW_AT_abstract_origin)
				{
					abstractOrigin = readRefValue(attrForm, cuStream, dwarfFormat);
				}
				else if(attrName == DW_AT_decl_line && isSubprogram && callLine == 0)
				{
					callLine = readAddrFormValue(attrForm,
												 cuStream,
												 addrSize,
												 dwarfFormat,
												 sections.debugAddr,
												 sections.debugAddrSize,
												 addrBase);
				}
				else
				{
					skipForm(attrForm,
							 cuStream,
							 addrSize,
							 dwarfFormat,
							 abbrevBuf.attrs[i].implicitConst);
				}
			}

			if(hpIsOffset) { highPC = lowPC + highPC; }

			Log::printf(Log::traceDwarf,
						"  DIE %s: lowPC=0x%" WAVM_PRIxPTR " highPC=0x%" WAVM_PRIxPTR
						" name=%s linkageName=%s callLine=%" WAVM_PRIuPTR
						" abstractOrigin=%" WAVM_PRIuPTR " hasChildren=%d\n",
						getTagName(abbrevBuf.tag),
						lowPC,
						highPC,
						dieName ? dieName : "(null)",
						dieLinkageName ? dieLinkageName : "(null)",
						callLine,
						abstractOrigin,
						(int)abbrevBuf.hasChildren);

			// Check if this DIE's address range contains our target address.
			bool contains = (lowPC != 0 && address >= lowPC && address < highPC);

			if(!contains)
			{
				Log::printf(Log::traceDwarf, "    -> does not contain target, skipping\n");
				// Skip children.
				if(abbrevBuf.hasChildren)
				{
					skipping = true;
					skipDepth = 0;
				}
				continue;
			}

			Log::printf(Log::traceDwarf, "    -> contains target, pushing scope\n");

			// This DIE contains the address. Push it onto the scope stack.
			if(scopeDepth < maxScopeDepth)
			{
				ScopeEntry& scope = scopeStack[scopeDepth++];
				scope.linkageName = dieLinkageName;
				scope.name = dieName;
				scope.callLine = callLine;
				scope.abstractOrigin = abstractOrigin;
			}

			// If this DIE has no children, it's a leaf, so we're done walking.
			if(!abbrevBuf.hasChildren) { break; }

			// Track this scope in the walk stack so the null DIE handler
			// knows to stop walking when this scope's children are exhausted.
			if(walkDepth < maxWalkDepth) { walkStack[walkDepth++] = WalkNestType::scope; }
		}

		Log::printf(Log::traceDwarf, "  DIE walk done: scopeDepth=%" WAVM_PRIuPTR "\n", scopeDepth);

		// Look up the line at the target address from the line table.
		// This gives the instruction index for the innermost frame.
		Uptr lineAtAddress = 0;
		if(stmtListOffset != Uptr(-1) && sections.debugLine)
		{
			Log::printf(Log::traceDwarf,
						"  Looking up line at address 0x%" WAVM_PRIxPTR
						" in line table at offset %" WAVM_PRIuPTR "\n",
						address,
						stmtListOffset);
			lineAtAddress = lookupLineAtAddress(
				sections.debugLine, sections.debugLineSize, stmtListOffset, address, addrSize);
			Log::printf(Log::traceDwarf, "  lineAtAddress = %" WAVM_PRIuPTR "\n", lineAtAddress);
		}
		// Build the output source location chain from the scope stack.
		// Innermost function first.
		for(Uptr i = scopeDepth; i > 0 && numLocations < maxLocations; --i)
		{
			ScopeEntry& scope = scopeStack[i - 1];

			// If names aren't set directly, follow abstract_origin.
			if(!scope.linkageName && !scope.name && scope.abstractOrigin != 0)
			{
				// abstract_origin is a CU-relative offset for ref1/2/4/8.
				Uptr absOffset = cuStartPos + scope.abstractOrigin;
				// For ref_addr, it's already an absolute offset, but we handled that above.
				Log::printf(Log::traceDwarf,
							"  Following abstract_origin at offset %" WAVM_PRIuPTR "\n",
							absOffset);
				if(absOffset < sections.debugInfoSize)
				{
					readDIENames(sections.debugInfo,
								 sections.debugInfoSize,
								 absOffset,
								 sections.debugAbbrev,
								 sections.debugAbbrevSize,
								 abbrevOffset,
								 addrSize,
								 dwarfFormat,
								 sections.debugStr,
								 sections.debugStrSize,
								 sections.debugStrOffsets,
								 sections.debugStrOffsetsSize,
								 strOffsetsBase,
								 scope.name,
								 scope.linkageName);
				}
			}

			outLocations[numLocations].linkageName = scope.linkageName;
			outLocations[numLocations].name = scope.name;
			// For the innermost frame, use the line table.
			// For outer frames, use DW_AT_call_line from the next inner scope
			// (which tells where the inner function was called from in this frame).
			if(i == scopeDepth) { outLocations[numLocations].line = lineAtAddress; }
			else
			{
				outLocations[numLocations].line = scopeStack[i].callLine;
			}

			Log::printf(
				Log::traceDwarf,
				"  Result[%" WAVM_PRIuPTR "]: linkageName=%s name=%s line=%" WAVM_PRIuPTR "\n",
				numLocations,
				outLocations[numLocations].linkageName ? outLocations[numLocations].linkageName
													   : "(null)",
				outLocations[numLocations].name ? outLocations[numLocations].name : "(null)",
				outLocations[numLocations].line);
			++numLocations;
		}

		Log::printf(Log::traceDwarf, "  Returning %" WAVM_PRIuPTR " locations\n", numLocations);
		return numLocations;
	}

	Log::printf(Log::traceDwarf, "  No matching CU found\n");
	return 0;
}
