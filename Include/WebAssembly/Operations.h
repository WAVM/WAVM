#pragma once

#include "WebAssembly.h"
#include "Core/Serialization.h"

namespace WebAssembly
{
	using namespace Serialization;

	#define ENUM_CONTROL_OPCODES(visit) \
		visit(0x00,nop,NoImmediates) \
		visit(0x01,beginBlock,ControlStructureImmediates) \
		visit(0x02,beginLoop,ControlStructureImmediates) \
		visit(0x03,beginIf,NoImmediates) \
		visit(0x04,beginIfElse,ControlStructureImmediates) \
		visit(0x05,select,NoImmediates) \
		visit(0x06,br,BranchImmediates) \
		visit(0x07,br_if,BranchImmediates) \
		visit(0x08,br_table,BranchTableImmediates) \
		visit(0x09,ret,NoImmediates) \
		visit(0x0a,unreachable,NoImmediates) \
		visit(0x0b,drop,NoImmediates) \
		visit(0x0f,end,NoImmediates) \
		visit(0x16,call,CallImmediates) \
		visit(0x17,call_indirect,CallIndirectImmediates)

	#define ENUM_LOAD_OPCODES(visit) \
		visit(0x20,i32_load8_s,LoadOrStoreImmediates) \
		visit(0x21,i32_load8_u,LoadOrStoreImmediates) \
		visit(0x22,i32_load16_s,LoadOrStoreImmediates) \
		visit(0x23,i32_load16_u,LoadOrStoreImmediates) \
		visit(0x24,i64_load8_s,LoadOrStoreImmediates) \
		visit(0x25,i64_load8_u,LoadOrStoreImmediates) \
		visit(0x26,i64_load16_s,LoadOrStoreImmediates) \
		visit(0x27,i64_load16_u,LoadOrStoreImmediates) \
		visit(0x28,i64_load32_s,LoadOrStoreImmediates) \
		visit(0x29,i64_load32_u,LoadOrStoreImmediates) \
		visit(0x2a,i32_load,LoadOrStoreImmediates) \
		visit(0x2b,i64_load,LoadOrStoreImmediates) \
		visit(0x2c,f32_load,LoadOrStoreImmediates) \
		visit(0x2d,f64_load,LoadOrStoreImmediates)

	#define ENUM_STORE_OPCODES(visit) \
		visit(0x2e,i32_store8,LoadOrStoreImmediates) \
		visit(0x2f,i32_store16,LoadOrStoreImmediates) \
		visit(0x30,i64_store8,LoadOrStoreImmediates) \
		visit(0x31,i64_store16,LoadOrStoreImmediates) \
		visit(0x32,i64_store32,LoadOrStoreImmediates) \
		visit(0x33,i32_store,LoadOrStoreImmediates) \
		visit(0x34,i64_store,LoadOrStoreImmediates) \
		visit(0x35,f32_store,LoadOrStoreImmediates) \
		visit(0x36,f64_store,LoadOrStoreImmediates)

	#define ENUM_LITERAL_OPCODES(visit) \
		visit(0x10,i32_const,LiteralImmediates<int32>) \
		visit(0x11,i64_const,LiteralImmediates<int64>) \
		visit(0x12,f64_const,LiteralImmediates<float64>) \
		visit(0x13,f32_const,LiteralImmediates<float32>)

	#define ENUM_VARIABLE_OPCODES(visit) \
		visit(0x14,get_local,GetOrSetVariableImmediates) \
		visit(0x15,set_local,GetOrSetVariableImmediates) \
		visit(0x19,tee_local,GetOrSetVariableImmediates) \
		visit(0x1a,get_global,GetOrSetVariableImmediates) \
		visit(0x1b,set_global,GetOrSetVariableImmediates)

	#define ENUM_I32_BINARY_OPCODES(visit) \
		visit(0x40,i32_add,NoImmediates) \
		visit(0x41,i32_sub,NoImmediates) \
		visit(0x42,i32_mul,NoImmediates) \
		visit(0x43,i32_div_s,NoImmediates) \
		visit(0x44,i32_div_u,NoImmediates) \
		visit(0x45,i32_rem_s,NoImmediates) \
		visit(0x46,i32_rem_u,NoImmediates) \
		visit(0x47,i32_and,NoImmediates) \
		visit(0x48,i32_or,NoImmediates) \
		visit(0x49,i32_xor,NoImmediates) \
		visit(0x4a,i32_shl,NoImmediates) \
		visit(0x4b,i32_shr_u,NoImmediates) \
		visit(0x4c,i32_shr_s,NoImmediates) \
		visit(0xb6,i32_rotr,NoImmediates) \
		visit(0xb7,i32_rotl,NoImmediates) \

	#define ENUM_I32_COMPARE_OPCODES(visit) \
		visit(0x4d,i32_eq,NoImmediates) \
		visit(0x4e,i32_ne,NoImmediates) \
		visit(0x4f,i32_lt_s,NoImmediates) \
		visit(0x50,i32_le_s,NoImmediates) \
		visit(0x51,i32_lt_u,NoImmediates) \
		visit(0x52,i32_le_u,NoImmediates) \
		visit(0x53,i32_gt_s,NoImmediates) \
		visit(0x54,i32_ge_s,NoImmediates) \
		visit(0x55,i32_gt_u,NoImmediates) \
		visit(0x56,i32_ge_u,NoImmediates)

	#define ENUM_I32_UNARY_OPCODES(visit) \
		visit(0x57,i32_clz,NoImmediates) \
		visit(0x58,i32_ctz,NoImmediates) \
		visit(0x59,i32_popcnt,NoImmediates)

	#define ENUM_I64_BINARY_OPCODES(visit) \
		visit(0x5b,i64_add,NoImmediates) \
		visit(0x5c,i64_sub,NoImmediates) \
		visit(0x5d,i64_mul,NoImmediates) \
		visit(0x5e,i64_div_s,NoImmediates) \
		visit(0x5f,i64_div_u,NoImmediates) \
		visit(0x60,i64_rem_s,NoImmediates) \
		visit(0x61,i64_rem_u,NoImmediates) \
		visit(0x62,i64_and,NoImmediates) \
		visit(0x63,i64_or,NoImmediates) \
		visit(0x64,i64_xor,NoImmediates) \
		visit(0x65,i64_shl,NoImmediates) \
		visit(0x66,i64_shr_u,NoImmediates) \
		visit(0x67,i64_shr_s,NoImmediates) \
		visit(0xb8,i64_rotr,NoImmediates) \
		visit(0xb9,i64_rotl,NoImmediates)

	#define ENUM_I64_COMPARE_OPCODES(visit) \
		visit(0x68,i64_eq,NoImmediates) \
		visit(0x69,i64_ne,NoImmediates) \
		visit(0x6a,i64_lt_s,NoImmediates) \
		visit(0x6b,i64_le_s,NoImmediates) \
		visit(0x6c,i64_lt_u,NoImmediates) \
		visit(0x6d,i64_le_u,NoImmediates) \
		visit(0x6e,i64_gt_s,NoImmediates) \
		visit(0x6f,i64_ge_s,NoImmediates) \
		visit(0x70,i64_gt_u,NoImmediates) \
		visit(0x71,i64_ge_u,NoImmediates)

	#define ENUM_I64_UNARY_OPCODES(visit) \
		visit(0x72,i64_clz,NoImmediates) \
		visit(0x73,i64_ctz,NoImmediates) \
		visit(0x74,i64_popcnt,NoImmediates)

	#define ENUM_F32_BINARY_OPCODES(visit) \
		visit(0x75,f32_add,NoImmediates) \
		visit(0x76,f32_sub,NoImmediates) \
		visit(0x77,f32_mul,NoImmediates) \
		visit(0x78,f32_div,NoImmediates) \
		visit(0x79,f32_min,NoImmediates) \
		visit(0x7a,f32_max,NoImmediates)

	#define ENUM_F32_UNARY_OPCODES(visit) \
		visit(0x7b,f32_abs,NoImmediates) \
		visit(0x7c,f32_neg,NoImmediates) \
		visit(0x7d,f32_copysign,NoImmediates) \
		visit(0x7e,f32_ceil,NoImmediates) \
		visit(0x7f,f32_floor,NoImmediates) \
		visit(0x80,f32_trunc,NoImmediates) \
		visit(0x81,f32_nearest,NoImmediates) \
		visit(0x82,f32_sqrt,NoImmediates)

	#define ENUM_F32_COMPARE_OPCODES(visit) \
		visit(0x83,f32_eq,NoImmediates) \
		visit(0x84,f32_ne,NoImmediates) \
		visit(0x85,f32_lt,NoImmediates) \
		visit(0x86,f32_le,NoImmediates) \
		visit(0x87,f32_gt,NoImmediates) \
		visit(0x88,f32_ge,NoImmediates)

	#define ENUM_F64_BINARY_OPCODES(visit) \
		visit(0x89,f64_add,NoImmediates) \
		visit(0x8a,f64_sub,NoImmediates) \
		visit(0x8b,f64_mul,NoImmediates) \
		visit(0x8c,f64_div,NoImmediates) \
		visit(0x8d,f64_min,NoImmediates) \
		visit(0x8e,f64_max,NoImmediates)

	#define ENUM_F64_UNARY_OPCODES(visit) \
		visit(0x8f,f64_abs,NoImmediates) \
		visit(0x90,f64_neg,NoImmediates) \
		visit(0x91,f64_copysign,NoImmediates) \
		visit(0x92,f64_ceil,NoImmediates) \
		visit(0x93,f64_floor,NoImmediates) \
		visit(0x94,f64_trunc,NoImmediates) \
		visit(0x95,f64_nearest,NoImmediates) \
		visit(0x96,f64_sqrt,NoImmediates)

	#define ENUM_F64_COMPARE_OPCODES(visit) \
		visit(0x97,f64_eq,NoImmediates) \
		visit(0x98,f64_ne,NoImmediates) \
		visit(0x99,f64_lt,NoImmediates) \
		visit(0x9a,f64_le,NoImmediates) \
		visit(0x9b,f64_gt,NoImmediates) \
		visit(0x9c,f64_ge,NoImmediates)

	#define ENUM_CONVERSION_OPCODES(visit) \
		visit(0x9d,i32_trunc_s_f32,NoImmediates) \
		visit(0x9e,i32_trunc_s_f64,NoImmediates) \
		visit(0x9f,i32_trunc_u_f32,NoImmediates) \
		visit(0xa0,i32_trunc_u_f64,NoImmediates) \
		visit(0xa1,i32_wrap_i64,NoImmediates) \
		visit(0xa2,i64_trunc_s_f32,NoImmediates) \
		visit(0xa3,i64_trunc_s_f64,NoImmediates) \
		visit(0xa4,i64_trunc_u_f32,NoImmediates) \
		visit(0xa5,i64_trunc_u_f64,NoImmediates) \
		visit(0xa6,i64_extend_s_i32,NoImmediates) \
		visit(0xa7,i64_extend_u_i32,NoImmediates) \
		visit(0xa8,f32_convert_s_i32,NoImmediates) \
		visit(0xa9,f32_convert_u_i32,NoImmediates) \
		visit(0xaa,f32_convert_s_i64,NoImmediates) \
		visit(0xab,f32_convert_u_i64,NoImmediates) \
		visit(0xac,f32_demote_f64,NoImmediates) \
		visit(0xad,f32_reinterpret_i32,NoImmediates) \
		visit(0xae,f64_convert_s_i32,NoImmediates) \
		visit(0xaf,f64_convert_u_i32,NoImmediates) \
		visit(0xb0,f64_convert_s_i64,NoImmediates) \
		visit(0xb1,f64_convert_u_i64,NoImmediates) \
		visit(0xb2,f64_promote_f32,NoImmediates) \
		visit(0xb3,f64_reinterpret_i64,NoImmediates) \
		visit(0xb4,i32_reinterpret_f32,NoImmediates) \
		visit(0xb5,i64_reinterpret_f64,NoImmediates)

	#define ENUM_MISC_OPCODES(visit) \
		visit(0x5a,i32_eqz,NoImmediates) \
		visit(0xba,i64_eqz,NoImmediates) \
		visit(0x39,grow_memory,NoImmediates) \
		visit(0x3b,current_memory,NoImmediates) \
		visit(0xff,error,ErrorImmediates)

	#define ENUM_OPCODES(visit) \
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

	enum class Opcode : uint8
	{
		#define VISIT_OPCODE(encoding,name,immediates) name = encoding,
		ENUM_OPCODES(VISIT_OPCODE)
		#undef VISIT_OPCODE
	};

	template<typename Stream>
	void serialize(Stream& stream,Opcode& opcode) { return Serialization::serializeNativeValue(stream,*(uint8*)&opcode); }

	struct NoImmediates
	{
		template<typename Stream>
		friend void serialize(Stream& stream,NoImmediates&) {}
	};

	struct ControlStructureImmediates
	{
		uintptr signatureTypeIndex;
		
		template<typename Stream>
		friend void serialize(Stream& stream,ControlStructureImmediates& controlStructureOp)
		{
			serializeVarUInt32(stream,controlStructureOp.signatureTypeIndex);
		}
	};

	struct BranchImmediates
	{
		uintptr targetDepth;

		template<typename Stream>
		friend void serialize(Stream& stream,BranchImmediates& branchOp)
		{
			serializeVarUInt32(stream,branchOp.targetDepth);
		}
	};

	struct BranchTableImmediates
	{
		uintptr defaultTargetDepth;
		std::vector<uintptr> targetDepths;
		
		template<typename Stream>
		friend void serialize(Stream& stream,BranchTableImmediates& branchTableOp)
		{
			serializeVarUInt32(stream,branchTableOp.defaultTargetDepth);
			serializeArray(stream,branchTableOp.targetDepths,[](Stream& stream,uintptr& targetDepth){serializeVarUInt32(stream,targetDepth);});
		}
	};

	template<typename Value>
	struct LiteralImmediates
	{
		Value value;
	};
	
	template<typename Stream,typename Value>
	void serialize(Stream& stream,LiteralImmediates<Value>& literalOp)
	{ serialize(stream,literalOp.value); }
		
	template<typename Stream>
	void serialize(Stream& stream,LiteralImmediates<int32>& literalOp)
	{ serializeVarInt32(stream,literalOp.value); }
	
	template<typename Stream>
	void serialize(Stream& stream,LiteralImmediates<int64>& literalOp)
	{ serializeVarInt64(stream,literalOp.value); }

	struct GetOrSetVariableImmediates
	{
		uintptr variableIndex;
		
		template<typename Stream>
		friend void serialize(Stream& stream,GetOrSetVariableImmediates& getOrSetLocalOp)
		{ serializeVarUInt32(stream,getOrSetLocalOp.variableIndex); }		
	};

	struct CallImmediates
	{
		uintptr functionIndex;

		template<typename Stream>
		friend void serialize(Stream& stream,CallImmediates& callOp)
		{
			serializeVarUInt32(stream,callOp.functionIndex);
		}
	};

	struct CallIndirectImmediates
	{
		uintptr typeIndex;
		
		template<typename Stream>
		friend void serialize(Stream& stream,CallIndirectImmediates& callIndirectOp)
		{
			serializeVarUInt32(stream,callIndirectOp.typeIndex);
		}
	};

	struct LoadOrStoreImmediates
	{
		uint32 offset;
		uint8 alignmentLog2;
		
		template<typename Stream>
		friend void serialize(Stream& stream,LoadOrStoreImmediates& loadOrStoreOp)
		{
			serializeVarUInt7(stream,loadOrStoreOp.alignmentLog2);
			serializeVarUInt32(stream,loadOrStoreOp.offset);
		}
	};

	struct ErrorImmediates
	{
		std::string message;
		
		template<typename Stream>
		friend void serialize(Stream& stream,ErrorImmediates& errorOp)
		{ serialize(stream,errorOp.message); }
	};

	struct OperationDecoder
	{
		OperationDecoder(const Serialization::InputStream& inStream): stream(inStream) {}

		operator bool() const { return stream.capacity() > 0; }

		template<typename Visitor>
		void decodeOp(Visitor& visitor)
		{
			Opcode opcode;
			serializeNativeValue(stream,opcode);
			switch(opcode)
			{
			#define VISIT_OPCODE(encoding,name,Immediates) \
				case Opcode::name: \
				{ \
					Immediates immediates; \
					serialize(stream,immediates); \
					return visitor.name(immediates); \
				}
			ENUM_OPCODES(VISIT_OPCODE)
			#undef VISIT_OPCODE
			default: return visitor.unknown(opcode);
			}
		}

	private:

		Serialization::InputStream stream;
	};

	struct OperationEncoder
	{
		Serialization::OutputStream& stream;
		uintptr controlStackDepth;
		uintptr reachableDepth;

		OperationEncoder(Serialization::OutputStream& inStream): stream(inStream), controlStackDepth(1), reachableDepth(1) {}

		#define VISIT_OPCODE(encoding,name,Immediates) \
			void name(Immediates immediates = {}) \
			{ \
				if(controlStackDepth == reachableDepth) \
				{ \
					Opcode opcode = Opcode::name; \
					serialize(stream,opcode); \
					serialize(stream,immediates); \
				} \
			}
		ENUM_OPCODES(VISIT_OPCODE)
		#undef VISIT_OPCODE
	};
	
	WEBASSEMBLY_API const char* getOpcodeName(Opcode opcode);
}
