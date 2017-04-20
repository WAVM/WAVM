#pragma once

#include "Core/Core.h"
#include "Inline/Serialization.h"
#include "IR.h"
#include "Types.h"

namespace IR
{
	using namespace Serialization;

	// Enumate the WebAssembly operators

	#define ENUM_CONTROL_OPERATORS(visitOp) \
		visitOp(0x02,block,"block",ControlStructureImm,CONTROL) \
		visitOp(0x03,loop,"loop",ControlStructureImm,CONTROL) \
		visitOp(0x04,if_,"if",ControlStructureImm,CONTROL) \
		visitOp(0x05,else_,"else",NoImm,CONTROL) \
		visitOp(0x0b,end,"end",NoImm,CONTROL)

	#define ENUM_PARAMETRIC_OPERATORS(visitOp) \
		visitOp(0x00,unreachable,"unreachable",NoImm,UNREACHABLE) \
		visitOp(0x0c,br,"br",BranchImm,BR) \
		visitOp(0x0d,br_if,"br_if",BranchImm,BR_IF) \
		visitOp(0x0e,br_table,"br_table",BranchTableImm,BR_TABLE) \
		visitOp(0x0f,return_,"return",NoImm,RETURN) \
		visitOp(0x10,call,"call",CallImm,CALL) \
		visitOp(0x11,call_indirect,"call_indirect",CallIndirectImm,CALLINDIRECT) \
		visitOp(0x1a,drop,"drop",NoImm,DROP) \
		visitOp(0x1b,select,"select",NoImm,SELECT) \
		\
		visitOp(0x20,get_local,"get_local",GetOrSetVariableImm,GETLOCAL) \
		visitOp(0x21,set_local,"set_local",GetOrSetVariableImm,SETLOCAL) \
		visitOp(0x22,tee_local,"tee_local",GetOrSetVariableImm,TEELOCAL) \
		visitOp(0x23,get_global,"get_global",GetOrSetVariableImm,GETGLOBAL) \
		visitOp(0x24,set_global,"set_global",GetOrSetVariableImm,SETGLOBAL)
	
	/*
		Possible signatures used by ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS:
		NULLARY(T) : () -> T
		UNARY(T,U) : T -> U
		BINARY(T,U) : (T,T) -> U
		LOAD(T) : i32 -> T
		STORE(T) : (i32,T) -> T
	*/

	#define ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(visitOp) \
		visitOp(0x01,nop,"nop",NoImm,NULLARY(none)) \
		\
		visitOp(0x3f,current_memory,"current_memory",MemoryImm,NULLARY(i32)) \
		visitOp(0x40,grow_memory,"grow_memory",MemoryImm,UNARY(i32,i32)) \
		\
		visitOp(0x28,i32_load,"i32.load",LoadOrStoreImm<2>,LOAD(i32)) \
		visitOp(0x29,i64_load,"i64.load",LoadOrStoreImm<3>,LOAD(i64)) \
		visitOp(0x2a,f32_load,"f32.load",LoadOrStoreImm<2>,LOAD(f32)) \
		visitOp(0x2b,f64_load,"f64.load",LoadOrStoreImm<3>,LOAD(f64)) \
		visitOp(0x2c,i32_load8_s,"i32.load8_s",LoadOrStoreImm<0>,LOAD(i32)) \
		visitOp(0x2d,i32_load8_u,"i32.load8_u",LoadOrStoreImm<0>,LOAD(i32)) \
		visitOp(0x2e,i32_load16_s,"i32.load16_s",LoadOrStoreImm<1>,LOAD(i32)) \
		visitOp(0x2f,i32_load16_u,"i32.load16_u",LoadOrStoreImm<1>,LOAD(i32)) \
		visitOp(0x30,i64_load8_s,"i64.load8_s",LoadOrStoreImm<0>,LOAD(i64)) \
		visitOp(0x31,i64_load8_u,"i64.load8_u",LoadOrStoreImm<0>,LOAD(i64)) \
		visitOp(0x32,i64_load16_s,"i64.load16_s",LoadOrStoreImm<1>,LOAD(i64)) \
		visitOp(0x33,i64_load16_u,"i64.load16_u",LoadOrStoreImm<1>,LOAD(i64)) \
		visitOp(0x34,i64_load32_s,"i64.load32_s",LoadOrStoreImm<2>,LOAD(i64)) \
		visitOp(0x35,i64_load32_u,"i64.load32_u",LoadOrStoreImm<2>,LOAD(i64)) \
		\
		visitOp(0x36,i32_store,"i32.store",LoadOrStoreImm<2>,STORE(i32)) \
		visitOp(0x37,i64_store,"i64.store",LoadOrStoreImm<3>,STORE(i64)) \
		visitOp(0x38,f32_store,"f32.store",LoadOrStoreImm<2>,STORE(f32)) \
		visitOp(0x39,f64_store,"f64.store",LoadOrStoreImm<3>,STORE(f64)) \
		visitOp(0x3a,i32_store8,"i32.store8",LoadOrStoreImm<0>,STORE(i32)) \
		visitOp(0x3b,i32_store16,"i32.store16",LoadOrStoreImm<1>,STORE(i32)) \
		visitOp(0x3c,i64_store8,"i64.store8",LoadOrStoreImm<0>,STORE(i64)) \
		visitOp(0x3d,i64_store16,"i64.store16",LoadOrStoreImm<1>,STORE(i64)) \
		visitOp(0x3e,i64_store32,"i64.store32",LoadOrStoreImm<2>,STORE(i64)) \
		\
		visitOp(0x41,i32_const,"i32.const",LiteralImm<int32>,NULLARY(i32)) \
		visitOp(0x42,i64_const,"i64.const",LiteralImm<int64>,NULLARY(i64)) \
		visitOp(0x43,f32_const,"f32.const",LiteralImm<float32>,NULLARY(f32)) \
		visitOp(0x44,f64_const,"f64.const",LiteralImm<float64>,NULLARY(f64)) \
		\
		visitOp(0x45,i32_eqz,"i32.eqz",NoImm,UNARY(i32,i32)) \
		visitOp(0x46,i32_eq,"i32.eq",NoImm,BINARY(i32,i32)) \
		visitOp(0x47,i32_ne,"i32.ne",NoImm,BINARY(i32,i32)) \
		visitOp(0x48,i32_lt_s,"i32.lt_s",NoImm,BINARY(i32,i32)) \
		visitOp(0x49,i32_lt_u,"i32.lt_u",NoImm,BINARY(i32,i32)) \
		visitOp(0x4a,i32_gt_s,"i32.gt_s",NoImm,BINARY(i32,i32)) \
		visitOp(0x4b,i32_gt_u,"i32.gt_u",NoImm,BINARY(i32,i32)) \
		visitOp(0x4c,i32_le_s,"i32.le_s",NoImm,BINARY(i32,i32)) \
		visitOp(0x4d,i32_le_u,"i32.le_u",NoImm,BINARY(i32,i32)) \
		visitOp(0x4e,i32_ge_s,"i32.ge_s",NoImm,BINARY(i32,i32)) \
		visitOp(0x4f,i32_ge_u,"i32.ge_u",NoImm,BINARY(i32,i32)) \
		\
		visitOp(0x50,i64_eqz,"i64.eqz",NoImm,UNARY(i64,i32)) \
		visitOp(0x51,i64_eq,"i64.eq",NoImm,BINARY(i64,i32)) \
		visitOp(0x52,i64_ne,"i64.ne",NoImm,BINARY(i64,i32)) \
		visitOp(0x53,i64_lt_s,"i64.lt_s",NoImm,BINARY(i64,i32)) \
		visitOp(0x54,i64_lt_u,"i64.lt_u",NoImm,BINARY(i64,i32)) \
		visitOp(0x55,i64_gt_s,"i64.gt_s",NoImm,BINARY(i64,i32)) \
		visitOp(0x56,i64_gt_u,"i64.gt_u",NoImm,BINARY(i64,i32)) \
		visitOp(0x57,i64_le_s,"i64.le_s",NoImm,BINARY(i64,i32)) \
		visitOp(0x58,i64_le_u,"i64.le_u",NoImm,BINARY(i64,i32)) \
		visitOp(0x59,i64_ge_s,"i64.ge_s",NoImm,BINARY(i64,i32)) \
		visitOp(0x5a,i64_ge_u,"i64.ge_u",NoImm,BINARY(i64,i32)) \
		\
		visitOp(0x5b,f32_eq,"f32.eq",NoImm,BINARY(f32,i32)) \
		visitOp(0x5c,f32_ne,"f32.ne",NoImm,BINARY(f32,i32)) \
		visitOp(0x5d,f32_lt,"f32.lt",NoImm,BINARY(f32,i32)) \
		visitOp(0x5e,f32_gt,"f32.gt",NoImm,BINARY(f32,i32)) \
		visitOp(0x5f,f32_le,"f32.le",NoImm,BINARY(f32,i32)) \
		visitOp(0x60,f32_ge,"f32.ge",NoImm,BINARY(f32,i32)) \
		\
		visitOp(0x61,f64_eq,"f64.eq",NoImm,BINARY(f64,i32)) \
		visitOp(0x62,f64_ne,"f64.ne",NoImm,BINARY(f64,i32)) \
		visitOp(0x63,f64_lt,"f64.lt",NoImm,BINARY(f64,i32)) \
		visitOp(0x64,f64_gt,"f64.gt",NoImm,BINARY(f64,i32)) \
		visitOp(0x65,f64_le,"f64.le",NoImm,BINARY(f64,i32)) \
		visitOp(0x66,f64_ge,"f64.ge",NoImm,BINARY(f64,i32)) \
		\
		visitOp(0x67,i32_clz,"i32.clz",NoImm,UNARY(i32,i32)) \
		visitOp(0x68,i32_ctz,"i32.ctz",NoImm,UNARY(i32,i32)) \
		visitOp(0x69,i32_popcnt,"i32.popcnt",NoImm,UNARY(i32,i32)) \
		\
		visitOp(0x6a,i32_add,"i32.add",NoImm,BINARY(i32,i32)) \
		visitOp(0x6b,i32_sub,"i32.sub",NoImm,BINARY(i32,i32)) \
		visitOp(0x6c,i32_mul,"i32.mul",NoImm,BINARY(i32,i32)) \
		visitOp(0x6d,i32_div_s,"i32.div_s",NoImm,BINARY(i32,i32)) \
		visitOp(0x6e,i32_div_u,"i32.div_u",NoImm,BINARY(i32,i32)) \
		visitOp(0x6f,i32_rem_s,"i32.rem_s",NoImm,BINARY(i32,i32)) \
		visitOp(0x70,i32_rem_u,"i32.rem_u",NoImm,BINARY(i32,i32)) \
		visitOp(0x71,i32_and,"i32.and",NoImm,BINARY(i32,i32)) \
		visitOp(0x72,i32_or,"i32.or",NoImm,BINARY(i32,i32)) \
		visitOp(0x73,i32_xor,"i32.xor",NoImm,BINARY(i32,i32)) \
		visitOp(0x74,i32_shl,"i32.shl",NoImm,BINARY(i32,i32)) \
		visitOp(0x75,i32_shr_s,"i32.shr_s",NoImm,BINARY(i32,i32)) \
		visitOp(0x76,i32_shr_u,"i32.shr_u",NoImm,BINARY(i32,i32)) \
		visitOp(0x77,i32_rotl,"i32.rotl",NoImm,BINARY(i32,i32)) \
		visitOp(0x78,i32_rotr,"i32.rotr",NoImm,BINARY(i32,i32)) \
		\
		visitOp(0x79,i64_clz,"i64.clz",NoImm,UNARY(i64,i64)) \
		visitOp(0x7a,i64_ctz,"i64.ctz",NoImm,UNARY(i64,i64)) \
		visitOp(0x7b,i64_popcnt,"i64.popcnt",NoImm,UNARY(i64,i64)) \
		\
		visitOp(0x7c,i64_add,"i64.add",NoImm,BINARY(i64,i64)) \
		visitOp(0x7d,i64_sub,"i64.sub",NoImm,BINARY(i64,i64)) \
		visitOp(0x7e,i64_mul,"i64.mul",NoImm,BINARY(i64,i64)) \
		visitOp(0x7f,i64_div_s,"i64.div_s",NoImm,BINARY(i64,i64)) \
		visitOp(0x80,i64_div_u,"i64.div_u",NoImm,BINARY(i64,i64)) \
		visitOp(0x81,i64_rem_s,"i64.rem_s",NoImm,BINARY(i64,i64)) \
		visitOp(0x82,i64_rem_u,"i64.rem_u",NoImm,BINARY(i64,i64)) \
		visitOp(0x83,i64_and,"i64.and",NoImm,BINARY(i64,i64)) \
		visitOp(0x84,i64_or,"i64.or",NoImm,BINARY(i64,i64)) \
		visitOp(0x85,i64_xor,"i64.xor",NoImm,BINARY(i64,i64)) \
		visitOp(0x86,i64_shl,"i64.shl",NoImm,BINARY(i64,i64)) \
		visitOp(0x87,i64_shr_s,"i64.shr_s",NoImm,BINARY(i64,i64)) \
		visitOp(0x88,i64_shr_u,"i64.shr_u",NoImm,BINARY(i64,i64)) \
		visitOp(0x89,i64_rotl,"i64.rotl",NoImm,BINARY(i64,i64)) \
		visitOp(0x8a,i64_rotr,"i64.rotr",NoImm,BINARY(i64,i64)) \
		\
		visitOp(0x8b,f32_abs,"f32.abs",NoImm,UNARY(f32,f32)) \
		visitOp(0x8c,f32_neg,"f32.neg",NoImm,UNARY(f32,f32)) \
		visitOp(0x8d,f32_ceil,"f32.ceil",NoImm,UNARY(f32,f32)) \
		visitOp(0x8e,f32_floor,"f32.floor",NoImm,UNARY(f32,f32)) \
		visitOp(0x8f,f32_trunc,"f32.trunc",NoImm,UNARY(f32,f32)) \
		visitOp(0x90,f32_nearest,"f32.nearest",NoImm,UNARY(f32,f32)) \
		visitOp(0x91,f32_sqrt,"f32.sqrt",NoImm,UNARY(f32,f32)) \
		\
		visitOp(0x92,f32_add,"f32.add",NoImm,BINARY(f32,f32)) \
		visitOp(0x93,f32_sub,"f32.sub",NoImm,BINARY(f32,f32)) \
		visitOp(0x94,f32_mul,"f32.mul",NoImm,BINARY(f32,f32)) \
		visitOp(0x95,f32_div,"f32.div",NoImm,BINARY(f32,f32)) \
		visitOp(0x96,f32_min,"f32.min",NoImm,BINARY(f32,f32)) \
		visitOp(0x97,f32_max,"f32.max",NoImm,BINARY(f32,f32)) \
		visitOp(0x98,f32_copysign,"f32.copysign",NoImm,BINARY(f32,f32)) \
		\
		visitOp(0x99,f64_abs,"f64.abs",NoImm,UNARY(f64,f64)) \
		visitOp(0x9a,f64_neg,"f64.neg",NoImm,UNARY(f64,f64)) \
		visitOp(0x9b,f64_ceil,"f64.ceil",NoImm,UNARY(f64,f64)) \
		visitOp(0x9c,f64_floor,"f64.floor",NoImm,UNARY(f64,f64)) \
		visitOp(0x9d,f64_trunc,"f64.trunc",NoImm,UNARY(f64,f64)) \
		visitOp(0x9e,f64_nearest,"f64.nearest",NoImm,UNARY(f64,f64)) \
		visitOp(0x9f,f64_sqrt,"f64.sqrt",NoImm,UNARY(f64,f64)) \
		\
		visitOp(0xa0,f64_add,"f64.add",NoImm,BINARY(f64,f64)) \
		visitOp(0xa1,f64_sub,"f64.sub",NoImm,BINARY(f64,f64)) \
		visitOp(0xa2,f64_mul,"f64.mul",NoImm,BINARY(f64,f64)) \
		visitOp(0xa3,f64_div,"f64.div",NoImm,BINARY(f64,f64)) \
		visitOp(0xa4,f64_min,"f64.min",NoImm,BINARY(f64,f64)) \
		visitOp(0xa5,f64_max,"f64.max",NoImm,BINARY(f64,f64)) \
		visitOp(0xa6,f64_copysign,"f64.copysign",NoImm,BINARY(f64,f64)) \
		\
		visitOp(0xa7,i32_wrap_i64,"i32.wrap/i64",NoImm,UNARY(i64,i32)) \
		visitOp(0xa8,i32_trunc_s_f32,"i32.trunc_s/f32",NoImm,UNARY(f32,i32)) \
		visitOp(0xa9,i32_trunc_u_f32,"i32.trunc_u/f32",NoImm,UNARY(f32,i32)) \
		visitOp(0xaa,i32_trunc_s_f64,"i32.trunc_s/f64",NoImm,UNARY(f64,i32)) \
		visitOp(0xab,i32_trunc_u_f64,"i32.trunc_u/f64",NoImm,UNARY(f64,i32)) \
		visitOp(0xac,i64_extend_s_i32,"i64.extend_s/i32",NoImm,UNARY(i32,i64)) \
		visitOp(0xad,i64_extend_u_i32,"i64.extend_u/i32",NoImm,UNARY(i32,i64)) \
		visitOp(0xae,i64_trunc_s_f32,"i64.trunc_s/f32",NoImm,UNARY(f32,i64)) \
		visitOp(0xaf,i64_trunc_u_f32,"i64.trunc_u/f32",NoImm,UNARY(f32,i64)) \
		visitOp(0xb0,i64_trunc_s_f64,"i64.trunc_s/f64",NoImm,UNARY(f64,i64)) \
		visitOp(0xb1,i64_trunc_u_f64,"i64.trunc_u/f64",NoImm,UNARY(f64,i64)) \
		visitOp(0xb2,f32_convert_s_i32,"f32.convert_s/i32",NoImm,UNARY(i32,f32)) \
		visitOp(0xb3,f32_convert_u_i32,"f32.convert_u/i32",NoImm,UNARY(i32,f32)) \
		visitOp(0xb4,f32_convert_s_i64,"f32.convert_s/i64",NoImm,UNARY(i64,f32)) \
		visitOp(0xb5,f32_convert_u_i64,"f32.convert_u/i64",NoImm,UNARY(i64,f32)) \
		visitOp(0xb6,f32_demote_f64,"f32.demote/f64",NoImm,UNARY(f64,f32)) \
		visitOp(0xb7,f64_convert_s_i32,"f64.convert_s/i32",NoImm,UNARY(i32,f64)) \
		visitOp(0xb8,f64_convert_u_i32,"f64.convert_u/i32",NoImm,UNARY(i32,f64)) \
		visitOp(0xb9,f64_convert_s_i64,"f64.convert_s/i64",NoImm,UNARY(i64,f64)) \
		visitOp(0xba,f64_convert_u_i64,"f64.convert_u/i64",NoImm,UNARY(i64,f64)) \
		visitOp(0xbb,f64_promote_f32,"f64.promote/f32",NoImm,UNARY(f32,f64)) \
		visitOp(0xbc,i32_reinterpret_f32,"i32.reinterpret/f32",NoImm,UNARY(f32,i32)) \
		visitOp(0xbd,i64_reinterpret_f64,"i64.reinterpret/f64",NoImm,UNARY(f64,i64)) \
		visitOp(0xbe,f32_reinterpret_i32,"f32.reinterpret/i32",NoImm,UNARY(i32,f32)) \
		visitOp(0xbf,f64_reinterpret_i64,"f64.reinterpret/i64",NoImm,UNARY(i64,f64))

	#define ENUM_NONCONTROL_OPERATORS(visitOp) \
		ENUM_PARAMETRIC_OPERATORS(visitOp) \
		ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(visitOp)

	#define ENUM_OPERATORS(visitOp) \
		ENUM_NONCONTROL_OPERATORS(visitOp) \
		ENUM_CONTROL_OPERATORS(visitOp)

	enum class Opcode : uint16
	{
		#define VISIT_OPCODE(opcode,name,...) name = opcode,
		ENUM_OPERATORS(VISIT_OPCODE)
		#undef VISIT_OPCODE

		maxSingleByteOpcode = 0xcf,
	};

	inline void serialize(InputStream& stream,Opcode& opcode)
	{
		opcode = (Opcode)0;
		Serialization::serializeNativeValue(stream,*(uint8*)&opcode);
		if(opcode > Opcode::maxSingleByteOpcode)
		{
			opcode = (Opcode)(uint16(opcode) << 8);
			Serialization::serializeNativeValue(stream,*(uint8*)&opcode);
		}
	}
	inline void serialize(OutputStream& stream,Opcode opcode)
	{
		if(opcode <= Opcode::maxSingleByteOpcode) { Serialization::serializeNativeValue(stream,*(uint8*)&opcode); }
		else
		{
			serializeNativeValue(stream,*(((uint8*)&opcode) + 1));
			serializeNativeValue(stream,*(((uint8*)&opcode) + 0));
		}
	}

	// Structures for operator immediates

	struct NoImm
	{
		template<typename Stream>
		friend void serialize(Stream& stream,NoImm&) {}
	};

	struct ControlStructureImm
	{
		ResultType resultType;

		template<typename Stream>
		friend void serialize(Stream& stream,ControlStructureImm& imm)
		{
			int8 encodedResultType = imm.resultType == ResultType::none ? -64 : -(int8)imm.resultType;
			serializeVarInt7(stream,encodedResultType);
			if(Stream::isInput) { imm.resultType = encodedResultType == -64 ? ResultType::none : (ResultType)-encodedResultType; }
		}
	};

	struct BranchImm
	{
		uintp targetDepth;

		template<typename Stream>
		friend void serialize(Stream& stream,BranchImm& imm)
		{
			serializeVarUInt32(stream,imm.targetDepth);
		}
	};

	struct BranchTableImm
	{
		std::vector<uintp> targetDepths;
		uintp defaultTargetDepth;

		template<typename Stream>
		friend void serialize(Stream& stream,BranchTableImm& imm)
		{
			serializeArray(stream,imm.targetDepths,[](Stream& stream,uintp& targetDepth){serializeVarUInt32(stream,targetDepth);});
			serializeVarUInt32(stream,imm.defaultTargetDepth);
		}
	};

	template<typename Value>
	struct LiteralImm
	{
		Value value;
	};

	template<typename Stream,typename Value>
	void serialize(Stream& stream,LiteralImm<Value>& imm)
	{ serialize(stream,imm.value); }
		
	template<typename Stream>
	void serialize(Stream& stream,LiteralImm<int32>& imm)
	{ serializeVarInt32(stream,imm.value); }
	
	template<typename Stream>
	void serialize(Stream& stream,LiteralImm<int64>& imm)
	{ serializeVarInt64(stream,imm.value); }

	struct GetOrSetVariableImm
	{
		uintp variableIndex;

		template<typename Stream>
		friend void serialize(Stream& stream,GetOrSetVariableImm& imm)
		{ serializeVarUInt32(stream,imm.variableIndex); }
	};

	struct CallImm
	{
		uintp functionIndex;

		template<typename Stream>
		friend void serialize(Stream& stream,CallImm& imm)
		{
			serializeVarUInt32(stream,imm.functionIndex);
		}
	};

	struct CallIndirectImm
	{
		IndexedFunctionType type;

		template<typename Stream>
		friend void serialize(Stream& stream,CallIndirectImm& imm)
		{
			serializeVarUInt32(stream,imm.type.index);

			uint8 reserved = 0;
			serializeVarUInt1(stream,reserved);
		}
	};

	template<size_t naturalAlignmentLog2>
	struct LoadOrStoreImm
	{
		uint32 alignmentLog2;
		uint32 offset;

		template<typename Stream>
		friend void serialize(Stream& stream,LoadOrStoreImm& imm)
		{
			serializeVarUInt32(stream,imm.alignmentLog2);
			serializeVarUInt32(stream,imm.offset);
		}
	};

	struct MemoryImm
	{
		template<typename Stream>
		friend void serialize(Stream& stream,MemoryImm& imm)
		{
			uint8 reserved = 0;
			serializeVarUInt1(stream,reserved);
		}
	};

	// Decodes an operator from an input stream and dispatches by opcode.
	struct OperationDecoder
	{
		OperationDecoder(Serialization::InputStream& inStream): stream(inStream) {}

		operator bool() const { return stream.capacity() != 0; }

		template<typename Visitor>
		void decodeOp(Visitor& visitor)
		{
			Opcode opcode;
			serialize(stream,opcode);
			switch(opcode)
			{
			#define VISIT_OPCODE(opcode,name,nameString,Imm,...) \
				case Opcode::name: \
				{ \
					Imm imm; \
					serialize(stream,imm); \
					return visitor.name(imm); \
				}
			ENUM_OPERATORS(VISIT_OPCODE)
			#undef VISIT_OPCODE
			default: return visitor.unknown(opcode);
			}
		}

	private:

		Serialization::InputStream& stream;
	};

	// Encodes an operator to an output stream.
	struct OperationEncoder
	{
		Serialization::OutputStream& stream;

		OperationEncoder(Serialization::OutputStream& inStream): stream(inStream) {}

		#define VISIT_OPCODE(_,name,nameString,Imm,...) \
			void name(Imm imm = {}) \
			{ \
				Opcode opcode = Opcode::name; \
				serialize(stream,opcode); \
				serialize(stream,imm); \
			}
		ENUM_OPERATORS(VISIT_OPCODE)
		#undef VISIT_OPCODE
	};

	IR_API const char* getOpcodeName(Opcode opcode);
}