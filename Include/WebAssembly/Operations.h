#pragma once

#include "Core/Serialization.h"
#include "WebAssembly.h"
#include "Types.h"

namespace WebAssembly
{
	using namespace Serialization;

	// Enumate the WebAssembly operators

	#define ENUM_CONTROL_OPS(visit) \
		visit(0x00,unreachable,NoImm) \
		visit(0x01,nop,NoImm) \
		visit(0x02,beginBlock,ControlStructureImm) \
		visit(0x03,beginLoop,ControlStructureImm) \
		visit(0x04,beginIf,ControlStructureImm) \
		visit(0x05,beginElse,NoImm) \
		visit(0x0b,end,NoImm) \
		visit(0x0c,br,BranchImm) \
		visit(0x0d,br_if,BranchImm) \
		visit(0x0e,br_table,BranchTableImm) \
		visit(0x0f,ret,NoImm) \
		visit(0x10,call,CallImm) \
		visit(0x11,call_indirect,CallIndirectImm) \
		visit(0x1a,drop,NoImm) \
		visit(0x1b,select,NoImm)
	
	#define ENUM_VARIABLE_OPS(visit) \
		visit(0x20,get_local,GetOrSetVariableImm) \
		visit(0x21,set_local,GetOrSetVariableImm) \
		visit(0x22,tee_local,GetOrSetVariableImm) \
		visit(0x23,get_global,GetOrSetVariableImm) \
		visit(0x24,set_global,GetOrSetVariableImm)

	#define ENUM_LOAD_OPS(visit) \
		visit(0x28,i32_load,LoadOrStoreImm) \
		visit(0x29,i64_load,LoadOrStoreImm) \
		visit(0x2a,f32_load,LoadOrStoreImm) \
		visit(0x2b,f64_load,LoadOrStoreImm) \
		visit(0x2c,i32_load8_s,LoadOrStoreImm) \
		visit(0x2d,i32_load8_u,LoadOrStoreImm) \
		visit(0x2e,i32_load16_s,LoadOrStoreImm) \
		visit(0x2f,i32_load16_u,LoadOrStoreImm) \
		visit(0x30,i64_load8_s,LoadOrStoreImm) \
		visit(0x31,i64_load8_u,LoadOrStoreImm) \
		visit(0x32,i64_load16_s,LoadOrStoreImm) \
		visit(0x33,i64_load16_u,LoadOrStoreImm) \
		visit(0x34,i64_load32_s,LoadOrStoreImm) \
		visit(0x35,i64_load32_u,LoadOrStoreImm) \

	#define ENUM_STORE_OPS(visit) \
		visit(0x36,i32_store,LoadOrStoreImm) \
		visit(0x37,i64_store,LoadOrStoreImm) \
		visit(0x38,f32_store,LoadOrStoreImm) \
		visit(0x39,f64_store,LoadOrStoreImm) \
		visit(0x3a,i32_store8,LoadOrStoreImm) \
		visit(0x3b,i32_store16,LoadOrStoreImm) \
		visit(0x3c,i64_store8,LoadOrStoreImm) \
		visit(0x3d,i64_store16,LoadOrStoreImm) \
		visit(0x3e,i64_store32,LoadOrStoreImm)

	#define ENUM_LITERAL_OPS(visit) \
		visit(0x41,i32_const,LiteralImm<int32>) \
		visit(0x42,i64_const,LiteralImm<int64>) \
		visit(0x43,f32_const,LiteralImm<float32>) \
		visit(0x44,f64_const,LiteralImm<float64>)

	#define ENUM_I32_COMPARE_OPS(visit) \
		visit(0x46,i32_eq,NoImm) \
		visit(0x47,i32_ne,NoImm) \
		visit(0x48,i32_lt_s,NoImm) \
		visit(0x49,i32_lt_u,NoImm) \
		visit(0x4a,i32_gt_s,NoImm) \
		visit(0x4b,i32_gt_u,NoImm) \
		visit(0x4c,i32_le_s,NoImm) \
		visit(0x4d,i32_le_u,NoImm) \
		visit(0x4e,i32_ge_s,NoImm) \
		visit(0x4f,i32_ge_u,NoImm)
	
	#define ENUM_I64_COMPARE_OPS(visit) \
		visit(0x51,i64_eq,NoImm) \
		visit(0x52,i64_ne,NoImm) \
		visit(0x53,i64_lt_s,NoImm) \
		visit(0x54,i64_lt_u,NoImm) \
		visit(0x55,i64_gt_s,NoImm) \
		visit(0x56,i64_gt_u,NoImm) \
		visit(0x57,i64_le_s,NoImm) \
		visit(0x58,i64_le_u,NoImm) \
		visit(0x59,i64_ge_s,NoImm) \
		visit(0x5a,i64_ge_u,NoImm)
	
	#define ENUM_F32_COMPARE_OPS(visit) \
		visit(0x5b,f32_eq,NoImm) \
		visit(0x5c,f32_ne,NoImm) \
		visit(0x5d,f32_lt,NoImm) \
		visit(0x5e,f32_gt,NoImm) \
		visit(0x5f,f32_le,NoImm) \
		visit(0x60,f32_ge,NoImm)

	#define ENUM_F64_COMPARE_OPS(visit) \
		visit(0x61,f64_eq,NoImm) \
		visit(0x62,f64_ne,NoImm) \
		visit(0x63,f64_lt,NoImm) \
		visit(0x64,f64_gt,NoImm) \
		visit(0x65,f64_le,NoImm) \
		visit(0x66,f64_ge,NoImm)
	
	#define ENUM_I32_UNARY_OPS(visit) \
		visit(0x67,i32_clz,NoImm) \
		visit(0x68,i32_ctz,NoImm) \
		visit(0x69,i32_popcnt,NoImm)

	#define ENUM_I32_BINARY_OPS(visit) \
		visit(0x6a,i32_add,NoImm) \
		visit(0x6b,i32_sub,NoImm) \
		visit(0x6c,i32_mul,NoImm) \
		visit(0x6d,i32_div_s,NoImm) \
		visit(0x6e,i32_div_u,NoImm) \
		visit(0x6f,i32_rem_s,NoImm) \
		visit(0x70,i32_rem_u,NoImm) \
		visit(0x71,i32_and,NoImm) \
		visit(0x72,i32_or,NoImm) \
		visit(0x73,i32_xor,NoImm) \
		visit(0x74,i32_shl,NoImm) \
		visit(0x75,i32_shr_s,NoImm) \
		visit(0x76,i32_shr_u,NoImm) \
		visit(0x77,i32_rotl,NoImm) \
		visit(0x78,i32_rotr,NoImm)

	#define ENUM_I64_UNARY_OPS(visit) \
		visit(0x79,i64_clz,NoImm) \
		visit(0x7a,i64_ctz,NoImm) \
		visit(0x7b,i64_popcnt,NoImm)

	#define ENUM_I64_BINARY_OPS(visit) \
		visit(0x7c,i64_add,NoImm) \
		visit(0x7d,i64_sub,NoImm) \
		visit(0x7e,i64_mul,NoImm) \
		visit(0x7f,i64_div_s,NoImm) \
		visit(0x80,i64_div_u,NoImm) \
		visit(0x81,i64_rem_s,NoImm) \
		visit(0x82,i64_rem_u,NoImm) \
		visit(0x83,i64_and,NoImm) \
		visit(0x84,i64_or,NoImm) \
		visit(0x85,i64_xor,NoImm) \
		visit(0x86,i64_shl,NoImm) \
		visit(0x87,i64_shr_s,NoImm) \
		visit(0x88,i64_shr_u,NoImm) \
		visit(0x89,i64_rotl,NoImm) \
		visit(0x8a,i64_rotr,NoImm)

	#define ENUM_F32_UNARY_OPS(visit) \
		visit(0x8b,f32_abs,NoImm) \
		visit(0x8c,f32_neg,NoImm) \
		visit(0x8d,f32_ceil,NoImm) \
		visit(0x8e,f32_floor,NoImm) \
		visit(0x8f,f32_trunc,NoImm) \
		visit(0x90,f32_nearest,NoImm) \
		visit(0x91,f32_sqrt,NoImm)

	#define ENUM_F32_BINARY_OPS(visit) \
		visit(0x92,f32_add,NoImm) \
		visit(0x93,f32_sub,NoImm) \
		visit(0x94,f32_mul,NoImm) \
		visit(0x95,f32_div,NoImm) \
		visit(0x96,f32_min,NoImm) \
		visit(0x97,f32_max,NoImm) \
		visit(0x98,f32_copysign,NoImm)

	#define ENUM_F64_UNARY_OPS(visit) \
		visit(0x99,f64_abs,NoImm) \
		visit(0x9a,f64_neg,NoImm) \
		visit(0x9b,f64_ceil,NoImm) \
		visit(0x9c,f64_floor,NoImm) \
		visit(0x9d,f64_trunc,NoImm) \
		visit(0x9e,f64_nearest,NoImm) \
		visit(0x9f,f64_sqrt,NoImm)

	#define ENUM_F64_BINARY_OPS(visit) \
		visit(0xa0,f64_add,NoImm) \
		visit(0xa1,f64_sub,NoImm) \
		visit(0xa2,f64_mul,NoImm) \
		visit(0xa3,f64_div,NoImm) \
		visit(0xa4,f64_min,NoImm) \
		visit(0xa5,f64_max,NoImm) \
		visit(0xa6,f64_copysign,NoImm)

	#define ENUM_CONVERSION_OPS(visit) \
		visit(0xa7,i32_wrap_i64,NoImm) \
		visit(0xa8,i32_trunc_s_f32,NoImm) \
		visit(0xa9,i32_trunc_u_f32,NoImm) \
		visit(0xaa,i32_trunc_s_f64,NoImm) \
		visit(0xab,i32_trunc_u_f64,NoImm) \
		visit(0xac,i64_extend_s_i32,NoImm) \
		visit(0xad,i64_extend_u_i32,NoImm) \
		visit(0xae,i64_trunc_s_f32,NoImm) \
		visit(0xaf,i64_trunc_u_f32,NoImm) \
		visit(0xb0,i64_trunc_s_f64,NoImm) \
		visit(0xb1,i64_trunc_u_f64,NoImm) \
		visit(0xb2,f32_convert_s_i32,NoImm) \
		visit(0xb3,f32_convert_u_i32,NoImm) \
		visit(0xb4,f32_convert_s_i64,NoImm) \
		visit(0xb5,f32_convert_u_i64,NoImm) \
		visit(0xb6,f32_demote_f64,NoImm) \
		visit(0xb7,f64_convert_s_i32,NoImm) \
		visit(0xb8,f64_convert_u_i32,NoImm) \
		visit(0xb9,f64_convert_s_i64,NoImm) \
		visit(0xba,f64_convert_u_i64,NoImm) \
		visit(0xbb,f64_promote_f32,NoImm) \
		visit(0xbc,i32_reinterpret_f32,NoImm) \
		visit(0xbd,i64_reinterpret_f64,NoImm) \
		visit(0xbe,f32_reinterpret_i32,NoImm) \
		visit(0xbf,f64_reinterpret_i64,NoImm)

	#define ENUM_MISC_OPS(visit) \
		visit(0x45,i32_eqz,NoImm) \
		visit(0x50,i64_eqz,NoImm) \
		visit(0x3f,current_memory,MemoryImm) \
		visit(0x40,grow_memory,MemoryImm) \
		visit(0xff,error,ErrorImm)

	#define ENUM_NONCONTROL_OPS(visit) \
		ENUM_LOAD_OPS(visit) ENUM_STORE_OPS(visit) \
		ENUM_LITERAL_OPS(visit) \
		ENUM_VARIABLE_OPS(visit) \
		ENUM_I32_BINARY_OPS(visit) ENUM_I32_UNARY_OPS(visit) ENUM_I32_COMPARE_OPS(visit) \
		ENUM_I64_BINARY_OPS(visit) ENUM_I64_UNARY_OPS(visit) ENUM_I64_COMPARE_OPS(visit) \
		ENUM_F32_BINARY_OPS(visit) ENUM_F32_UNARY_OPS(visit) ENUM_F32_COMPARE_OPS(visit) \
		ENUM_F64_BINARY_OPS(visit) ENUM_F64_UNARY_OPS(visit) ENUM_F64_COMPARE_OPS(visit) \
		ENUM_CONVERSION_OPS(visit) \
		ENUM_MISC_OPS(visit)

	#define ENUM_OPS(visit) \
		ENUM_NONCONTROL_OPS(visit) \
		ENUM_CONTROL_OPS(visit)

	enum class Opcode : uint8
	{
		#define VISIT_OPCODE(encoding,name,imm) name = encoding,
		ENUM_OPS(VISIT_OPCODE)
		#undef VISIT_OPCODE
	};

	template<typename Stream>
	void serialize(Stream& stream,Opcode& opcode) { return Serialization::serializeNativeValue(stream,*(uint8*)&opcode); }

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
		uintp typeIndex;
		
		template<typename Stream>
		friend void serialize(Stream& stream,CallIndirectImm& imm)
		{
			serializeVarUInt32(stream,imm.typeIndex);

			uint8 reserved = 0;
			serializeVarUInt1(stream,reserved);
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

	struct MemoryImm
	{
		template<typename Stream>
		friend void serialize(Stream& stream,MemoryImm& imm)
		{
			uint8 reserved = 0;
			serializeVarUInt1(stream,reserved);
		}
	};

	struct ErrorImm
	{
		std::string message;
		
		template<typename Stream>
		friend void serialize(Stream& stream,ErrorImm& imm)
		{ serialize(stream,imm.message); }
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
			ENUM_OPS(VISIT_OPCODE)
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
		ENUM_OPS(VISIT_OPCODE)
		#undef VISIT_OPCODE
	};

	WEBASSEMBLY_API const char* getOpcodeName(Opcode opcode);
}
