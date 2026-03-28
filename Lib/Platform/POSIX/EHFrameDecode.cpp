#if WAVM_PLATFORM_POSIX

#include <cinttypes>
#include <cstring>
#include "WAVM/DWARF/Constants.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/LEB128.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/StringBuilder.h"
#include "WAVM/Platform/Unwind.h"

using namespace WAVM;
using namespace WAVM::DWARF;
using namespace WAVM::Platform;
using namespace WAVM::Serialization;

// Parse FDE and its referenced CIE to extract alignment factors and CFI instructions
static bool parseFDE(const U8* fde,
					 Uptr fdeSize,
					 Uptr& outCodeAlignFactor,
					 I64& outDataAlignFactor,
					 const U8*& outInstructions,
					 Uptr& outInstructionsSize)
{
	if(fdeSize < 8) { return false; }

	// FDE header: length (4 bytes), CIE pointer (4 bytes)
	U32 fdeLength;
	memcpy(&fdeLength, fde, 4);
	if(fdeLength == 0) { return false; }

	U32 cieOffset;
	memcpy(&cieOffset, fde + 4, 4);
	if(cieOffset == 0) { return false; } // This would be a CIE, not an FDE

	// CIE pointer is relative to its own position (fde + 4)
	const U8* cie = (fde + 4) - cieOffset;

	// Sanity check: cie should be before fde
	if(cie >= fde) { return false; }

	// Parse CIE
	U32 cieLength;
	memcpy(&cieLength, cie, 4);
	if(cieLength == 0) { return false; }

	MemoryInputStream cieStream(cie + 4, cieLength);

	// CIE ID (should be 0 for .eh_frame format)
	U32 cieId;
	if(!tryRead(cieStream, cieId)) { return false; }
	if(cieId != 0) { return false; }

	// Version
	if(!cieStream.tryAdvance(1)) { return false; } // Skip version byte

	// Augmentation string (null-terminated). Get pointer into underlying buffer.
	const U8* augString = cieStream.tryAdvance(0);
	Uptr augStringLen = 0;
	{
		const U8* bytePtr;
		do
		{
			bytePtr = cieStream.tryAdvance(1);
			if(!bytePtr) { break; }
			if(*bytePtr != 0) { ++augStringLen; }
		} while(*bytePtr != 0);
	}

	// Check if augmentation starts with 'z' (indicates FDE has augmentation data length)
	bool hasZAugmentation = (augStringLen > 0 && augString && augString[0] == 'z');

	// Code alignment factor (ULEB128)
	U64 codeAlignFactor;
	if(!trySerializeVarUInt64(cieStream, codeAlignFactor)) { return false; }
	outCodeAlignFactor = Uptr(codeAlignFactor);

	// Data alignment factor (SLEB128)
	if(!trySerializeVarSInt64(cieStream, outDataAlignFactor)) { return false; }

	// Return address register (ULEB128) - skip it
	U64 tmp;
	trySerializeVarUInt64(cieStream, tmp);

	// If 'z' augmentation, parse augmentation data to find pointer encoding
	Uptr fdePointerSize = sizeof(void*); // Default to pointer size
	if(hasZAugmentation && cieStream.capacity())
	{
		U64 augDataLen;
		if(!trySerializeVarUInt64(cieStream, augDataLen)) { return false; }
		Uptr augDataEndPos = cieStream.position() + Uptr(augDataLen);

		// Parse augmentation string characters (skip 'z')
		for(Uptr i = 1; i < augStringLen && cieStream.position() < augDataEndPos; ++i)
		{
			char c = static_cast<char>(augString[i]);
			if(c == 'R')
			{
				// FDE pointer encoding
				U8 encoding;
				if(!tryRead(cieStream, encoding)) { break; }
				Uptr ptrSize = getEncodedPointerSize(encoding);
				if(ptrSize > 0) { fdePointerSize = ptrSize; }
			}
			else if(c == 'L')
			{
				// LSDA encoding - skip 1 byte
				cieStream.tryAdvance(1);
			}
			else if(c == 'P')
			{
				// Personality encoding + pointer - skip encoding byte + pointer
				U8 encoding;
				if(!tryRead(cieStream, encoding)) { break; }
				Uptr ptrSize = getEncodedPointerSize(encoding);
				if(ptrSize > 0) { cieStream.tryAdvance(ptrSize); }
			}
			else if(c == 'S')
			{
				// Signal frame - no data
			}
		}
	}

	// Now parse the FDE body to get CFI instructions.
	// FDE body starts after the 4-byte CIE pointer field.
	Uptr fdeBodySize = fdeLength >= 4 ? fdeLength - 4 : 0;
	MemoryInputStream fdeStream(fde + 8, fdeBodySize);

	// Skip PC begin (size depends on CIE's FDE pointer encoding)
	fdeStream.tryAdvance(fdePointerSize);

	// Skip PC range (size depends on CIE's FDE pointer encoding)
	fdeStream.tryAdvance(fdePointerSize);

	// Skip augmentation data length and data (ULEB128 + data) only if CIE has 'z' augmentation
	if(hasZAugmentation && fdeStream.capacity())
	{
		U64 augLen;
		if(!trySerializeVarUInt64(fdeStream, augLen)) { return false; }
		fdeStream.tryAdvance(Uptr(augLen));
	}

	// The remaining bytes in fdeStream are the CFI instructions.
	// Get a pointer into the underlying buffer for the caller.
	outInstructionsSize = fdeStream.capacity();
	outInstructions = fdeStream.tryAdvance(outInstructionsSize);
	if(!outInstructions) { outInstructionsSize = 0; }

	return true;
}

// DWARF CFI opcode constants
enum
{
	DW_CFA_nop = 0x00,
	DW_CFA_advance_loc1 = 0x02,
	DW_CFA_advance_loc2 = 0x03,
	DW_CFA_advance_loc4 = 0x04,
	DW_CFA_offset_extended = 0x05,
	DW_CFA_restore_extended = 0x06,
	DW_CFA_undefined = 0x07,
	DW_CFA_same_value = 0x08,
	DW_CFA_register = 0x09,
	DW_CFA_remember_state = 0x0a,
	DW_CFA_restore_state = 0x0b,
	DW_CFA_def_cfa = 0x0c,
	DW_CFA_def_cfa_register = 0x0d,
	DW_CFA_def_cfa_offset = 0x0e,
	DW_CFA_def_cfa_expression = 0x0f,
	DW_CFA_expression = 0x10,
	DW_CFA_offset_extended_sf = 0x11,
	DW_CFA_def_cfa_sf = 0x12,
	DW_CFA_def_cfa_offset_sf = 0x13,
	DW_CFA_val_offset = 0x14,
	DW_CFA_val_offset_sf = 0x15,
	DW_CFA_val_expression = 0x16,
	// High 2 bits encoding:
	// 0x40-0x7f: DW_CFA_advance_loc (delta in low 6 bits)
	// 0x80-0xbf: DW_CFA_offset (reg in low 6 bits)
	// 0xc0-0xff: DW_CFA_restore (reg in low 6 bits)
};

// Check if an opcode is an advance_loc variant
static bool isAdvanceLocOpcode(U8 opcode)
{
	return (opcode >> 6) == 1 || opcode == DW_CFA_advance_loc1 || opcode == DW_CFA_advance_loc2
		   || opcode == DW_CFA_advance_loc4;
}

struct DecodedCFIOp
{
	U8 opcode;
	Uptr codeOffset;
	U32 reg;
	I64 operand;
};

// Parse a single CFI instruction from the stream, returning the number of bytes consumed.
static Uptr parseCFIInstruction(MemoryInputStream& stream,
								Uptr codeOffset,
								Uptr codeAlignFactor,
								I64 dataAlignFactor,
								DecodedCFIOp& outOp)
{
	outOp.opcode = 0;
	outOp.codeOffset = codeOffset;
	outOp.reg = 0;
	outOp.operand = 0;

	Uptr startPos = stream.position();

	U8 op;
	if(!tryRead(stream, op)) { return 0; }
	outOp.opcode = op;
	U8 high2 = op >> 6;
	U8 low6 = op & 0x3f;

	// Helper lambdas to read LEB128 values from the stream.
	auto readULEB = [&stream](U64& v) { return trySerializeVarUInt64(stream, v); };
	auto readSLEB = [&stream](I64& v) { return trySerializeVarSInt64(stream, v); };

	if(high2 == 1)
	{ // DW_CFA_advance_loc
		outOp.operand = low6 * codeAlignFactor;
	}
	else if(high2 == 2)
	{ // DW_CFA_offset
		outOp.reg = low6;
		U64 v;
		if(readULEB(v)) { outOp.operand = I64(v) * dataAlignFactor; }
	}
	else if(high2 == 3)
	{ // DW_CFA_restore
		outOp.reg = low6;
	}
	else
	{
		switch(op)
		{
		case DW_CFA_nop: break;
		case DW_CFA_advance_loc1: {
			U8 delta;
			if(tryRead(stream, delta)) { outOp.operand = delta * codeAlignFactor; }
			break;
		}
		case DW_CFA_advance_loc2: {
			U16 delta;
			if(tryRead(stream, delta)) { outOp.operand = delta * codeAlignFactor; }
			break;
		}
		case DW_CFA_advance_loc4: {
			U32 delta;
			if(tryRead(stream, delta)) { outOp.operand = delta * codeAlignFactor; }
			break;
		}
		case DW_CFA_undefined:
		case DW_CFA_same_value: {
			U64 v;
			if(readULEB(v)) { outOp.reg = U32(v); }
			break;
		}
		case DW_CFA_register: {
			U64 r, v;
			if(readULEB(r)) { outOp.reg = U32(r); }
			if(readULEB(v)) { outOp.operand = I64(v); }
			break;
		}
		case DW_CFA_def_cfa: {
			U64 r, v;
			if(readULEB(r)) { outOp.reg = U32(r); }
			if(readULEB(v)) { outOp.operand = I64(v); }
			break;
		}
		case DW_CFA_def_cfa_register: {
			U64 v;
			if(readULEB(v)) { outOp.reg = U32(v); }
			break;
		}
		case DW_CFA_def_cfa_offset: {
			U64 v;
			if(readULEB(v)) { outOp.operand = I64(v); }
			break;
		}
		case DW_CFA_def_cfa_expression:
		case DW_CFA_expression:
		case DW_CFA_val_expression: {
			if(op == DW_CFA_expression || op == DW_CFA_val_expression)
			{
				U64 v;
				if(readULEB(v)) { outOp.reg = U32(v); }
			}
			// Skip the BLOCK (length-prefixed byte sequence)
			U64 blockLen;
			if(readULEB(blockLen)) { stream.tryAdvance(Uptr(blockLen)); }
			break;
		}
		case DW_CFA_offset_extended: {
			U64 r, v;
			if(readULEB(r)) { outOp.reg = U32(r); }
			if(readULEB(v)) { outOp.operand = I64(v) * dataAlignFactor; }
			break;
		}
		case DW_CFA_offset_extended_sf: {
			U64 r;
			I64 v;
			if(readULEB(r)) { outOp.reg = U32(r); }
			if(readSLEB(v)) { outOp.operand = v * dataAlignFactor; }
			break;
		}
		case DW_CFA_restore_extended: {
			U64 v;
			if(readULEB(v)) { outOp.reg = U32(v); }
			break;
		}
		case DW_CFA_remember_state:
		case DW_CFA_restore_state: break;
		default:
			// For unknown opcodes, we can't know how many bytes to skip,
			// so we just consume the opcode byte and hope for the best
			break;
		}
	}

	return stream.position() - startPos;
}

Uptr UnwindProcInfo::decodeUnwindOps(UnwindOp* outOps, Uptr maxOps) const
{
#ifdef __APPLE__
	// Handle compact unwind encoding on macOS
	if(compactEncoding != 0 && (!unwindData || unwindDataSize == 0))
	{
		if(!outOps) { return 1; }
		if(maxOps > 0)
		{
			outOps[0].codeOffset = 0;
			outOps[0].opcode = 0;
			outOps[0].reg = 0;
			outOps[0].operand = 0;
			outOps[0].compactEncoding = compactEncoding;
		}
		return 1;
	}
#endif

	// Parse FDE to get CFI instructions
	if(!unwindData || unwindDataSize == 0) { return 0; }

	Uptr codeAlignFactor;
	I64 dataAlignFactor;
	const U8* cfiInstructions;
	Uptr cfiSize;
	if(!parseFDE(
		   unwindData, unwindDataSize, codeAlignFactor, dataAlignFactor, cfiInstructions, cfiSize))
	{
		return 0;
	}

	// Parse CFI instructions.
	Uptr numOps = 0;
	Uptr codeOffset = 0;
	MemoryInputStream cfiStream(cfiInstructions, cfiSize);

	while(cfiStream.capacity())
	{
		DecodedCFIOp decoded;
		Uptr bytesConsumed
			= parseCFIInstruction(cfiStream, codeOffset, codeAlignFactor, dataAlignFactor, decoded);
		if(bytesConsumed == 0) { break; }

		if(outOps && numOps < maxOps)
		{
			outOps[numOps].codeOffset = decoded.codeOffset;
			outOps[numOps].opcode = decoded.opcode;
			outOps[numOps].reg = decoded.reg;
			outOps[numOps].operand = decoded.operand;
			outOps[numOps].compactEncoding = 0;
		}
		++numOps;

		if(isAdvanceLocOpcode(decoded.opcode)) { codeOffset += decoded.operand; }
	}

	return numOps;
}

#ifdef __APPLE__
static void formatCompactUnwind(U32 encoding, StringBuilderBase& sb)
{
	sb.appendf("compact_unwind 0x%08x", encoding);

#if defined(__aarch64__)
	U32 mode = encoding & 0x0F000000;
	switch(mode)
	{
	case 0x02000000: {
		U32 stackSize = (encoding >> 12) & 0xFFF;
		if(stackSize > 0) { sb.appendf(" (frameless, stack=%u)", stackSize * 16); }
		else
		{
			sb.append(" (frameless)");
		}
		break;
	}
	case 0x03000000: sb.append(" (dwarf)"); break;
	case 0x04000000: {
		sb.append(" (frame-based");
		U32 regs = encoding & 0x1FFF;
		struct
		{
			U32 mask;
			const char* name;
		} regPairs[] = {{0x001, ", x19-x20"},
						{0x002, ", x21-x22"},
						{0x004, ", x23-x24"},
						{0x008, ", x25-x26"},
						{0x010, ", x27-x28"},
						{0x100, ", d8-d9"},
						{0x200, ", d10-d11"},
						{0x400, ", d12-d13"},
						{0x800, ", d14-d15"}};
		for(const auto& rp : regPairs)
		{
			if(regs & rp.mask) { sb.append(rp.name); }
		}
		sb.append(")", 1);
		break;
	}
	default: sb.append(" (unknown mode)"); break;
	}
#elif defined(__x86_64__)
	U32 mode = encoding & 0x0F000000;
	const char* modeStr = nullptr;
	switch(mode)
	{
	case 0x01000000: modeStr = " (frameless, imm stack)"; break;
	case 0x02000000: modeStr = " (frameless, indirect)"; break;
	case 0x03000000: modeStr = " (dwarf)"; break;
	case 0x04000000: modeStr = " (frame-based)"; break;
	default: modeStr = " (unknown mode)"; break;
	}
	sb.append(modeStr);
#endif
}
#endif

void UnwindOp::format(StringBuilderBase& sb) const
{
	U8 high2 = opcode >> 6;

#ifdef __APPLE__
	if(compactEncoding != 0)
	{
		formatCompactUnwind(compactEncoding, sb);
		return;
	}
#endif

	// Handle high-2-bit encoded opcodes
	if(high2 == 1 || opcode == DW_CFA_advance_loc1 || opcode == DW_CFA_advance_loc2
	   || opcode == DW_CFA_advance_loc4)
	{
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_advance_loc %" PRId64, codeOffset, operand);
		return;
	}
	if(high2 == 2)
	{
		sb.appendf(
			"@%" WAVM_PRIuPTR ": DW_CFA_offset r%u at CFA%+" PRId64, codeOffset, reg, operand);
		return;
	}
	if(high2 == 3)
	{
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_restore r%u", codeOffset, reg);
		return;
	}

	switch(opcode)
	{
	case DW_CFA_nop: sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_nop", codeOffset); break;
	case DW_CFA_undefined:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_undefined r%u", codeOffset, reg);
		break;
	case DW_CFA_same_value:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_same_value r%u", codeOffset, reg);
		break;
	case DW_CFA_register:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_register r%u=r%" PRId64, codeOffset, reg, operand);
		break;
	case DW_CFA_def_cfa:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_def_cfa r%u%+" PRId64, codeOffset, reg, operand);
		break;
	case DW_CFA_def_cfa_register:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_def_cfa_register r%u", codeOffset, reg);
		break;
	case DW_CFA_def_cfa_offset:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_def_cfa_offset %" PRId64, codeOffset, operand);
		break;
	case DW_CFA_def_cfa_expression:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_def_cfa_expression", codeOffset);
		break;
	case DW_CFA_expression:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_expression r%u", codeOffset, reg);
		break;
	case DW_CFA_val_expression:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_val_expression r%u", codeOffset, reg);
		break;
	case DW_CFA_offset_extended:
	case DW_CFA_offset_extended_sf:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_offset_extended r%u at CFA%+" PRId64,
				   codeOffset,
				   reg,
				   operand);
		break;
	case DW_CFA_restore_extended:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_restore_extended r%u", codeOffset, reg);
		break;
	case DW_CFA_remember_state:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_remember_state", codeOffset);
		break;
	case DW_CFA_restore_state:
		sb.appendf("@%" WAVM_PRIuPTR ": DW_CFA_restore_state", codeOffset);
		break;
	default: sb.appendf("@%" WAVM_PRIuPTR ": unknown (opcode 0x%02x)", codeOffset, opcode); break;
	}
}

#endif // WAVM_PLATFORM_POSIX
