#pragma once

#include "Core/Serialization.h"
#include "WebAssembly.h"
#include "Types.h"

namespace WebAssembly
{
	using namespace Serialization;

	#define ENUM_CONTROL_OPCODES(visit) \
		visit(0x00,nop,NoImm) \
		visit(0x01,beginBlock,ControlStructureImm) \
		visit(0x02,beginLoop,ControlStructureImm) \
		visit(0x03,beginIf,ControlStructureImm) \
		visit(0x05,select,NoImm) \
		visit(0x06,br,BranchImm) \
		visit(0x07,br_if,BranchImm) \
		visit(0x08,br_table,BranchTableImm) \
		visit(0x09,ret,NoImm) \
		visit(0x0a,unreachable,NoImm) \
		visit(0x0b,drop,NoImm) \
		visit(0x16,call,CallImm) \
		visit(0x17,call_indirect,CallIndirectImm)

	#define ENUM_LOAD_OPCODES(visit) \
		visit(0x20,i32_load8_s,LoadOrStoreImm) \
		visit(0x21,i32_load8_u,LoadOrStoreImm) \
		visit(0x22,i32_load16_s,LoadOrStoreImm) \
		visit(0x23,i32_load16_u,LoadOrStoreImm) \
		visit(0x24,i64_load8_s,LoadOrStoreImm) \
		visit(0x25,i64_load8_u,LoadOrStoreImm) \
		visit(0x26,i64_load16_s,LoadOrStoreImm) \
		visit(0x27,i64_load16_u,LoadOrStoreImm) \
		visit(0x28,i64_load32_s,LoadOrStoreImm) \
		visit(0x29,i64_load32_u,LoadOrStoreImm) \
		visit(0x2a,i32_load,LoadOrStoreImm) \
		visit(0x2b,i64_load,LoadOrStoreImm) \
		visit(0x2c,f32_load,LoadOrStoreImm) \
		visit(0x2d,f64_load,LoadOrStoreImm)

	#define ENUM_STORE_OPCODES(visit) \
		visit(0x2e,i32_store8,LoadOrStoreImm) \
		visit(0x2f,i32_store16,LoadOrStoreImm) \
		visit(0x30,i64_store8,LoadOrStoreImm) \
		visit(0x31,i64_store16,LoadOrStoreImm) \
		visit(0x32,i64_store32,LoadOrStoreImm) \
		visit(0x33,i32_store,LoadOrStoreImm) \
		visit(0x34,i64_store,LoadOrStoreImm) \
		visit(0x35,f32_store,LoadOrStoreImm) \
		visit(0x36,f64_store,LoadOrStoreImm)

	#define ENUM_LITERAL_OPCODES(visit) \
		visit(0x10,i32_const,LiteralImm<int32>) \
		visit(0x11,i64_const,LiteralImm<int64>) \
		visit(0x12,f64_const,LiteralImm<float64>) \
		visit(0x13,f32_const,LiteralImm<float32>)

	#define ENUM_VARIABLE_OPCODES(visit) \
		visit(0x14,get_local,GetOrSetVariableImm) \
		visit(0x15,set_local,GetOrSetVariableImm) \
		visit(0x19,tee_local,GetOrSetVariableImm) \
		visit(0x1a,get_global,GetOrSetVariableImm) \
		visit(0x1b,set_global,GetOrSetVariableImm)

	#define ENUM_I32_BINARY_OPCODES(visit) \
		visit(0x40,i32_add,NoImm) \
		visit(0x41,i32_sub,NoImm) \
		visit(0x42,i32_mul,NoImm) \
		visit(0x43,i32_div_s,NoImm) \
		visit(0x44,i32_div_u,NoImm) \
		visit(0x45,i32_rem_s,NoImm) \
		visit(0x46,i32_rem_u,NoImm) \
		visit(0x47,i32_and,NoImm) \
		visit(0x48,i32_or,NoImm) \
		visit(0x49,i32_xor,NoImm) \
		visit(0x4a,i32_shl,NoImm) \
		visit(0x4b,i32_shr_u,NoImm) \
		visit(0x4c,i32_shr_s,NoImm) \
		visit(0xb6,i32_rotr,NoImm) \
		visit(0xb7,i32_rotl,NoImm) \

	#define ENUM_I32_COMPARE_OPCODES(visit) \
		visit(0x4d,i32_eq,NoImm) \
		visit(0x4e,i32_ne,NoImm) \
		visit(0x4f,i32_lt_s,NoImm) \
		visit(0x50,i32_le_s,NoImm) \
		visit(0x51,i32_lt_u,NoImm) \
		visit(0x52,i32_le_u,NoImm) \
		visit(0x53,i32_gt_s,NoImm) \
		visit(0x54,i32_ge_s,NoImm) \
		visit(0x55,i32_gt_u,NoImm) \
		visit(0x56,i32_ge_u,NoImm)

	#define ENUM_I32_UNARY_OPCODES(visit) \
		visit(0x57,i32_clz,NoImm) \
		visit(0x58,i32_ctz,NoImm) \
		visit(0x59,i32_popcnt,NoImm)

	#define ENUM_I64_BINARY_OPCODES(visit) \
		visit(0x5b,i64_add,NoImm) \
		visit(0x5c,i64_sub,NoImm) \
		visit(0x5d,i64_mul,NoImm) \
		visit(0x5e,i64_div_s,NoImm) \
		visit(0x5f,i64_div_u,NoImm) \
		visit(0x60,i64_rem_s,NoImm) \
		visit(0x61,i64_rem_u,NoImm) \
		visit(0x62,i64_and,NoImm) \
		visit(0x63,i64_or,NoImm) \
		visit(0x64,i64_xor,NoImm) \
		visit(0x65,i64_shl,NoImm) \
		visit(0x66,i64_shr_u,NoImm) \
		visit(0x67,i64_shr_s,NoImm) \
		visit(0xb8,i64_rotr,NoImm) \
		visit(0xb9,i64_rotl,NoImm)

	#define ENUM_I64_COMPARE_OPCODES(visit) \
		visit(0x68,i64_eq,NoImm) \
		visit(0x69,i64_ne,NoImm) \
		visit(0x6a,i64_lt_s,NoImm) \
		visit(0x6b,i64_le_s,NoImm) \
		visit(0x6c,i64_lt_u,NoImm) \
		visit(0x6d,i64_le_u,NoImm) \
		visit(0x6e,i64_gt_s,NoImm) \
		visit(0x6f,i64_ge_s,NoImm) \
		visit(0x70,i64_gt_u,NoImm) \
		visit(0x71,i64_ge_u,NoImm)

	#define ENUM_I64_UNARY_OPCODES(visit) \
		visit(0x72,i64_clz,NoImm) \
		visit(0x73,i64_ctz,NoImm) \
		visit(0x74,i64_popcnt,NoImm)

	#define ENUM_F32_BINARY_OPCODES(visit) \
		visit(0x75,f32_add,NoImm) \
		visit(0x76,f32_sub,NoImm) \
		visit(0x77,f32_mul,NoImm) \
		visit(0x78,f32_div,NoImm) \
		visit(0x79,f32_min,NoImm) \
		visit(0x7a,f32_max,NoImm)

	#define ENUM_F32_UNARY_OPCODES(visit) \
		visit(0x7b,f32_abs,NoImm) \
		visit(0x7c,f32_neg,NoImm) \
		visit(0x7d,f32_copysign,NoImm) \
		visit(0x7e,f32_ceil,NoImm) \
		visit(0x7f,f32_floor,NoImm) \
		visit(0x80,f32_trunc,NoImm) \
		visit(0x81,f32_nearest,NoImm) \
		visit(0x82,f32_sqrt,NoImm)

	#define ENUM_F32_COMPARE_OPCODES(visit) \
		visit(0x83,f32_eq,NoImm) \
		visit(0x84,f32_ne,NoImm) \
		visit(0x85,f32_lt,NoImm) \
		visit(0x86,f32_le,NoImm) \
		visit(0x87,f32_gt,NoImm) \
		visit(0x88,f32_ge,NoImm)

	#define ENUM_F64_BINARY_OPCODES(visit) \
		visit(0x89,f64_add,NoImm) \
		visit(0x8a,f64_sub,NoImm) \
		visit(0x8b,f64_mul,NoImm) \
		visit(0x8c,f64_div,NoImm) \
		visit(0x8d,f64_min,NoImm) \
		visit(0x8e,f64_max,NoImm)

	#define ENUM_F64_UNARY_OPCODES(visit) \
		visit(0x8f,f64_abs,NoImm) \
		visit(0x90,f64_neg,NoImm) \
		visit(0x91,f64_copysign,NoImm) \
		visit(0x92,f64_ceil,NoImm) \
		visit(0x93,f64_floor,NoImm) \
		visit(0x94,f64_trunc,NoImm) \
		visit(0x95,f64_nearest,NoImm) \
		visit(0x96,f64_sqrt,NoImm)

	#define ENUM_F64_COMPARE_OPCODES(visit) \
		visit(0x97,f64_eq,NoImm) \
		visit(0x98,f64_ne,NoImm) \
		visit(0x99,f64_lt,NoImm) \
		visit(0x9a,f64_le,NoImm) \
		visit(0x9b,f64_gt,NoImm) \
		visit(0x9c,f64_ge,NoImm)

	#define ENUM_CONVERSION_OPCODES(visit) \
		visit(0x9d,i32_trunc_s_f32,NoImm) \
		visit(0x9e,i32_trunc_s_f64,NoImm) \
		visit(0x9f,i32_trunc_u_f32,NoImm) \
		visit(0xa0,i32_trunc_u_f64,NoImm) \
		visit(0xa1,i32_wrap_i64,NoImm) \
		visit(0xa2,i64_trunc_s_f32,NoImm) \
		visit(0xa3,i64_trunc_s_f64,NoImm) \
		visit(0xa4,i64_trunc_u_f32,NoImm) \
		visit(0xa5,i64_trunc_u_f64,NoImm) \
		visit(0xa6,i64_extend_s_i32,NoImm) \
		visit(0xa7,i64_extend_u_i32,NoImm) \
		visit(0xa8,f32_convert_s_i32,NoImm) \
		visit(0xa9,f32_convert_u_i32,NoImm) \
		visit(0xaa,f32_convert_s_i64,NoImm) \
		visit(0xab,f32_convert_u_i64,NoImm) \
		visit(0xac,f32_demote_f64,NoImm) \
		visit(0xad,f32_reinterpret_i32,NoImm) \
		visit(0xae,f64_convert_s_i32,NoImm) \
		visit(0xaf,f64_convert_u_i32,NoImm) \
		visit(0xb0,f64_convert_s_i64,NoImm) \
		visit(0xb1,f64_convert_u_i64,NoImm) \
		visit(0xb2,f64_promote_f32,NoImm) \
		visit(0xb3,f64_reinterpret_i64,NoImm) \
		visit(0xb4,i32_reinterpret_f32,NoImm) \
		visit(0xb5,i64_reinterpret_f64,NoImm)

	#define ENUM_MISC_OPCODES(visit) \
		visit(0x5a,i32_eqz,NoImm) \
		visit(0xba,i64_eqz,NoImm) \
		visit(0x39,grow_memory,NoImm) \
		visit(0x3b,current_memory,NoImm) \
		visit(0xff,error,ErrorImm)

	#define ENUM_NONEND_OPCODES(visit) \
		ENUM_CONTROL_OPCODES(visit) \
		ENUM_LOAD_OPCODES(visit) ENUM_STORE_OPCODES(visit) \
		ENUM_LITERAL_OPCODES(visit) \
		ENUM_VARIABLE_OPCODES(visit) \
		ENUM_I32_BINARY_OPCODES(visit) ENUM_I32_UNARY_OPCODES(visit) ENUM_I32_COMPARE_OPCODES(visit) \
		ENUM_I64_BINARY_OPCODES(visit) ENUM_I64_UNARY_OPCODES(visit) ENUM_I64_COMPARE_OPCODES(visit) \
		ENUM_F32_BINARY_OPCODES(visit) ENUM_F32_UNARY_OPCODES(visit) ENUM_F32_COMPARE_OPCODES(visit) \
		ENUM_F64_BINARY_OPCODES(visit) ENUM_F64_UNARY_OPCODES(visit) ENUM_F64_COMPARE_OPCODES(visit) \
		ENUM_CONVERSION_OPCODES(visit) \
		ENUM_MISC_OPCODES(visit)

	#define ENUM_OPCODES(visit) \
		ENUM_NONEND_OPCODES(visit) \
		visit(0x04,beginElse,NoImm) \
		visit(0x0f,end,NoImm)

	enum class Opcode : uint8
	{
		#define VISIT_OPCODE(encoding,name,imm) name = encoding,
		ENUM_OPCODES(VISIT_OPCODE)
		#undef VISIT_OPCODE
	};

	template<typename Stream>
	void serialize(Stream& stream,Opcode& opcode) { return Serialization::serializeNativeValue(stream,*(uint8*)&opcode); }

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
			Serialization::serializeNativeValue(stream,imm.resultType);
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
		uintp typeIndex;
		
		template<typename Stream>
		friend void serialize(Stream& stream,CallIndirectImm& imm)
		{
			serializeVarUInt32(stream,imm.typeIndex);
		}
	};

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

	struct ErrorImm
	{
		std::string message;
		
		template<typename Stream>
		friend void serialize(Stream& stream,ErrorImm& imm)
		{ serialize(stream,imm.message); }
	};

	struct OperationDecoder
	{
		OperationDecoder(Serialization::InputStream& inStream): stream(inStream) {}

		operator bool() const { return stream.capacity() != 0; }

		template<typename Visitor>
		void decodeOp(Visitor& visitor)
		{
			Opcode opcode;
			serializeNativeValue(stream,opcode);
			switch(opcode)
			{
			#define VISIT_OPCODE(encoding,name,Imm) \
				case Opcode::name: \
				{ \
					Imm imm; \
					serialize(stream,imm); \
					return visitor.name(imm); \
				}
			ENUM_OPCODES(VISIT_OPCODE)
			#undef VISIT_OPCODE
			default: return visitor.unknown(opcode);
			}
		}

	private:

		Serialization::InputStream& stream;
	};

	struct OperationEncoder
	{
		Serialization::OutputStream& stream;
		uintp unreachableDepth;

		OperationEncoder(Serialization::OutputStream& inStream): stream(inStream), unreachableDepth(0) {}

		#define VISIT_OPCODE(encoding,name,Imm) \
			void name(Imm imm = {}) \
			{ \
				if(!unreachableDepth) \
				{ \
					Opcode opcode = Opcode::name; \
					serialize(stream,opcode); \
					serialize(stream,imm); \
				} \
			}
		ENUM_OPCODES(VISIT_OPCODE)
		#undef VISIT_OPCODE
	};
	
	WEBASSEMBLY_API const char* getOpcodeName(Opcode opcode);
}
