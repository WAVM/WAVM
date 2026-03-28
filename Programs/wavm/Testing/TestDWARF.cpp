// Tests for the DWARF parser (WAVM::DWARF::getSourceLocations).
// Constructs synthetic DWARF sections in memory and verifies address-to-source-location lookup.

#include <cstring>
#include <vector>
#include "TestUtils.h"
#include "WAVM/DWARF/Constants.h"
#include "WAVM/DWARF/DWARF.h"
#include "WAVM/DWARF/Sections.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/LEB128.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Platform/Defines.h"
#include "wavm-test.h"

using namespace WAVM;
using namespace WAVM::DWARF;
using namespace WAVM::Testing;
using Serialization::ArrayOutputStream;
using Serialization::serializeBytes;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

template<typename T> static void write(Serialization::OutputStream& stream, const T& value)
{
	serializeBytes(stream, (const U8*)&value, sizeof(T));
}

static void writeULEB128(Serialization::OutputStream& stream, U64 value)
{
	Serialization::serializeVarUInt64(stream, value);
}

static void writeSLEB128(Serialization::OutputStream& stream, I64 value)
{
	Serialization::serializeVarInt64(stream, value);
}

// Patch a U32 at a given offset in the finalized byte vector.
static void patchU32(std::vector<U8>& bytes, Uptr offset, U32 value)
{
	memcpy(bytes.data() + offset, &value, sizeof(U32));
}

// Populate Sections from finalized vectors.
static DWARF::Sections makeSections(const std::vector<U8>& info,
									const std::vector<U8>& abbrev,
									const std::vector<U8>& str,
									const std::vector<U8>& line)
{
	DWARF::Sections s;
	s.debugInfo = info.empty() ? nullptr : info.data();
	s.debugInfoSize = info.size();
	s.debugAbbrev = abbrev.empty() ? nullptr : abbrev.data();
	s.debugAbbrevSize = abbrev.size();
	s.debugStr = str.empty() ? nullptr : str.data();
	s.debugStrSize = str.size();
	s.debugLine = line.empty() ? nullptr : line.data();
	s.debugLineSize = line.size();
	return s;
}

// ---------------------------------------------------------------------------
// .debug_abbrev builder
// ---------------------------------------------------------------------------

struct AbbrevAttrDef
{
	U16 attr;
	U8 form;
};

static void buildAbbrevEntry(Serialization::OutputStream& stream,
							 U64 code,
							 U16 tag,
							 bool hasChildren,
							 const std::vector<AbbrevAttrDef>& attrs)
{
	writeULEB128(stream, code);
	writeULEB128(stream, tag);
	write(stream, U8(hasChildren ? 1 : 0));
	for(const auto& a : attrs)
	{
		writeULEB128(stream, a.attr);
		writeULEB128(stream, a.form);
	}
	// Terminating pair
	writeULEB128(stream, U64(0));
	writeULEB128(stream, U64(0));
}

static void terminateAbbrevTable(Serialization::OutputStream& stream)
{
	writeULEB128(stream, U64(0));
}

// ---------------------------------------------------------------------------
// .debug_str builder
// ---------------------------------------------------------------------------

// Adds a null-terminated string to .debug_str stream, returns its offset.
static U32 addDebugStr(ArrayOutputStream& stream, const char* str)
{
	U32 offset = U32(stream.position());
	while(*str) { write(stream, U8(*str++)); }
	write(stream, U8(0));
	return offset;
}

// ---------------------------------------------------------------------------
// .debug_info CU header builder
// ---------------------------------------------------------------------------

// Begins a DWARF v4 CU. Returns the offset of the unit_length placeholder.
// After building all DIEs, get the bytes and patch the unit_length.
static Uptr beginCUv4(ArrayOutputStream& stream, U32 abbrevOffset, U8 addrSize = 8)
{
	Uptr lengthOffset = stream.position();
	write(stream, U32(0)); // placeholder for unit_length
	write(stream, U16(4)); // version = 4
	write(stream, abbrevOffset);
	write(stream, addrSize);
	return lengthOffset;
}

// Begins a DWARF v5 CU. Returns the offset of the unit_length placeholder.
static Uptr beginCUv5(ArrayOutputStream& stream, U32 abbrevOffset, U8 addrSize = 8)
{
	Uptr lengthOffset = stream.position();
	write(stream, U32(0)); // placeholder for unit_length
	write(stream, U16(5)); // version = 5
	write(stream, DW_UT_compile);
	write(stream, addrSize);
	write(stream, abbrevOffset);
	return lengthOffset;
}

// ---------------------------------------------------------------------------
// .debug_line builder
// ---------------------------------------------------------------------------

// Line table header constants used by all tests.
static constexpr U8 kMinInstructionLength = 1;
static constexpr U8 kMaxOpsPerInstruction = 1;
static constexpr U8 kDefaultIsStmt = 1;
static constexpr I8 kLineBase = -5;
static constexpr U8 kLineRange = 14;
static constexpr U8 kOpcodeBase = 13;
// Standard opcode lengths (opcodes 1..12)
static const U8 kStdOpcodeLengths[12] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1};
// Header content size is fixed for our tests (no directories or files).
static constexpr U32 kLineHeaderLength = 1 + 1 + 1 + 1 + 1 + 1 + 12 + 1 + 1; // = 20

// Begins a DWARF v4 line table. Returns the offset of the unit_length placeholder.
static Uptr beginLineTableV4(ArrayOutputStream& stream)
{
	Uptr lengthOffset = stream.position();
	write(stream, U32(0));            // placeholder for unit_length
	write(stream, U16(4));            // version = 4
	write(stream, kLineHeaderLength); // header_length (fixed for our tests)
	write(stream, kMinInstructionLength);
	write(stream, kMaxOpsPerInstruction);
	write(stream, kDefaultIsStmt);
	write(stream, kLineBase);
	write(stream, kLineRange);
	write(stream, kOpcodeBase);
	for(int i = 0; i < 12; ++i) { write(stream, kStdOpcodeLengths[i]); }
	write(stream, U8(0)); // empty include directory list
	write(stream, U8(0)); // empty file name list
	return lengthOffset;
}

// Emit DW_LNE_set_address extended opcode.
static void lineSetAddress(Serialization::OutputStream& stream, U64 address)
{
	write(stream, U8(0));         // extended opcode marker
	writeULEB128(stream, U64(9)); // length = 1 (extOp) + 8 (address)
	write(stream, U8(2));         // DW_LNE_set_address
	write(stream, address);
}

// Emit DW_LNE_end_sequence.
static void lineEndSequence(Serialization::OutputStream& stream)
{
	write(stream, U8(0)); // extended opcode marker
	writeULEB128(stream, U64(1));
	write(stream, U8(1)); // DW_LNE_end_sequence
}

// Emit DW_LNS_copy (opcode 1): emit row without advancing.
static void lineCopy(Serialization::OutputStream& stream) { write(stream, U8(1)); }

// Emit DW_LNS_advance_pc (opcode 2).
static void lineAdvancePC(Serialization::OutputStream& stream, U64 adv)
{
	write(stream, U8(2));
	writeULEB128(stream, adv);
}

// Emit DW_LNS_advance_line (opcode 3).
static void lineAdvanceLine(Serialization::OutputStream& stream, I64 adv)
{
	write(stream, U8(3));
	writeSLEB128(stream, adv);
}

// Emit DW_LNS_const_add_pc (opcode 8).
static void lineConstAddPC(Serialization::OutputStream& stream) { write(stream, U8(8)); }

// Compute the const_add_pc address increment.
static Uptr constAddPCIncrement()
{
	return Uptr((255 - kOpcodeBase) / kLineRange) * kMinInstructionLength;
}

// ---------------------------------------------------------------------------
// Test 1: testSimpleFunction
// 1 CU with 1 subprogram, name via .debug_str, line from .debug_line.
// ---------------------------------------------------------------------------

static void testSimpleFunction(TEST_STATE_PARAM)
{
	// .debug_str
	ArrayOutputStream strStream;
	write(strStream, U8(0)); // offset 0 = empty string
	U32 nameOffset = addDebugStr(strStream, "myFunction");

	// .debug_abbrev
	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(
		abbrevStream,
		2,
		DW_TAG_subprogram,
		false,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	// .debug_info
	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	// CU DIE (abbrev 1)
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0x1000)); // low_pc
	write(infoStream, U32(0x100));  // high_pc (offset)
	write(infoStream, U32(0));      // stmt_list
	// Subprogram DIE (abbrev 2)
	writeULEB128(infoStream, 2);
	write(infoStream, nameOffset);  // name -> .debug_str
	write(infoStream, U64(0x1000)); // low_pc
	write(infoStream, U32(0x80));   // high_pc (offset)
	// Null terminator for CU children
	write(infoStream, U8(0));

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	// .debug_line
	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0x1000);
	lineAdvanceLine(lineStream, 41); // line = 1 + 41 = 42
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x100);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, 0x1020, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, "myFunction") == 0);
		CHECK_EQ(locs[0].line, Uptr(42));
	}
}

// ---------------------------------------------------------------------------
// Test 2: testFunctionNotFound
// ---------------------------------------------------------------------------

static void testFunctionNotFound(TEST_STATE_PARAM)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	addDebugStr(strStream, "foo");

	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	buildAbbrevEntry(
		abbrevStream,
		2,
		DW_TAG_subprogram,
		false,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0x1000));
	write(infoStream, U32(0x100));
	writeULEB128(infoStream, 2);
	write(infoStream, U32(1)); // name
	write(infoStream, U64(0x1000));
	write(infoStream, U32(0x80));
	write(infoStream, U8(0));

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	std::vector<U8> line; // empty
	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	// Address 0x2000 is outside [0x1000, 0x1100)
	Uptr count = DWARF::getSourceLocations(sections, 0x2000, locs, 4);
	CHECK_EQ(count, Uptr(0));
}

// ---------------------------------------------------------------------------
// Test 3: testInlineChain
// ---------------------------------------------------------------------------

static void testInlineChain(TEST_STATE_PARAM)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	U32 outerName = addDebugStr(strStream, "outerFunc");
	U32 innerName = addDebugStr(strStream, "innerFunc");

	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(
		abbrevStream,
		2,
		DW_TAG_subprogram,
		true,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	buildAbbrevEntry(abbrevStream,
					 3,
					 DW_TAG_inlined_subroutine,
					 false,
					 {{DW_AT_name, DW_FORM_strp},
					  {DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_call_line, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0x1000));
	write(infoStream, U32(0x200));
	write(infoStream, U32(0)); // stmt_list
	// subprogram
	writeULEB128(infoStream, 2);
	write(infoStream, outerName);
	write(infoStream, U64(0x1000));
	write(infoStream, U32(0x200));
	// inlined_subroutine
	writeULEB128(infoStream, 3);
	write(infoStream, innerName);
	write(infoStream, U64(0x1050));
	write(infoStream, U32(0x40));
	write(infoStream, U32(100)); // call_line
	write(infoStream, U8(0));    // null for subprogram children
	write(infoStream, U8(0));    // null for CU children

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0x1050);
	lineAdvanceLine(lineStream, 54); // line = 55
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x200);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, 0x1060, locs, 4);
	CHECK_EQ(count, Uptr(2));
	if(count >= 2)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, "innerFunc") == 0);
		CHECK_EQ(locs[0].line, Uptr(55));
		CHECK_TRUE(locs[1].name != nullptr && strcmp(locs[1].name, "outerFunc") == 0);
		CHECK_EQ(locs[1].line, Uptr(100));
	}
}

// ---------------------------------------------------------------------------
// Test 4: testAbstractOrigin
// ---------------------------------------------------------------------------

static void testAbstractOrigin(TEST_STATE_PARAM)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	U32 callerName = addDebugStr(strStream, "caller");
	U32 inlinedName = addDebugStr(strStream, "inlinedHelper");
	U32 inlinedLinkage = addDebugStr(strStream, "_Z13inlinedHelperv");

	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(abbrevStream,
					 2,
					 DW_TAG_subprogram,
					 false,
					 {{DW_AT_name, DW_FORM_strp}, {DW_AT_linkage_name, DW_FORM_strp}});
	buildAbbrevEntry(
		abbrevStream,
		3,
		DW_TAG_subprogram,
		true,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	buildAbbrevEntry(abbrevStream,
					 4,
					 DW_TAG_inlined_subroutine,
					 false,
					 {{DW_AT_abstract_origin, DW_FORM_ref4},
					  {DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_call_line, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	// CU DIE
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0x2000));
	write(infoStream, U32(0x200));
	write(infoStream, U32(0));
	// Abstract subprogram DIE: record CU-relative offset
	Uptr abstractDIEOffset = infoStream.position();
	U32 abstractCURelOffset = U32(abstractDIEOffset - cuLenOff);
	writeULEB128(infoStream, 2);
	write(infoStream, inlinedName);
	write(infoStream, inlinedLinkage);
	// Concrete subprogram
	writeULEB128(infoStream, 3);
	write(infoStream, callerName);
	write(infoStream, U64(0x2000));
	write(infoStream, U32(0x200));
	// Inlined subroutine
	writeULEB128(infoStream, 4);
	write(infoStream, abstractCURelOffset);
	write(infoStream, U64(0x2080));
	write(infoStream, U32(0x40));
	write(infoStream, U32(77)); // call_line
	write(infoStream, U8(0));   // null for caller children
	write(infoStream, U8(0));   // null for CU children

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0x2080);
	lineAdvanceLine(lineStream, 29); // line = 30
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x200);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, 0x2090, locs, 4);
	CHECK_EQ(count, Uptr(2));
	if(count >= 2)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, "inlinedHelper") == 0);
		CHECK_TRUE(locs[0].linkageName != nullptr
				   && strcmp(locs[0].linkageName, "_Z13inlinedHelperv") == 0);
		CHECK_EQ(locs[0].line, Uptr(30));
		CHECK_TRUE(locs[1].name != nullptr && strcmp(locs[1].name, "caller") == 0);
		CHECK_EQ(locs[1].line, Uptr(77));
	}
}

// ---------------------------------------------------------------------------
// Test 5: testDWARFv4Header
// ---------------------------------------------------------------------------

// Helper: build a simple CU+subprogram+line-table and verify lookup.
static void testSimpleCU(TEST_STATE_PARAM,
						 Uptr (*beginCU)(ArrayOutputStream&, U32, U8),
						 const char* funcName,
						 U64 baseAddr,
						 Uptr expectedLine)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	U32 nameOff = addDebugStr(strStream, funcName);

	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(
		abbrevStream,
		2,
		DW_TAG_subprogram,
		false,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCU(infoStream, 0, 8);
	writeULEB128(infoStream, 1);
	write(infoStream, baseAddr);
	write(infoStream, U32(0x80));
	write(infoStream, U32(0));
	writeULEB128(infoStream, 2);
	write(infoStream, nameOff);
	write(infoStream, baseAddr);
	write(infoStream, U32(0x80));
	write(infoStream, U8(0));

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, baseAddr);
	lineAdvanceLine(lineStream, I64(expectedLine) - 1);
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x80);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, Uptr(baseAddr) + 0x10, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, funcName) == 0);
		CHECK_EQ(locs[0].line, expectedLine);
	}
}

static void testDWARFv4Header(TEST_STATE_PARAM)
{
	testSimpleCU(TEST_STATE_ARG, beginCUv4, "v4func", 0x3000, 10);
}

// ---------------------------------------------------------------------------
// Test 6: testDWARFv5Header
// ---------------------------------------------------------------------------

static void testDWARFv5Header(TEST_STATE_PARAM)
{
	testSimpleCU(TEST_STATE_ARG, beginCUv5, "v5func", 0x4000, 20);
}

// ---------------------------------------------------------------------------
// Test 7: testHighPCAsOffset
// ---------------------------------------------------------------------------

static void testHighPCAsOffset(TEST_STATE_PARAM)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	U32 nameOff = addDebugStr(strStream, "offsetFunc");

	ArrayOutputStream abbrevStream;
	// CU uses high_pc as addr (absolute)
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_addr},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	// Subprogram uses high_pc as data4 (offset)
	buildAbbrevEntry(
		abbrevStream,
		2,
		DW_TAG_subprogram,
		false,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0x5000)); // low_pc
	write(infoStream, U64(0x5200)); // high_pc (absolute addr)
	write(infoStream, U32(0));
	writeULEB128(infoStream, 2);
	write(infoStream, nameOff);
	write(infoStream, U64(0x5000));
	write(infoStream, U32(0x100)); // high_pc offset -> actual 0x5100
	write(infoStream, U8(0));

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0x5000);
	lineAdvanceLine(lineStream, 74); // line = 75
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x200);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, 0x5050, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, "offsetFunc") == 0);
		CHECK_EQ(locs[0].line, Uptr(75));
	}
}

// ---------------------------------------------------------------------------
// Test 8: testLineTableSpecialOpcodes
// ---------------------------------------------------------------------------

static void testLineTableSpecialOpcodes(TEST_STATE_PARAM)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	U32 nameOff = addDebugStr(strStream, "specialFunc");

	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(
		abbrevStream,
		2,
		DW_TAG_subprogram,
		false,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0x6000));
	write(infoStream, U32(0x400));
	write(infoStream, U32(0));
	writeULEB128(infoStream, 2);
	write(infoStream, nameOff);
	write(infoStream, U64(0x6000));
	write(infoStream, U32(0x400));
	write(infoStream, U8(0));

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	// Line table with special opcodes.
	// With lineBase=-5, lineRange=14, opcodeBase=13:
	// Special opcode: adjusted = opcode - 13
	//   addr_advance = (adjusted / 14) * 1
	//   line_advance = -5 + (adjusted % 14)
	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0x6000);
	lineAdvanceLine(lineStream, 9); // line = 10
	lineCopy(lineStream);           // row at 0x6000, line=10

	// Special opcode: advance addr by 2, line by 1. adjusted=2*14+6=34, opcode=47
	write(lineStream, U8(47)); // row at 0x6002, line=11
	// Special opcode: advance addr by 1, line by 3. adjusted=1*14+8=22, opcode=35
	write(lineStream, U8(35)); // row at 0x6003, line=14

	lineAdvancePC(lineStream, 0x400);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, 0x6000, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1) { CHECK_EQ(locs[0].line, Uptr(10)); }

	memset(locs, 0, sizeof(locs));
	count = DWARF::getSourceLocations(sections, 0x6002, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1) { CHECK_EQ(locs[0].line, Uptr(11)); }

	memset(locs, 0, sizeof(locs));
	count = DWARF::getSourceLocations(sections, 0x6003, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1) { CHECK_EQ(locs[0].line, Uptr(14)); }
}

// ---------------------------------------------------------------------------
// Test 9: testLineTableConstAddPC
// ---------------------------------------------------------------------------

static void testLineTableConstAddPC(TEST_STATE_PARAM)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	U32 nameOff = addDebugStr(strStream, "constAddPCFunc");

	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(
		abbrevStream,
		2,
		DW_TAG_subprogram,
		false,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	Uptr expectedIncrement = constAddPCIncrement();
	U32 cuSize = U32(0x100 + expectedIncrement);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0x7000));
	write(infoStream, cuSize);
	write(infoStream, U32(0));
	writeULEB128(infoStream, 2);
	write(infoStream, nameOff);
	write(infoStream, U64(0x7000));
	write(infoStream, cuSize);
	write(infoStream, U8(0));

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0x7000);
	lineAdvanceLine(lineStream, 19); // line = 20
	lineCopy(lineStream);
	lineConstAddPC(lineStream);
	lineAdvanceLine(lineStream, 5); // line = 25
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x100);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, 0x7000, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1) { CHECK_EQ(locs[0].line, Uptr(20)); }

	memset(locs, 0, sizeof(locs));
	count = DWARF::getSourceLocations(sections, 0x7000 + expectedIncrement, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1) { CHECK_EQ(locs[0].line, Uptr(25)); }
}

// ---------------------------------------------------------------------------
// Test 10: testMultipleCUs
// ---------------------------------------------------------------------------

static void testMultipleCUs(TEST_STATE_PARAM)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	U32 name1 = addDebugStr(strStream, "cu1func");
	U32 name2 = addDebugStr(strStream, "cu2func");

	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(
		abbrevStream,
		2,
		DW_TAG_subprogram,
		false,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	// Line table for CU1 at offset 0, CU2 at lineOffset2
	ArrayOutputStream lineStream;
	Uptr lineLenOff1 = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0xA000);
	lineAdvanceLine(lineStream, 9);
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x100);
	lineEndSequence(lineStream);
	Uptr line1End = lineStream.position();

	U32 lineOffset2 = U32(lineStream.position());
	Uptr lineLenOff2 = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0xB000);
	lineAdvanceLine(lineStream, 49); // line = 50
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x100);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff1, U32(line1End - lineLenOff1 - 4));
	patchU32(line, lineLenOff2, U32(line.size() - lineLenOff2 - 4));

	ArrayOutputStream infoStream;
	// CU 1
	Uptr cu1LenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0xA000));
	write(infoStream, U32(0x100));
	write(infoStream, U32(0));
	writeULEB128(infoStream, 2);
	write(infoStream, name1);
	write(infoStream, U64(0xA000));
	write(infoStream, U32(0x100));
	write(infoStream, U8(0));
	Uptr cu1End = infoStream.position();

	// CU 2
	Uptr cu2LenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0xB000));
	write(infoStream, U32(0x100));
	write(infoStream, lineOffset2);
	writeULEB128(infoStream, 2);
	write(infoStream, name2);
	write(infoStream, U64(0xB000));
	write(infoStream, U32(0x100));
	write(infoStream, U8(0));

	auto info = infoStream.getBytes();
	patchU32(info, cu1LenOff, U32(cu1End - cu1LenOff - 4));
	patchU32(info, cu2LenOff, U32(info.size() - cu2LenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, 0xB020, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, "cu2func") == 0);
		CHECK_EQ(locs[0].line, Uptr(50));
	}
}

// ---------------------------------------------------------------------------
// Test 11: testInlineStringForm
// ---------------------------------------------------------------------------

static void testInlineStringForm(TEST_STATE_PARAM)
{
	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(abbrevStream,
					 2,
					 DW_TAG_subprogram,
					 false,
					 {{DW_AT_name, DW_FORM_string},
					  {DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0xC000));
	write(infoStream, U32(0x100));
	write(infoStream, U32(0));
	writeULEB128(infoStream, 2);
	// Inline string "inlineStr"
	const char* inlineName = "inlineStr";
	for(const char* c = inlineName; *c; ++c) { write(infoStream, U8(*c)); }
	write(infoStream, U8(0));
	write(infoStream, U64(0xC000));
	write(infoStream, U32(0x100));
	write(infoStream, U8(0));

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0xC000);
	lineAdvanceLine(lineStream, 4); // line = 5
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x100);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	std::vector<U8> str; // empty
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, 0xC010, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, "inlineStr") == 0);
		CHECK_EQ(locs[0].line, Uptr(5));
	}
}

// ---------------------------------------------------------------------------
// Test 12: testEmptySections
// ---------------------------------------------------------------------------

static void testEmptySections(TEST_STATE_PARAM)
{
	{
		DWARF::Sections sections;
		DWARF::SourceLocation locs[4];
		memset(locs, 0, sizeof(locs));
		Uptr count = DWARF::getSourceLocations(sections, 0x1000, locs, 4);
		CHECK_EQ(count, Uptr(0));
	}
	{
		U8 dummy = 0;
		DWARF::Sections sections;
		sections.debugInfo = &dummy;
		sections.debugInfoSize = 0;
		sections.debugAbbrev = &dummy;
		sections.debugAbbrevSize = 0;
		DWARF::SourceLocation locs[4];
		memset(locs, 0, sizeof(locs));
		Uptr count = DWARF::getSourceLocations(sections, 0x1000, locs, 4);
		CHECK_EQ(count, Uptr(0));
	}
	{
		DWARF::Sections sections;
		Uptr count = DWARF::getSourceLocations(sections, 0x1000, nullptr, 0);
		CHECK_EQ(count, Uptr(0));
	}
}

// ---------------------------------------------------------------------------
// Test 13: testMaxLocations
// ---------------------------------------------------------------------------

static void testMaxLocations(TEST_STATE_PARAM)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	U32 name0 = addDebugStr(strStream, "level0");
	U32 name1 = addDebugStr(strStream, "level1");
	U32 name2 = addDebugStr(strStream, "level2");
	U32 name3 = addDebugStr(strStream, "level3");

	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(
		abbrevStream,
		2,
		DW_TAG_subprogram,
		true,
		{{DW_AT_name, DW_FORM_strp}, {DW_AT_low_pc, DW_FORM_addr}, {DW_AT_high_pc, DW_FORM_data4}});
	buildAbbrevEntry(abbrevStream,
					 3,
					 DW_TAG_inlined_subroutine,
					 true,
					 {{DW_AT_name, DW_FORM_strp},
					  {DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_call_line, DW_FORM_data4}});
	buildAbbrevEntry(abbrevStream,
					 4,
					 DW_TAG_inlined_subroutine,
					 false,
					 {{DW_AT_name, DW_FORM_strp},
					  {DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_call_line, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0xD000));
	write(infoStream, U32(0x200));
	write(infoStream, U32(0));
	// level0: subprogram
	writeULEB128(infoStream, 2);
	write(infoStream, name0);
	write(infoStream, U64(0xD000));
	write(infoStream, U32(0x200));
	// level1: inlined (has children)
	writeULEB128(infoStream, 3);
	write(infoStream, name1);
	write(infoStream, U64(0xD000));
	write(infoStream, U32(0x180));
	write(infoStream, U32(10));
	// level2: inlined (has children)
	writeULEB128(infoStream, 3);
	write(infoStream, name2);
	write(infoStream, U64(0xD000));
	write(infoStream, U32(0x100));
	write(infoStream, U32(20));
	// level3: inlined (leaf)
	writeULEB128(infoStream, 4);
	write(infoStream, name3);
	write(infoStream, U64(0xD000));
	write(infoStream, U32(0x80));
	write(infoStream, U32(30));
	write(infoStream, U8(0)); // null for level2
	write(infoStream, U8(0)); // null for level1
	write(infoStream, U8(0)); // null for level0
	write(infoStream, U8(0)); // null for CU

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0xD000);
	lineAdvanceLine(lineStream, 41); // line = 42
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x200);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	// Full chain would be 4 locations. Request only 2.
	Uptr count = DWARF::getSourceLocations(sections, 0xD010, locs, 2);
	CHECK_EQ(count, Uptr(2));
	if(count >= 2)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, "level3") == 0);
		CHECK_EQ(locs[0].line, Uptr(42));
		CHECK_TRUE(locs[1].name != nullptr && strcmp(locs[1].name, "level2") == 0);
		CHECK_EQ(locs[1].line, Uptr(30));
	}
}

// ---------------------------------------------------------------------------
// Test 14: testDeclLineForSubprogram
// ---------------------------------------------------------------------------

static void testDeclLineForSubprogram(TEST_STATE_PARAM)
{
	ArrayOutputStream strStream;
	write(strStream, U8(0));
	U32 outerName = addDebugStr(strStream, "outerWithDeclLine");
	U32 innerName = addDebugStr(strStream, "innerInlined");

	ArrayOutputStream abbrevStream;
	buildAbbrevEntry(abbrevStream,
					 1,
					 DW_TAG_compile_unit,
					 true,
					 {{DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_stmt_list, DW_FORM_sec_offset}});
	buildAbbrevEntry(abbrevStream,
					 2,
					 DW_TAG_subprogram,
					 true,
					 {{DW_AT_name, DW_FORM_strp},
					  {DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_decl_line, DW_FORM_data4}});
	buildAbbrevEntry(abbrevStream,
					 3,
					 DW_TAG_inlined_subroutine,
					 false,
					 {{DW_AT_name, DW_FORM_strp},
					  {DW_AT_low_pc, DW_FORM_addr},
					  {DW_AT_high_pc, DW_FORM_data4},
					  {DW_AT_call_line, DW_FORM_data4}});
	terminateAbbrevTable(abbrevStream);

	ArrayOutputStream infoStream;
	Uptr cuLenOff = beginCUv4(infoStream, 0);
	writeULEB128(infoStream, 1);
	write(infoStream, U64(0xE000));
	write(infoStream, U32(0x200));
	write(infoStream, U32(0));
	// Subprogram with decl_line = 200
	writeULEB128(infoStream, 2);
	write(infoStream, outerName);
	write(infoStream, U64(0xE000));
	write(infoStream, U32(0x200));
	write(infoStream, U32(200));
	// Inlined subroutine with call_line = 210
	writeULEB128(infoStream, 3);
	write(infoStream, innerName);
	write(infoStream, U64(0xE040));
	write(infoStream, U32(0x40));
	write(infoStream, U32(210));
	write(infoStream, U8(0)); // null for subprogram children
	write(infoStream, U8(0)); // null for CU children

	auto info = infoStream.getBytes();
	patchU32(info, cuLenOff, U32(info.size() - cuLenOff - 4));

	ArrayOutputStream lineStream;
	Uptr lineLenOff = beginLineTableV4(lineStream);
	lineSetAddress(lineStream, 0xE040);
	lineAdvanceLine(lineStream, 14); // line = 15
	lineCopy(lineStream);
	lineAdvancePC(lineStream, 0x200);
	lineEndSequence(lineStream);

	auto line = lineStream.getBytes();
	patchU32(line, lineLenOff, U32(line.size() - lineLenOff - 4));

	auto str = strStream.getBytes();
	auto abbrev = abbrevStream.getBytes();

	DWARF::Sections sections = makeSections(info, abbrev, str, line);
	DWARF::SourceLocation locs[4];
	memset(locs, 0, sizeof(locs));

	Uptr count = DWARF::getSourceLocations(sections, 0xE050, locs, 4);
	CHECK_EQ(count, Uptr(2));
	if(count >= 2)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, "innerInlined") == 0);
		CHECK_EQ(locs[0].line, Uptr(15));
		CHECK_TRUE(locs[1].name != nullptr && strcmp(locs[1].name, "outerWithDeclLine") == 0);
		CHECK_EQ(locs[1].line, Uptr(210));
	}

	// Address outside inlined but inside subprogram: only subprogram returned.
	memset(locs, 0, sizeof(locs));
	count = DWARF::getSourceLocations(sections, 0xE000, locs, 4);
	CHECK_EQ(count, Uptr(1));
	if(count >= 1)
	{
		CHECK_TRUE(locs[0].name != nullptr && strcmp(locs[0].name, "outerWithDeclLine") == 0);
		CHECK_EQ(locs[0].line, Uptr(0)); // no line table entry covers 0xE000
	}
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

I32 execDWARFTest(int argc, char** argv)
{
	WAVM_SUPPRESS_UNUSED(argc);
	WAVM_SUPPRESS_UNUSED(argv);

	TEST_STATE_LOCAL;
	Timing::Timer timer;

	testSimpleFunction(TEST_STATE_ARG);
	testFunctionNotFound(TEST_STATE_ARG);
	testInlineChain(TEST_STATE_ARG);
	testAbstractOrigin(TEST_STATE_ARG);
	testDWARFv4Header(TEST_STATE_ARG);
	testDWARFv5Header(TEST_STATE_ARG);
	testHighPCAsOffset(TEST_STATE_ARG);
	testLineTableSpecialOpcodes(TEST_STATE_ARG);
	testLineTableConstAddPC(TEST_STATE_ARG);
	testMultipleCUs(TEST_STATE_ARG);
	testInlineStringForm(TEST_STATE_ARG);
	testEmptySections(TEST_STATE_ARG);
	testMaxLocations(TEST_STATE_ARG);
	testDeclLineForSubprogram(TEST_STATE_ARG);

	Timing::logTimer("Ran DWARF tests", timer);

	return testState.exitCode();
}
