#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Inline/Unicode.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/WASM/WASM.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Serialization;

static void throwIfNotValidUTF8(const std::string& string)
{
	const U8* endChar = (const U8*)string.data() + string.size();
	if(Unicode::validateUTF8String((const U8*)string.data(), endChar) != endChar)
	{ throw FatalSerializationException("invalid UTF-8 encoding"); }
}

FORCEINLINE void serializeOpcode(InputStream& stream, Opcode& opcode)
{
	opcode = (Opcode)0;
	serializeNativeValue(stream, *(U8*)&opcode);
	if(opcode > Opcode::maxSingleByteOpcode)
	{
		opcode = (Opcode)(U16(opcode) << 8);
		serializeVarUInt8(stream, *(U8*)&opcode);
	}
}
FORCEINLINE void serializeOpcode(OutputStream& stream, Opcode opcode)
{
	if(opcode <= Opcode::maxSingleByteOpcode)
	{ Serialization::serializeNativeValue(stream, *(U8*)&opcode); }
	else
	{
		U8 opcodePrefix = U8(U16(opcode) >> 8);
		U8 opcodeVarUInt = U8(opcode);
		serializeNativeValue(stream, opcodePrefix);
		serializeVarUInt8(stream, opcodeVarUInt);
	}
}

// These serialization functions need to be declared in the IR namespace for the array serializer in
// the Serialization namespace to find them.
namespace WAVM { namespace IR {
	static ValueType decodeValueType(Iptr encodedValueType)
	{
		switch(encodedValueType)
		{
		case -1: return ValueType::i32;
		case -2: return ValueType::i64;
		case -3: return ValueType::f32;
		case -4: return ValueType::f64;
		case -5: return ValueType::v128;
		case -16: return ValueType::funcref;
		case -17: return ValueType::anyref;
		default: throw FatalSerializationException("invalid value type encoding");
		};
	}
	static I8 encodeValueType(ValueType valueType)
	{
		switch(valueType)
		{
		case ValueType::i32: return -1;
		case ValueType::i64: return -2;
		case ValueType::f32: return -3;
		case ValueType::f64: return -4;
		case ValueType::v128: return -5;
		case ValueType::funcref: return -16;
		case ValueType::anyref: return -17;
		default: throw FatalSerializationException("invalid value type");
		};
	}
	static void serialize(InputStream& stream, ValueType& type)
	{
		I8 encodedValueType = 0;
		serializeVarInt7(stream, encodedValueType);
		type = decodeValueType(encodedValueType);
	}
	static void serialize(OutputStream& stream, ValueType type)
	{
		I8 encodedValueType = encodeValueType(type);
		serializeVarInt7(stream, encodedValueType);
	}

	FORCEINLINE static void serialize(InputStream& stream, TypeTuple& typeTuple)
	{
		Uptr numElems;
		serializeVarUInt32(stream, numElems);

		std::vector<ValueType> elems;
		for(Uptr elemIndex = 0; elemIndex < numElems; ++elemIndex)
		{
			ValueType elem;
			serialize(stream, elem);
			elems.push_back(elem);
		}

		typeTuple = TypeTuple(elems);
	}
	static void serialize(OutputStream& stream, TypeTuple& typeTuple)
	{
		Uptr numElems = typeTuple.size();
		serializeVarUInt32(stream, numElems);

		for(ValueType elem : typeTuple) { serialize(stream, elem); }
	}

	template<typename Stream>
	void serialize(Stream& stream, SizeConstraints& sizeConstraints, bool hasMax)
	{
		serializeVarUInt32(stream, sizeConstraints.min);
		if(hasMax) { serializeVarUInt64(stream, sizeConstraints.max); }
		else if(Stream::isInput)
		{
			sizeConstraints.max = UINT64_MAX;
		}
	}

	template<typename Stream> void serialize(Stream& stream, ReferenceType& referenceType)
	{
		if(Stream::isInput)
		{
			U8 encodedReferenceType = 0;
			serializeNativeValue(stream, encodedReferenceType);
			switch(encodedReferenceType)
			{
			case 0x70: referenceType = ReferenceType::funcref; break;
			case 0x6F: referenceType = ReferenceType::anyref; break;
			default: throw FatalSerializationException("invalid reference type encoding");
			}
		}
		else
		{
			U8 encodedReferenceType;
			switch(referenceType)
			{
			case ReferenceType::funcref: encodedReferenceType = 0x70; break;
			case ReferenceType::anyref: encodedReferenceType = 0x6F; break;
			default: Errors::unreachable();
			}
			serializeNativeValue(stream, encodedReferenceType);
		}
	}

	template<typename Stream> void serialize(Stream& stream, TableType& tableType)
	{
		serialize(stream, tableType.elementType);

		Uptr flags = 0;
		if(!Stream::isInput && tableType.size.max != UINT64_MAX) { flags |= 0x01; }
		if(!Stream::isInput && tableType.isShared) { flags |= 0x02; }
		serializeVarUInt32(stream, flags);
		if(Stream::isInput) { tableType.isShared = (flags & 0x02) != 0; }
		serialize(stream, tableType.size, flags & 0x01);
	}

	template<typename Stream> void serialize(Stream& stream, MemoryType& memoryType)
	{
		Uptr flags = 0;
		if(!Stream::isInput && memoryType.size.max != UINT64_MAX) { flags |= 0x01; }
		if(!Stream::isInput && memoryType.isShared) { flags |= 0x02; }
		serializeVarUInt32(stream, flags);
		if(Stream::isInput) { memoryType.isShared = (flags & 0x02) != 0; }
		serialize(stream, memoryType.size, flags & 0x01);
	}

	template<typename Stream> void serialize(Stream& stream, GlobalType& globalType)
	{
		serialize(stream, globalType.valueType);
		U8 isMutable = globalType.isMutable ? 1 : 0;
		serializeVarUInt1(stream, isMutable);
		if(Stream::isInput) { globalType.isMutable = isMutable != 0; }
	}

	template<typename Stream> void serialize(Stream& stream, ExceptionType& exceptionType)
	{
		serialize(stream, exceptionType.params);
	}

	template<typename Stream> void serialize(Stream& stream, ExternKind& kind)
	{
		serializeNativeValue(stream, *(U8*)&kind);
	}

	template<typename Stream> void serialize(Stream& stream, Export& e)
	{
		serialize(stream, e.name);
		throwIfNotValidUTF8(e.name);
		serialize(stream, e.kind);
		serializeVarUInt32(stream, e.index);
	}

	template<typename Stream> void serialize(Stream& stream, InitializerExpression& initializer)
	{
		serializeOpcode(stream, initializer.typeOpcode);
		switch(initializer.type)
		{
		case InitializerExpression::Type::i32_const:
			serializeVarInt32(stream, initializer.i32);
			break;
		case InitializerExpression::Type::i64_const:
			serializeVarInt64(stream, initializer.i64);
			break;
		case InitializerExpression::Type::f32_const: serialize(stream, initializer.f32); break;
		case InitializerExpression::Type::f64_const: serialize(stream, initializer.f64); break;
		case InitializerExpression::Type::v128_const: serialize(stream, initializer.v128); break;
		case InitializerExpression::Type::global_get:
			serializeVarUInt32(stream, initializer.ref);
			break;
		case InitializerExpression::Type::ref_null: break;
		case InitializerExpression::Type::ref_func:
			serializeVarUInt32(stream, initializer.ref);
			break;
		default: throw FatalSerializationException("invalid initializer expression opcode");
		}
		serializeConstant(stream, "expected end opcode", (U8)Opcode::end);
	}

	template<typename Stream> void serialize(Stream& stream, TableDef& tableDef)
	{
		serialize(stream, tableDef.type);
	}

	template<typename Stream> void serialize(Stream& stream, MemoryDef& memoryDef)
	{
		serialize(stream, memoryDef.type);
	}

	template<typename Stream> void serialize(Stream& stream, GlobalDef& globalDef)
	{
		serialize(stream, globalDef.type);
		serialize(stream, globalDef.initializer);
	}

	template<typename Stream> void serialize(Stream& stream, ExceptionTypeDef& exceptionTypeDef)
	{
		serialize(stream, exceptionTypeDef.type);
	}

	template<typename Stream> void serialize(Stream& stream, ElemSegment& elemSegment)
	{
		if(Stream::isInput)
		{
			U32 flags = 0;
			serializeVarUInt32(stream, flags);

			switch(flags)
			{
			case 0:
				elemSegment.isActive = true;
				elemSegment.tableIndex = 0;
				serialize(stream, elemSegment.baseOffset);
				break;
			case 1:
				elemSegment.isActive = false;
				elemSegment.tableIndex = UINTPTR_MAX;
				elemSegment.baseOffset = {};
				break;
			case 2:
				elemSegment.isActive = true;
				serializeVarUInt32(stream, elemSegment.tableIndex);
				serialize(stream, elemSegment.baseOffset);
				break;
			default: throw FatalSerializationException("invalid elem segment flags");
			};
		}
		else
		{
			if(!elemSegment.isActive) { serializeConstant<U8>(stream, "", 1); }
			else
			{
				if(elemSegment.tableIndex == 0) { serializeConstant<U8>(stream, "", 0); }
				else
				{
					serializeConstant<U8>(stream, "", 2);
					serializeVarUInt32(stream, elemSegment.tableIndex);
				}
				serialize(stream, elemSegment.baseOffset);
			}
		}
		if(elemSegment.isActive)
		{
			serializeArray(stream, elemSegment.elems, [](Stream& stream, Elem& elem) {
				if(Stream::isInput) { elem.type = Elem::Type::ref_func; }
				wavmAssert(elem.type == Elem::Type::ref_func);
				serializeVarUInt32(stream, elem.index);
			});
		}
		else
		{
			serialize(stream, elemSegment.elemType);
			serializeArray(stream, elemSegment.elems, [](Stream& stream, Elem& elem) {
				serializeOpcode(stream, elem.typeOpcode);
				switch(elem.type)
				{
				case Elem::Type::ref_null: break;
				case Elem::Type::ref_func: serializeVarUInt32(stream, elem.index); break;
				default: throw FatalSerializationException("invalid elem opcode");
				};
				serializeConstant(stream, "expected end opcode", (U8)Opcode::end);
			});
		}
	}

	template<typename Stream> void serialize(Stream& stream, DataSegment& dataSegment)
	{
		if(Stream::isInput)
		{
			U32 flags = 0;
			serializeVarUInt32(stream, flags);

			switch(flags)
			{
			case 0:
				dataSegment.isActive = true;
				dataSegment.memoryIndex = 0;
				serialize(stream, dataSegment.baseOffset);
				break;
			case 1:
				dataSegment.isActive = false;
				dataSegment.memoryIndex = UINTPTR_MAX;
				dataSegment.baseOffset = {};
				break;
			case 2:
				dataSegment.isActive = true;
				serializeVarUInt32(stream, dataSegment.memoryIndex);
				serialize(stream, dataSegment.baseOffset);
				break;
			default: throw FatalSerializationException("invalid data segment flags");
			};
		}
		else
		{
			if(!dataSegment.isActive) { serializeConstant<U8>(stream, "", 1); }
			else
			{
				if(dataSegment.memoryIndex == 0) { serializeConstant<U8>(stream, "", 0); }
				else
				{
					serializeConstant<U8>(stream, "", 2);
					serializeVarUInt32(stream, dataSegment.memoryIndex);
				}
				serialize(stream, dataSegment.baseOffset);
			}
		}
		serialize(stream, dataSegment.data);
	}
}}

enum
{
	magicNumber = 0x6d736100, // "\0asm"
	currentVersion = 1
};

enum class SectionType : U8
{
	unknown,
	type,
	import,
	function,
	table,
	memory,
	global,
	exceptionTypes,
	export_,
	start,
	elem,
	dataCount,
	code,
	data,
	user
};

static void serialize(InputStream& stream, SectionType& sectionType)
{
	const U8 serializedSectionId = *stream.advance(1);
	switch(serializedSectionId)
	{
	case 0: sectionType = SectionType::user; break;
	case 1: sectionType = SectionType::type; break;
	case 2: sectionType = SectionType::import; break;
	case 3: sectionType = SectionType::function; break;
	case 4: sectionType = SectionType::table; break;
	case 5: sectionType = SectionType::memory; break;
	case 6: sectionType = SectionType::global; break;
	case 7: sectionType = SectionType::export_; break;
	case 8: sectionType = SectionType::start; break;
	case 9: sectionType = SectionType::elem; break;
	case 10: sectionType = SectionType::code; break;
	case 11: sectionType = SectionType::data; break;
	case 12: sectionType = SectionType ::dataCount; break;
	case 0x7f: sectionType = SectionType::exceptionTypes; break;
	default:
		throw FatalSerializationException(std::string("invalid section type: ")
										  + std::to_string(serializedSectionId));
	};
}

static void serialize(OutputStream& stream, SectionType sectionType)
{
	U8 serializedSectionId;
	switch(sectionType)
	{
	case SectionType::user: serializedSectionId = 0; break;
	case SectionType::type: serializedSectionId = 1; break;
	case SectionType::import: serializedSectionId = 2; break;
	case SectionType::function: serializedSectionId = 3; break;
	case SectionType::table: serializedSectionId = 4; break;
	case SectionType::memory: serializedSectionId = 5; break;
	case SectionType::global: serializedSectionId = 6; break;
	case SectionType::export_: serializedSectionId = 7; break;
	case SectionType::start: serializedSectionId = 8; break;
	case SectionType::elem: serializedSectionId = 9; break;
	case SectionType::code: serializedSectionId = 10; break;
	case SectionType::data: serializedSectionId = 11; break;
	case SectionType::dataCount: serializedSectionId = 12; break;
	case SectionType::exceptionTypes: serializedSectionId = 0x7f; break;
	default: Errors::unreachable();
	};

	*stream.advance(1) = serializedSectionId;
}

template<typename Stream> void serialize(Stream& stream, NoImm&, const FunctionDef&) {}

static void serialize(InputStream& stream, ControlStructureImm& imm, const FunctionDef&)
{
	Iptr encodedBlockType;
	serializeVarInt32(stream, encodedBlockType);
	if(encodedBlockType >= 0)
	{
		imm.type.format = IndexedBlockType::functionType;
		imm.type.index = encodedBlockType;
	}
	else if(encodedBlockType == -64)
	{
		imm.type.format = IndexedBlockType::noParametersOrResult;
		imm.type.resultType = ValueType::any;
	}
	else
	{
		imm.type.format = IndexedBlockType::oneResult;
		imm.type.resultType = decodeValueType(encodedBlockType);
	}
}

static void serialize(OutputStream& stream, const ControlStructureImm& imm, const FunctionDef&)
{
	Iptr encodedBlockType;
	switch(imm.type.format)
	{
	case IndexedBlockType::noParametersOrResult: encodedBlockType = -64; break;
	case IndexedBlockType::oneResult:
		encodedBlockType = encodeValueType(imm.type.resultType);
		break;
	case IndexedBlockType::functionType: encodedBlockType = imm.type.index; break;
	default: Errors::unreachable();
	};
	serializeVarInt32(stream, encodedBlockType);
}

template<typename Stream> void serialize(Stream& stream, BranchImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.targetDepth);
}

static void serialize(InputStream& stream, BranchTableImm& imm, FunctionDef& functionDef)
{
	std::vector<Uptr> branchTable;
	serializeArray(stream, branchTable, [](InputStream& stream, Uptr& targetDepth) {
		serializeVarUInt32(stream, targetDepth);
	});
	imm.branchTableIndex = functionDef.branchTables.size();
	functionDef.branchTables.push_back(std::move(branchTable));
	serializeVarUInt32(stream, imm.defaultTargetDepth);
}
static void serialize(OutputStream& stream, BranchTableImm& imm, FunctionDef& functionDef)
{
	wavmAssert(imm.branchTableIndex < functionDef.branchTables.size());
	std::vector<Uptr>& branchTable = functionDef.branchTables[imm.branchTableIndex];
	serializeArray(stream, branchTable, [](OutputStream& stream, Uptr& targetDepth) {
		serializeVarUInt32(stream, targetDepth);
	});
	serializeVarUInt32(stream, imm.defaultTargetDepth);
}

template<typename Stream> void serialize(Stream& stream, LiteralImm<I32>& imm, const FunctionDef&)
{
	serializeVarInt32(stream, imm.value);
}

template<typename Stream> void serialize(Stream& stream, LiteralImm<I64>& imm, const FunctionDef&)
{
	serializeVarInt64(stream, imm.value);
}

template<typename Stream, bool isGlobal>
void serialize(Stream& stream, GetOrSetVariableImm<isGlobal>& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.variableIndex);
}

template<typename Stream> void serialize(Stream& stream, FunctionImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.functionIndex);
}

template<typename Stream> void serialize(Stream& stream, CallIndirectImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.type.index);
	serializeVarUInt32(stream, imm.tableIndex);
}

template<typename Stream, Uptr naturalAlignmentLog2>
void serialize(Stream& stream, LoadOrStoreImm<naturalAlignmentLog2>& imm, const FunctionDef&)
{
	serializeVarUInt7(stream, imm.alignmentLog2);
	serializeVarUInt32(stream, imm.offset);
}
template<typename Stream> void serialize(Stream& stream, MemoryImm& imm, const FunctionDef&)
{
	serializeConstant(stream, "memory.(grow|size|fill) immediate memory field must be 0", U8(0));
	if(Stream::isInput) { imm.memoryIndex = 0; }
}
template<typename Stream> void serialize(Stream& stream, MemoryCopyImm& imm, const FunctionDef&)
{
	serializeConstant(stream, "memory.copy immediate source field must be 0", U8(0));
	if(Stream::isInput) { imm.sourceMemoryIndex = 0; }
	serializeConstant(stream, "memory.copy immediate dest field must be 0", U8(0));
	if(Stream::isInput) { imm.destMemoryIndex = 0; }
}
template<typename Stream> void serialize(Stream& stream, TableImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.tableIndex);
}
template<typename Stream> void serialize(Stream& stream, TableCopyImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.sourceTableIndex);
	serializeVarUInt32(stream, imm.destTableIndex);
}

template<typename Stream> void serialize(Stream& stream, V128& v128)
{
	serializeNativeValue(stream, v128);
}

template<typename Stream, Uptr numLanes>
void serialize(Stream& stream, LaneIndexImm<numLanes>& imm, const FunctionDef&)
{
	serializeVarUInt7(stream, imm.laneIndex);
}

template<typename Stream, Uptr numLanes>
void serialize(Stream& stream, ShuffleImm<numLanes>& imm, const FunctionDef&)
{
	for(Uptr laneIndex = 0; laneIndex < numLanes; ++laneIndex)
	{ serializeVarUInt7(stream, imm.laneIndices[laneIndex]); }
}

template<typename Stream, Uptr naturalAlignmentLog2>
void serialize(Stream& stream, AtomicLoadOrStoreImm<naturalAlignmentLog2>& imm, const FunctionDef&)
{
	serializeVarUInt7(stream, imm.alignmentLog2);
	serializeVarUInt32(stream, imm.offset);
}

template<typename Stream> void serialize(Stream& stream, ExceptionTypeImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.exceptionTypeIndex);
}

template<typename Stream> void serialize(Stream& stream, RethrowImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.catchDepth);
}

template<typename Stream>
void serialize(Stream& stream, DataSegmentAndMemImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.dataSegmentIndex);
	serializeVarUInt32(stream, imm.memoryIndex);
}

template<typename Stream> void serialize(Stream& stream, DataSegmentImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.dataSegmentIndex);
}

template<typename Stream>
void serialize(Stream& stream, ElemSegmentAndTableImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.elemSegmentIndex);
	serializeVarUInt32(stream, imm.tableIndex);
}

template<typename Stream> void serialize(Stream& stream, ElemSegmentImm& imm, const FunctionDef&)
{
	serializeVarUInt32(stream, imm.elemSegmentIndex);
}

template<typename Stream, typename Value>
void serialize(Stream& stream, LiteralImm<Value>& imm, const FunctionDef&)
{
	serialize(stream, imm.value);
}

template<typename SerializeSection>
void serializeSection(OutputStream& stream, SectionType type, SerializeSection serializeSectionBody)
{
	serialize(stream, type);
	ArrayOutputStream sectionStream;
	serializeSectionBody(sectionStream);
	std::vector<U8> sectionBytes = sectionStream.getBytes();
	Uptr sectionNumBytes = sectionBytes.size();
	serializeVarUInt32(stream, sectionNumBytes);
	serializeBytes(stream, sectionBytes.data(), sectionBytes.size());
}
template<typename SerializeSection>
void serializeSection(InputStream& stream, SectionType type, SerializeSection serializeSectionBody)
{
	Uptr numSectionBytes = 0;
	serializeVarUInt32(stream, numSectionBytes);
	MemoryInputStream sectionStream(stream.advance(numSectionBytes), numSectionBytes);
	serializeSectionBody(sectionStream);
	if(sectionStream.capacity())
	{ throw FatalSerializationException("section contained more data than expected"); }
}

static void serialize(OutputStream& stream, UserSection& userSection)
{
	serialize(stream, SectionType::user);
	ArrayOutputStream sectionStream;
	serialize(sectionStream, userSection.name);
	serializeBytes(sectionStream, userSection.data.data(), userSection.data.size());
	std::vector<U8> sectionBytes = sectionStream.getBytes();
	serialize(stream, sectionBytes);
}

static void serialize(InputStream& stream, UserSection& userSection)
{
	Uptr numSectionBytes = 0;
	serializeVarUInt32(stream, numSectionBytes);

	MemoryInputStream sectionStream(stream.advance(numSectionBytes), numSectionBytes);
	serialize(sectionStream, userSection.name);
	throwIfNotValidUTF8(userSection.name);
	userSection.data.resize(sectionStream.capacity());
	serializeBytes(sectionStream, userSection.data.data(), userSection.data.size());
	wavmAssert(!sectionStream.capacity());
}

struct LocalSet
{
	Uptr num;
	ValueType type;
};

template<typename Stream> void serialize(Stream& stream, LocalSet& localSet)
{
	serializeVarUInt32(stream, localSet.num);
	serialize(stream, localSet.type);
}

struct OperatorSerializerStream
{
	typedef void Result;

	OperatorSerializerStream(Serialization::OutputStream& inByteStream, FunctionDef& inFunctionDef)
	: byteStream(inByteStream), functionDef(inFunctionDef)
	{
	}

#define VISIT_OPCODE(_, name, nameString, Imm, ...)                                                \
	void name(Imm imm) const                                                                       \
	{                                                                                              \
		Opcode opcode = Opcode::name;                                                              \
		serializeOpcode(byteStream, opcode);                                                       \
		serialize(byteStream, imm, functionDef);                                                   \
	}
	ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE

	void unknown(Opcode opcode) { throw FatalSerializationException("unknown opcode"); }

private:
	Serialization::OutputStream& byteStream;
	FunctionDef& functionDef;
};

static void serializeFunctionBody(OutputStream& sectionStream,
								  Module& module,
								  FunctionDef& functionDef)
{
	ArrayOutputStream bodyStream;

	// Convert the function's local types into LocalSets: runs of locals of the same type.
	LocalSet* localSets
		= (LocalSet*)alloca(sizeof(LocalSet) * functionDef.nonParameterLocalTypes.size());
	Uptr numLocalSets = 0;
	if(functionDef.nonParameterLocalTypes.size())
	{
		localSets[0].type = ValueType::any;
		localSets[0].num = 0;
		for(auto localType : functionDef.nonParameterLocalTypes)
		{
			if(localSets[numLocalSets].type != localType)
			{
				if(localSets[numLocalSets].type != ValueType::any) { ++numLocalSets; }
				localSets[numLocalSets].type = localType;
				localSets[numLocalSets].num = 0;
			}
			++localSets[numLocalSets].num;
		}
		if(localSets[numLocalSets].type != ValueType::any) { ++numLocalSets; }
	}

	// Serialize the local sets.
	serializeVarUInt32(bodyStream, numLocalSets);
	for(Uptr setIndex = 0; setIndex < numLocalSets; ++setIndex)
	{ serialize(bodyStream, localSets[setIndex]); }

	// Serialize the function code.
	OperatorDecoderStream irDecoderStream(functionDef.code);
	OperatorSerializerStream wasmOpEncoderStream(bodyStream, functionDef);
	while(irDecoderStream) { irDecoderStream.decodeOp(wasmOpEncoderStream); };

	std::vector<U8> bodyBytes = bodyStream.getBytes();
	serialize(sectionStream, bodyBytes);
}

static void serializeFunctionBody(InputStream& sectionStream,
								  Module& module,
								  FunctionDef& functionDef)
{
	Uptr numBodyBytes = 0;
	serializeVarUInt32(sectionStream, numBodyBytes);

	MemoryInputStream bodyStream(sectionStream.advance(numBodyBytes), numBodyBytes);

	// Deserialize local sets and unpack them into a linear array of local types.
	Uptr numLocalSets = 0;
	serializeVarUInt32(bodyStream, numLocalSets);
	for(Uptr setIndex = 0; setIndex < numLocalSets; ++setIndex)
	{
		LocalSet localSet;
		serialize(bodyStream, localSet);
		if(functionDef.nonParameterLocalTypes.size() + localSet.num >= module.featureSpec.maxLocals)
		{ throw FatalSerializationException("too many locals"); }
		for(Uptr index = 0; index < localSet.num; ++index)
		{ functionDef.nonParameterLocalTypes.push_back(localSet.type); }
	}

	// Deserialize the function code, validate it, and re-encode it in the IR format.
	ArrayOutputStream irCodeByteStream;
	OperatorEncoderStream irEncoderStream(irCodeByteStream);
	CodeValidationStream codeValidationStream(module, functionDef);
	while(bodyStream.capacity())
	{
		Opcode opcode;
		serializeOpcode(bodyStream, opcode);
		switch(opcode)
		{
#define VISIT_OPCODE(_, name, nameString, Imm, ...)                                                \
	case Opcode::name:                                                                             \
	{                                                                                              \
		Imm imm;                                                                                   \
		serialize(bodyStream, imm, functionDef);                                                   \
		codeValidationStream.name(imm);                                                            \
		irEncoderStream.name(imm);                                                                 \
		break;                                                                                     \
	}
			ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
		default: throw FatalSerializationException("unknown opcode");
		};
	};
	codeValidationStream.finish();

	functionDef.code = std::move(irCodeByteStream.getBytes());
}

template<typename Stream> void serializeTypeSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::type, [&module](Stream& sectionStream) {
		serializeArray(sectionStream, module.types, [](Stream& stream, FunctionType& functionType) {
			serializeConstant(stream, "function type tag", U8(0x60));
			if(Stream::isInput)
			{
				TypeTuple params;
				serialize(stream, params);

				TypeTuple results;
				serialize(stream, results);

				functionType = FunctionType(results, params);
			}
			else
			{
				TypeTuple params = functionType.params();
				serialize(stream, params);

				TypeTuple results = functionType.results();
				serialize(stream, results);
			}
		});
	});
}
template<typename Stream> void serializeImportSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::import, [&module](Stream& sectionStream) {
		Uptr size = module.functions.imports.size() + module.tables.imports.size()
					+ module.memories.imports.size() + module.globals.imports.size()
					+ module.exceptionTypes.imports.size();
		serializeVarUInt32(sectionStream, size);
		if(Stream::isInput)
		{
			for(Uptr index = 0; index < size; ++index)
			{
				std::string moduleName;
				std::string exportName;
				ExternKind kind = ExternKind::invalid;
				serialize(sectionStream, moduleName);
				serialize(sectionStream, exportName);
				throwIfNotValidUTF8(moduleName);
				throwIfNotValidUTF8(exportName);
				serialize(sectionStream, kind);
				switch(kind)
				{
				case ExternKind::function:
				{
					U32 functionTypeIndex = 0;
					serializeVarUInt32(sectionStream, functionTypeIndex);
					if(functionTypeIndex >= module.types.size())
					{ throw FatalSerializationException("invalid import function type index"); }
					module.functions.imports.push_back(
						{{functionTypeIndex}, std::move(moduleName), std::move(exportName)});
					break;
				}
				case ExternKind::table:
				{
					TableType tableType;
					serialize(sectionStream, tableType);
					module.tables.imports.push_back(
						{tableType, std::move(moduleName), std::move(exportName)});
					break;
				}
				case ExternKind::memory:
				{
					MemoryType memoryType;
					serialize(sectionStream, memoryType);
					module.memories.imports.push_back(
						{memoryType, std::move(moduleName), std::move(exportName)});
					break;
				}
				case ExternKind::global:
				{
					GlobalType globalType;
					serialize(sectionStream, globalType);
					module.globals.imports.push_back(
						{globalType, std::move(moduleName), std::move(exportName)});
					break;
				}
				case ExternKind::exceptionType:
				{
					ExceptionType exceptionType;
					serialize(sectionStream, exceptionType);
					module.exceptionTypes.imports.push_back(
						{exceptionType, std::move(moduleName), std::move(exportName)});
					break;
				}
				default: throw FatalSerializationException("invalid ExternKind");
				}
			}
		}
		else
		{
			for(auto& functionImport : module.functions.imports)
			{
				serialize(sectionStream, functionImport.moduleName);
				serialize(sectionStream, functionImport.exportName);
				ExternKind kind = ExternKind::function;
				serialize(sectionStream, kind);
				serializeVarUInt32(sectionStream, functionImport.type.index);
			}
			for(auto& tableImport : module.tables.imports)
			{
				serialize(sectionStream, tableImport.moduleName);
				serialize(sectionStream, tableImport.exportName);
				ExternKind kind = ExternKind::table;
				serialize(sectionStream, kind);
				serialize(sectionStream, tableImport.type);
			}
			for(auto& memoryImport : module.memories.imports)
			{
				serialize(sectionStream, memoryImport.moduleName);
				serialize(sectionStream, memoryImport.exportName);
				ExternKind kind = ExternKind::memory;
				serialize(sectionStream, kind);
				serialize(sectionStream, memoryImport.type);
			}
			for(auto& globalImport : module.globals.imports)
			{
				serialize(sectionStream, globalImport.moduleName);
				serialize(sectionStream, globalImport.exportName);
				ExternKind kind = ExternKind::global;
				serialize(sectionStream, kind);
				serialize(sectionStream, globalImport.type);
			}
			for(auto& exceptionTypeImport : module.exceptionTypes.imports)
			{
				serialize(sectionStream, exceptionTypeImport.moduleName);
				serialize(sectionStream, exceptionTypeImport.exportName);
				ExternKind kind = ExternKind::exceptionType;
				serialize(sectionStream, kind);
				serialize(sectionStream, exceptionTypeImport.type);
			}
		}
	});
}

template<typename Stream> void serializeFunctionSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::function, [&module](Stream& sectionStream) {
		Uptr numFunctions = module.functions.defs.size();
		serializeVarUInt32(sectionStream, numFunctions);
		if(Stream::isInput)
		{
			// Grow the vector one element at a time: try to get a serialization exception
			// before making a huge allocation for malformed input.
			module.functions.defs.clear();
			for(Uptr functionIndex = 0; functionIndex < numFunctions; ++functionIndex)
			{
				U32 functionTypeIndex = 0;
				serializeVarUInt32(sectionStream, functionTypeIndex);
				if(functionTypeIndex >= module.types.size())
				{ throw FatalSerializationException("invalid function type index"); }
				module.functions.defs.push_back({{functionTypeIndex}, {}, {}, {}});
			}
			module.functions.defs.shrink_to_fit();
		}
		else
		{
			for(FunctionDef& function : module.functions.defs)
			{ serializeVarUInt32(sectionStream, function.type.index); }
		}
	});
}

template<typename Stream> void serializeTableSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::table, [&module](Stream& sectionStream) {
		serialize(sectionStream, module.tables.defs);
	});
}

template<typename Stream> void serializeMemorySection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::memory, [&module](Stream& sectionStream) {
		serialize(sectionStream, module.memories.defs);
	});
}

template<typename Stream> void serializeGlobalSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::global, [&module](Stream& sectionStream) {
		serialize(sectionStream, module.globals.defs);
	});
}

template<typename Stream> void serializeExceptionTypeSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::exceptionTypes, [&module](Stream& sectionStream) {
		serialize(sectionStream, module.exceptionTypes.defs);
	});
}

template<typename Stream> void serializeExportSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::export_, [&module](Stream& sectionStream) {
		serialize(sectionStream, module.exports);
	});
}

template<typename Stream> void serializeStartSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::start, [&module](Stream& sectionStream) {
		serializeVarUInt32(sectionStream, module.startFunctionIndex);
	});
}

template<typename Stream> void serializeElementSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::elem, [&module](Stream& sectionStream) {
		serialize(sectionStream, module.elemSegments);
	});
}

static void serializeCodeSection(InputStream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::code, [&module](InputStream& sectionStream) {
		Uptr numFunctionBodies = module.functions.defs.size();
		serializeVarUInt32(sectionStream, numFunctionBodies);
		if(numFunctionBodies != module.functions.defs.size())
		{
			throw FatalSerializationException(
				"function and code sections have mismatched function counts");
		}
		for(FunctionDef& functionDef : module.functions.defs)
		{ serializeFunctionBody(sectionStream, module, functionDef); }
	});
}

void serializeCodeSection(OutputStream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::code, [&module](OutputStream& sectionStream) {
		Uptr numFunctionBodies = module.functions.defs.size();
		serializeVarUInt32(sectionStream, numFunctionBodies);
		for(FunctionDef& functionDef : module.functions.defs)
		{ serializeFunctionBody(sectionStream, module, functionDef); }
	});
}

template<typename Stream> void serializeDataCountSection(Stream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::dataCount, [&module](Stream& sectionStream) {
		Uptr numDataSegments = module.dataSegments.size();
		serializeVarUInt32(sectionStream, numDataSegments);
		if(Stream::isInput)
		{
			// To make fuzzing more effective, fail gracefully instead of through OOM if the
			// DataCount section specifies a large number of data segments.
			if(numDataSegments > module.featureSpec.maxDataSegments)
			{ throw FatalSerializationException("too many data segments"); }

			module.dataSegments.resize(numDataSegments);
		}
	});
}

void serializeDataSection(InputStream& moduleStream, Module& module, bool hadDataCountSection)
{
	serializeSection(moduleStream,
					 SectionType::data,
					 [&module, hadDataCountSection](InputStream& sectionStream) {
						 Uptr numDataSegments = 0;
						 serializeVarUInt32(sectionStream, numDataSegments);
						 if(!hadDataCountSection)
						 {
							 // To make fuzzing more effective, fail gracefully instead of through
							 // OOM if the DataCount section specifies a large number of data
							 // segments.
							 if(numDataSegments > module.featureSpec.maxDataSegments)
							 { throw FatalSerializationException("too many data segments"); }
							 module.dataSegments.resize(numDataSegments);
						 }
						 else if(numDataSegments != module.dataSegments.size())
						 {
							 throw FatalSerializationException(
								 "DataCount and Data sections have mismatched segment counts");
						 }
						 for(Uptr segmentIndex = 0; segmentIndex < module.dataSegments.size();
							 ++segmentIndex)
						 { serialize(sectionStream, module.dataSegments[segmentIndex]); }
					 });
}

void serializeDataSection(OutputStream& moduleStream, Module& module)
{
	serializeSection(moduleStream, SectionType::data, [&module](OutputStream& sectionStream) {
		serialize(sectionStream, module.dataSegments);
	});
}

static void serializeModule(OutputStream& moduleStream, Module& module)
{
	serializeConstant(moduleStream, "magic number", U32(magicNumber));
	serializeConstant(moduleStream, "version", U32(currentVersion));

	if(module.types.size() > 0) { serializeTypeSection(moduleStream, module); }
	if(module.functions.imports.size() > 0 || module.tables.imports.size() > 0
	   || module.memories.imports.size() > 0 || module.globals.imports.size() > 0
	   || module.exceptionTypes.imports.size() > 0)
	{ serializeImportSection(moduleStream, module); }
	if(module.functions.defs.size() > 0) { serializeFunctionSection(moduleStream, module); }
	if(module.tables.defs.size() > 0) { serializeTableSection(moduleStream, module); }
	if(module.memories.defs.size() > 0) { serializeMemorySection(moduleStream, module); }
	if(module.globals.defs.size() > 0) { serializeGlobalSection(moduleStream, module); }
	if(module.exceptionTypes.defs.size() > 0)
	{ serializeExceptionTypeSection(moduleStream, module); }
	if(module.exports.size() > 0) { serializeExportSection(moduleStream, module); }
	if(module.startFunctionIndex != UINTPTR_MAX) { serializeStartSection(moduleStream, module); }
	if(module.elemSegments.size() > 0) { serializeElementSection(moduleStream, module); }
	if(module.dataSegments.size() > 0) { serializeDataCountSection(moduleStream, module); }
	if(module.functions.defs.size() > 0) { serializeCodeSection(moduleStream, module); }
	if(module.dataSegments.size() > 0) { serializeDataSection(moduleStream, module); }

	for(auto& userSection : module.userSections) { serialize(moduleStream, userSection); }
}

static void serializeModule(InputStream& moduleStream, Module& module)
{
	serializeConstant(moduleStream, "magic number", U32(magicNumber));
	serializeConstant(moduleStream, "version", U32(currentVersion));

	SectionType lastKnownSectionType = SectionType::unknown;
	bool hadFunctionDefinitions = false;
	bool hadDataCountSection = false;
	bool hadDataSection = false;
	while(moduleStream.capacity())
	{
		SectionType sectionType;
		serialize(moduleStream, sectionType);

		if(sectionType != SectionType::user)
		{
			if(sectionType > lastKnownSectionType) { lastKnownSectionType = sectionType; }
			else
			{
				throw FatalSerializationException("incorrect order for known section");
			}
		}

		switch(sectionType)
		{
		case SectionType::type:
			serializeTypeSection(moduleStream, module);
			IR::validateTypes(module);
			break;
		case SectionType::import:
			serializeImportSection(moduleStream, module);
			IR::validateImports(module);
			break;
		case SectionType::function:
			serializeFunctionSection(moduleStream, module);
			IR::validateFunctionDeclarations(module);
			break;
		case SectionType::table:
			serializeTableSection(moduleStream, module);
			IR::validateTableDefs(module);
			break;
		case SectionType::memory:
			serializeMemorySection(moduleStream, module);
			IR::validateMemoryDefs(module);
			break;
		case SectionType::global:
			serializeGlobalSection(moduleStream, module);
			IR::validateGlobalDefs(module);
			break;
		case SectionType::exceptionTypes:
			serializeExceptionTypeSection(moduleStream, module);
			IR::validateExceptionTypeDefs(module);
			break;
		case SectionType::export_:
			serializeExportSection(moduleStream, module);
			IR::validateExports(module);
			break;
		case SectionType::start:
			serializeStartSection(moduleStream, module);
			IR::validateStartFunction(module);
			break;
		case SectionType::elem:
			serializeElementSection(moduleStream, module);
			IR::validateElemSegments(module);
			break;
		case SectionType::dataCount:
			serializeDataCountSection(moduleStream, module);
			hadDataCountSection = true;
			break;
		case SectionType::code:
			serializeCodeSection(moduleStream, module);
			hadFunctionDefinitions = true;
			break;
		case SectionType::data:
			serializeDataSection(moduleStream, module, hadDataCountSection);
			hadDataSection = true;
			IR::validateDataSegments(module);
			break;
		case SectionType::user:
		{
			UserSection& userSection
				= *module.userSections.insert(module.userSections.end(), UserSection());
			serialize(moduleStream, userSection);
			break;
		}
		default: throw FatalSerializationException("unknown section ID");
		};
	};

	if(module.functions.defs.size() && !hadFunctionDefinitions)
	{
		throw IR::ValidationException(
			"module contained function declarations, but no corresponding "
			"function definition section");
	}

	if(module.dataSegments.size() && !hadDataSection)
	{
		throw IR::ValidationException(
			"module contained DataCount section with non-zero segment count, but no corresponding "
			"Data section");
	}
}

void WASM::serialize(Serialization::InputStream& stream, Module& module)
{
	serializeModule(stream, module);
}
void WASM::serialize(Serialization::OutputStream& stream, const Module& module)
{
	serializeModule(stream, const_cast<Module&>(module));
}

bool WASM::loadBinaryModule(const void* wasmBytes,
							Uptr numBytes,
							IR::Module& outModule,
							Log::Category errorCategory)
{
	// Load the module from a binary WebAssembly file.
	try
	{
		Timing::Timer loadTimer;

		Serialization::MemoryInputStream stream((const U8*)wasmBytes, numBytes);
		WASM::serialize(stream, outModule);

		Timing::logRatePerSecond("Loaded WASM", loadTimer, numBytes / 1024.0 / 1024.0, "MB");
		return true;
	}
	catch(Serialization::FatalSerializationException const& exception)
	{
		Log::printf(errorCategory,
					"Error deserializing WebAssembly binary file:\n%s\n",
					exception.message.c_str());
		return false;
	}
	catch(IR::ValidationException const& exception)
	{
		Log::printf(errorCategory,
					"Error validating WebAssembly binary file:\n%s\n",
					exception.message.c_str());
		return false;
	}
	catch(std::bad_alloc const&)
	{
		Log::printf(errorCategory, "Memory allocation failed: input is likely malformed\n");
		return false;
	}
}
