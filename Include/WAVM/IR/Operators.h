#pragma once

#include <string.h>
#include <vector>

#include "WAVM/IR/IR.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Platform/Defines.h"

#include "OperatorTable.h"

namespace WAVM { namespace IR {
	// Structures for operator immediates

	struct NoImm
	{
	};
	struct MemoryImm
	{
		Uptr memoryIndex;
	};
	struct MemoryCopyImm
	{
		Uptr sourceMemoryIndex;
		Uptr destMemoryIndex;
	};
	struct TableImm
	{
		Uptr tableIndex;
	};
	struct TableCopyImm
	{
		Uptr sourceTableIndex;
		Uptr destTableIndex;
	};

	struct ControlStructureImm
	{
		IndexedBlockType type;
	};

	struct BranchImm
	{
		Uptr targetDepth;
	};

	struct BranchTableImm
	{
		Uptr defaultTargetDepth;

		// An index into the FunctionDef's branchTables array.
		Uptr branchTableIndex;
	};

	template<typename Value> struct LiteralImm
	{
		Value value;
	};

	template<bool isGlobal> struct GetOrSetVariableImm
	{
		Uptr variableIndex;
	};

	struct FunctionImm
	{
		Uptr functionIndex;
	};

	struct CallIndirectImm
	{
		IndexedFunctionType type;
		Uptr tableIndex;
	};

	template<Uptr naturalAlignmentLog2> struct LoadOrStoreImm
	{
		U8 alignmentLog2;
		U32 offset;
	};

	template<Uptr numLanes> struct LaneIndexImm
	{
		U8 laneIndex;
	};

	template<Uptr numLanes> struct ShuffleImm
	{
		U8 laneIndices[numLanes];
	};

	template<Uptr naturalAlignmentLog2> struct AtomicLoadOrStoreImm
	{
		U8 alignmentLog2;
		U32 offset;
	};

	struct ExceptionTypeImm
	{
		Uptr exceptionTypeIndex;
	};
	struct RethrowImm
	{
		Uptr catchDepth;
	};

	struct DataSegmentAndMemImm
	{
		Uptr dataSegmentIndex;
		Uptr memoryIndex;
	};

	struct DataSegmentImm
	{
		Uptr dataSegmentIndex;
	};

	struct ElemSegmentAndTableImm
	{
		Uptr elemSegmentIndex;
		Uptr tableIndex;
	};

	struct ElemSegmentImm
	{
		Uptr elemSegmentIndex;
	};

	enum class Opcode : U16
	{
#define VISIT_OPCODE(opcode, name, ...) name = opcode,
		ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE

			maxSingleByteOpcode
		= 0xdf,
	};

	PACKED_STRUCT(template<typename Imm> struct OpcodeAndImm {
		Opcode opcode;
		Imm imm;
	});

	// Specialize for the empty immediate struct so they don't take an extra byte of space.
	template<> struct OpcodeAndImm<NoImm>
	{
		union
		{
			Opcode opcode;
			NoImm imm;
		};
	};

	// Decodes an operator from an input stream and dispatches by opcode.
	struct OperatorDecoderStream
	{
		OperatorDecoderStream(const std::vector<U8>& codeBytes)
		: nextByte(codeBytes.data()), end(codeBytes.data() + codeBytes.size())
		{
		}

		operator bool() const { return nextByte < end; }

		template<typename Visitor> typename Visitor::Result decodeOp(Visitor& visitor)
		{
			wavmAssert(nextByte + sizeof(Opcode) <= end);
			Opcode opcode;
			memcpy(&opcode, nextByte, sizeof(Opcode));
			switch(opcode)
			{
#define VISIT_OPCODE(opcode, name, nameString, Imm, ...)                                           \
	case Opcode::name:                                                                             \
	{                                                                                              \
		wavmAssert(nextByte + sizeof(OpcodeAndImm<Imm>) <= end);                                   \
		OpcodeAndImm<Imm> encodedOperator;                                                         \
		memcpy(&encodedOperator, nextByte, sizeof(OpcodeAndImm<Imm>));                             \
		nextByte += sizeof(OpcodeAndImm<Imm>);                                                     \
		return visitor.name(encodedOperator.imm);                                                  \
	}
				ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
			default: nextByte += sizeof(Opcode); return visitor.unknown(opcode);
			}
		}

		template<typename Visitor> typename Visitor::Result decodeOpWithoutConsume(Visitor& visitor)
		{
			const U8* savedNextByte = nextByte;
			typename Visitor::Result result = decodeOp(visitor);
			nextByte = savedNextByte;
			return result;
		}

	private:
		const U8* nextByte;
		const U8* end;
	};

	// Encodes an operator to an output stream.
	struct OperatorEncoderStream
	{
		OperatorEncoderStream(Serialization::OutputStream& inByteStream) : byteStream(inByteStream)
		{
		}

#define VISIT_OPCODE(_, name, nameString, Imm, ...)                                                \
	void name(Imm imm = {})                                                                        \
	{                                                                                              \
		OpcodeAndImm<Imm> encodedOperator;                                                         \
		encodedOperator.opcode = Opcode::name;                                                     \
		encodedOperator.imm = imm;                                                                 \
		memcpy((OpcodeAndImm<Imm>*)byteStream.advance(sizeof(OpcodeAndImm<Imm>)),                  \
			   &encodedOperator,                                                                   \
			   sizeof(OpcodeAndImm<Imm>));                                                         \
	}
		ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE

	private:
		Serialization::OutputStream& byteStream;
	};

	IR_API const char* getOpcodeName(Opcode opcode);

	struct NonParametricOpSignatures
	{
#define VISIT_OP(_1, name, ...) FunctionType name;
		ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(VISIT_OP)
#undef VISIT_OP
	};
	IR_API const NonParametricOpSignatures& getNonParametricOpSigs();
}}
