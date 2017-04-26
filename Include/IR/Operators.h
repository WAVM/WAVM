#pragma once

#include "Core/Core.h"
#include "IR.h"
#include "Types.h"
#include "Inline/Serialization.h"

namespace IR
{
	// Structures for operator immediates

	struct NoImm {};
	struct MemoryImm {};

	struct ControlStructureImm
	{
		ResultType resultType;
	};

	struct BranchImm
	{
		uint32 targetDepth;
	};

	PACKED_STRUCT(
	struct BranchTableImm
	{
		uint32 defaultTargetDepth;
		uintp branchTableIndex; // An index into the FunctionDef's branchTables array.
	});

	template<typename Value>
	struct LiteralImm
	{
		Value value;
	};

	struct GetOrSetVariableImm
	{
		uint32 variableIndex;
	};

	struct CallImm
	{
		uint32 functionIndex;
	};

	struct CallIndirectImm
	{
		IndexedFunctionType type;
	};

	PACKED_STRUCT(
	template<size_t naturalAlignmentLog2>
	struct LoadOrStoreImm
	{
		uint8 alignmentLog2;
		uint32 offset;
	});

	#if ENABLE_SIMD_PROTOTYPE
	template<size_t numLanes>
	struct LaneIndexImm
	{
		uint8 laneIndex;
	};
	
	template<size_t numLanes>
	struct SwizzleImm
	{
		uint8 laneIndices[numLanes];
	};
	
	template<size_t numLanes>
	struct ShuffleImm
	{
		uint8 laneIndices[numLanes];
	};
	#endif

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
		VECTORSELECT(V,B) : (V,V,B) -> V
        BUILDVECTOR(S,N,V) : S{N} -> V
        REPLACELANE(S,V) : (V,S) -> V
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
		visitOp(0xbf,f64_reinterpret_i64,"f64.reinterpret/i64",NoImm,UNARY(i64,f64)) \
		ENUM_SIMD_OPERATORS(visitOp)

	#if !ENABLE_SIMD_PROTOTYPE
	#define ENUM_SIMD_OPERATORS(visitOp)
	#else
	#define ENUM_SIMD_OPERATORS(visitOp) \
		visitOp(0xd001,v128_and,"v128.and",NoImm,BINARY(v128,v128)) \
		visitOp(0xd002,v128_or,"v128.or",NoImm,BINARY(v128,v128)) \
		visitOp(0xd003,v128_xor,"v128.xor",NoImm,BINARY(v128,v128)) \
		visitOp(0xd004,v128_not,"v128.not",NoImm,BINARY(v128,v128)) \
		visitOp(0xd005,v128_load,"v128.load",LoadOrStoreImm<4>,LOAD(v128)) \
		visitOp(0xd006,v128_store,"v128.store",LoadOrStoreImm<4>,STORE(v128)) \
		\
		visitOp(0xd007,v8x16_select,"v8x16.select",NoImm,VECTORSELECT(v128,b8x16)) \
		visitOp(0xd008,v8x16_swizzle,"v8x16.swizzle",SwizzleImm<16>,UNARY(v128,v128)) \
		visitOp(0xd009,v8x16_shuffle,"v8x16.shuffle",ShuffleImm<16>,BINARY(v128,v128)) \
		\
		visitOp(0xd00a,i8x16_build,"i8x16.build",NoImm,BUILDVECTOR(i32,16,v128)) \
		visitOp(0xd00b,i8x16_splat,"i8x16.splat",NoImm,UNARY(i32,v128)) \
		visitOp(0xd00c,i8x16_extract_lane_s,"i8x16.extract_lane_s",LaneIndexImm<16>,UNARY(v128,i32)) \
		visitOp(0xd00d,i8x16_extract_lane_u,"i8x16.extract_lane_u",LaneIndexImm<16>,UNARY(v128,i32)) \
		visitOp(0xd00e,i8x16_replace_lane,"i8x16.replace_lane",LaneIndexImm<16>,REPLACELANE(i32,v128)) \
		visitOp(0xd00f,i8x16_add,"i8x16.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xd010,i8x16_sub,"i8x16.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xd011,i8x16_mul,"i8x16.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xd012,i8x16_neg,"i8x16.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xd013,i8x16_add_sat_s,"i8x16.add_sat_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xd014,i8x16_add_sat_u,"i8x16.add_sat_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xd015,i8x16_sub_sat_s,"i8x16.sub_sat_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xd016,i8x16_sub_sat_u,"i8x16.sub_sat_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xd017,i8x16_shl,"i8x16.shl",NoImm,BINARY(v128,v128)) \
		visitOp(0xd018,i8x16_shr_s,"i8x16.shr_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xd019,i8x16_shr_u,"i8x16.shr_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xd01a,i8x16_eq,"i8x16.eq",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xd01b,i8x16_ne,"i8x16.ne",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xd01c,i8x16_lt_s,"i8x16.lt_s",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xd01d,i8x16_lt_u,"i8x16.lt_u",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xd01e,i8x16_le_s,"i8x16.le_s",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xd01f,i8x16_le_u,"i8x16.le_u",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xd020,i8x16_gt_s,"i8x16.gt_s",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xd021,i8x16_gt_u,"i8x16.gt_u",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xd022,i8x16_ge_s,"i8x16.ge_s",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xd023,i8x16_ge_u,"i8x16.ge_u",NoImm,BINARY(v128,b8x16)) \
		\
		visitOp(0xd024,v16x8_select,"v16x8.select",NoImm,VECTORSELECT(v128,b16x8)) \
		visitOp(0xd025,v16x8_swizzle,"v16x8.swizzle",SwizzleImm<8>,UNARY(v128,v128)) \
		visitOp(0xd026,v16x8_shuffle,"v16x8.shuffle",ShuffleImm<8>,BINARY(v128,v128)) \
		\
		visitOp(0xd027,i16x8_build,"i16x8.build",NoImm,BUILDVECTOR(i32,8,v128)) \
		visitOp(0xd028,i16x8_splat,"i16x8.splat",NoImm,UNARY(i32,v128)) \
		visitOp(0xd029,i16x8_extract_lane_s,"i16x8.extract_lane_s",LaneIndexImm<8>,UNARY(v128,i32)) \
		visitOp(0xd02a,i16x8_extract_lane_u,"i16x8.extract_lane_u",LaneIndexImm<8>,UNARY(v128,i32)) \
		visitOp(0xd02b,i16x8_replace_lane,"i16x8.replace_lane",LaneIndexImm<8>,REPLACELANE(i32,v128)) \
		visitOp(0xd02c,i16x8_add,"i16x8.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xd02d,i16x8_sub,"i16x8.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xd02e,i16x8_mul,"i16x8.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xd02f,i16x8_neg,"i16x8.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xd030,i16x8_add_sat_s,"i16x8.add_sat_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xd031,i16x8_add_sat_u,"i16x8.add_sat_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xd032,i16x8_sub_sat_s,"i16x8.sub_sat_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xd033,i16x8_sub_sat_u,"i16x8.sub_sat_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xd034,i16x8_shl,"i16x8.shl",NoImm,BINARY(v128,v128)) \
		visitOp(0xd035,i16x8_shr_s,"i16x8.shr_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xd036,i16x8_shr_u,"i16x8.shr_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xd037,i16x8_eq,"i16x8.eq",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xd038,i16x8_ne,"i16x8.ne",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xd039,i16x8_lt_s,"i16x8.lt_s",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xd03a,i16x8_lt_u,"i16x8.lt_u",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xd03b,i16x8_le_s,"i16x8.le_s",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xd03c,i16x8_le_u,"i16x8.le_u",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xd03d,i16x8_gt_s,"i16x8.gt_s",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xd03e,i16x8_gt_u,"i16x8.gt_u",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xd03f,i16x8_ge_s,"i16x8.ge_s",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xd040,i16x8_ge_u,"i16x8.ge_u",NoImm,BINARY(v128,b16x8)) \
		\
		visitOp(0xd041,v32x4_select,"v32x4.select",NoImm,VECTORSELECT(v128,b32x4)) \
		visitOp(0xd042,v32x4_swizzle,"v32x4.swizzle",SwizzleImm<4>,UNARY(v128,v128)) \
		visitOp(0xd043,v32x4_shuffle,"v32x4.shuffle",ShuffleImm<4>,BINARY(v128,v128)) \
		\
		visitOp(0xd044,v32x4_load1,"v32x4.load1",LoadOrStoreImm<2>,LOAD(v128)) \
		visitOp(0xd045,v32x4_load2,"v32x4.load2",LoadOrStoreImm<2>,LOAD(v128)) \
		visitOp(0xd046,v32x4_load3,"v32x4.load3",LoadOrStoreImm<2>,LOAD(v128)) \
		visitOp(0xd047,v32x4_store1,"v32x4.store1",LoadOrStoreImm<2>,STORE(v128)) \
		visitOp(0xd048,v32x4_store2,"v32x4.store2",LoadOrStoreImm<2>,STORE(v128)) \
		visitOp(0xd049,v32x4_store3,"v32x4.store3",LoadOrStoreImm<2>,STORE(v128)) \
		\
		visitOp(0xd04a,i32x4_build,"i32x4.build",NoImm,BUILDVECTOR(i32,4,v128)) \
		visitOp(0xd04b,i32x4_splat,"i32x4.splat",NoImm,UNARY(i32,v128)) \
		visitOp(0xd04c,i32x4_extract_lane,"i32x4.extract_lane",LaneIndexImm<4>,UNARY(v128,i32)) \
		visitOp(0xd04d,i32x4_replace_lane,"i32x4.replace_lane",LaneIndexImm<4>,REPLACELANE(i32,v128)) \
		visitOp(0xd04e,i32x4_add,"i32x4.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xd04f,i32x4_sub,"i32x4.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xd050,i32x4_mul,"i32x4.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xd051,i32x4_neg,"i32x4.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xd052,i32x4_shl,"i32x4.shl",NoImm,BINARY(v128,v128)) \
		visitOp(0xd053,i32x4_shr_s,"i32x4.shr_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xd054,i32x4_shr_u,"i32x4.shr_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xd055,i32x4_eq,"i32x4.eq",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd056,i32x4_ne,"i32x4.ne",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd057,i32x4_lt_s,"i32x4.lt_s",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd058,i32x4_lt_u,"i32x4.lt_u",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd059,i32x4_le_s,"i32x4.le_s",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd05a,i32x4_le_u,"i32x4.le_u",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd05b,i32x4_gt_s,"i32x4.gt_s",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd05c,i32x4_gt_u,"i32x4.gt_u",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd05d,i32x4_ge_s,"i32x4.ge_s",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd05e,i32x4_ge_u,"i32x4.ge_u",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd05f,i32x4_trunc_s_f32x4,"i32x4.trunc_s/f32x4",NoImm,UNARY(v128,v128)) \
		visitOp(0xd060,i32x4_trunc_u_f32x4,"i32x4.trunc_u/f32x4",NoImm,UNARY(v128,v128)) \
		\
		visitOp(0xd061,f32x4_build,"f32x4.build",NoImm,BUILDVECTOR(f32,4,v128)) \
		visitOp(0xd062,f32x4_splat,"f32x4.splat",NoImm,UNARY(f32,v128)) \
		visitOp(0xd063,f32x4_extract_lane,"f32x4.extract_lane",LaneIndexImm<4>,UNARY(v128,f32)) \
		visitOp(0xd064,f32x4_replace_lane,"f32x4.replace_lane",LaneIndexImm<4>,REPLACELANE(f32,v128)) \
		visitOp(0xd065,f32x4_add,"f32x4.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xd066,f32x4_sub,"f32x4.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xd067,f32x4_mul,"f32x4.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xd068,f32x4_neg,"f32x4.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xd069,f32x4_eq,"f32x4.eq",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd06a,f32x4_ne,"f32x4.ne",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd06b,f32x4_lt,"f32x4.lt",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd06c,f32x4_le,"f32x4.le",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd06d,f32x4_gt,"f32x4.gt",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd06e,f32x4_ge,"f32x4.ge",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xd06f,f32x4_abs,"f32x4.abs",NoImm,UNARY(v128,v128)) \
		visitOp(0xd070,f32x4_min,"f32x4.min",NoImm,UNARY(v128,v128)) \
		visitOp(0xd071,f32x4_max,"f32x4.max",NoImm,UNARY(v128,v128)) \
		visitOp(0xd072,f32x4_div,"f32x4.div",NoImm,BINARY(v128,v128)) \
		visitOp(0xd073,f32x4_sqrt,"f32x4.sqrt",NoImm,UNARY(v128,v128)) \
		visitOp(0xd074,f32x4_convert_s_i32x4,"f32x4.convert_s/i32x4",NoImm,UNARY(v128,v128)) \
		visitOp(0xd075,f32x4_convert_u_i32x4,"f32x4.convert_u/i32x4",NoImm,UNARY(v128,v128)) \
		\
		visitOp(0xd076,v64x2_select,"v64x2.select",NoImm,VECTORSELECT(v128,b64x2)) \
		visitOp(0xd077,v64x2_swizzle,"v64x2.swizzle",SwizzleImm<2>,UNARY(v128,v128)) \
		visitOp(0xd078,v64x2_shuffle,"v64x2.shuffle",ShuffleImm<2>,BINARY(v128,v128)) \
		\
		visitOp(0xd079,i64x2_build,"i64x2.build",NoImm,BUILDVECTOR(i64,2,v128)) \
		visitOp(0xd07a,i64x2_splat,"i64x2.splat",NoImm,UNARY(i64,v128)) \
		visitOp(0xd07b,i64x2_extract_lane,"i64x2.extract_lane",LaneIndexImm<2>,UNARY(v128,i64)) \
		visitOp(0xd07c,i64x2_replace_lane,"i64x2.replace_lane",LaneIndexImm<2>,REPLACELANE(i64,v128)) \
		visitOp(0xd07d,i64x2_add,"i64x2.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xd07e,i64x2_sub,"i64x2.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xd07f,i64x2_mul,"i64x2.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xd080,i64x2_neg,"i64x2.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xd081,i64x2_shl,"i64x2.shl",NoImm,BINARY(v128,v128)) \
		visitOp(0xd082,i64x2_shr_s,"i64x2.shr_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xd083,i64x2_shr_u,"i64x2.shr_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xd084,i64x2_eq,"i64x2.eq",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd085,i64x2_ne,"i64x2.ne",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd086,i64x2_lt_s,"i64x2.lt_s",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd087,i64x2_lt_u,"i64x2.lt_u",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd088,i64x2_le_s,"i64x2.le_s",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd089,i64x2_le_u,"i64x2.le_u",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd08a,i64x2_gt_s,"i64x2.gt_s",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd08b,i64x2_gt_u,"i64x2.gt_u",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd08c,i64x2_ge_s,"i64x2.ge_s",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd08d,i64x2_ge_u,"i64x2.ge_u",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd08e,i64x2_trunc_s_f64x2,"i64x2.trunc_s/f64x2",NoImm,UNARY(v128,v128)) \
		visitOp(0xd08f,i64x2_trunc_u_f64x2,"i64x2.trunc_u/f64x2",NoImm,UNARY(v128,v128)) \
		\
		visitOp(0xd090,f64x2_build,"f64x2.build",NoImm,BUILDVECTOR(f64,2,v128)) \
		visitOp(0xd091,f64x2_splat,"f64x2.splat",NoImm,UNARY(f64,v128)) \
		visitOp(0xd092,f64x2_extract_lane,"f64x2.extract_lane",LaneIndexImm<2>,UNARY(v128,f64)) \
		visitOp(0xd093,f64x2_replace_lane,"f64x2.replace_lane",LaneIndexImm<2>,REPLACELANE(f64,v128)) \
		visitOp(0xd094,f64x2_add,"f64x2.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xd095,f64x2_sub,"f64x2.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xd096,f64x2_mul,"f64x2.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xd097,f64x2_neg,"f64x2.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xd098,f64x2_eq,"f64x2.eq",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd099,f64x2_ne,"f64x2.ne",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd09a,f64x2_lt,"f64x2.lt",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd09b,f64x2_le,"f64x2.le",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd09c,f64x2_gt,"f64x2.gt",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd09d,f64x2_ge,"f64x2.ge",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xd09e,f64x2_abs,"f64x2.abs",NoImm,UNARY(v128,v128)) \
		visitOp(0xd09f,f64x2_min,"f64x2.min",NoImm,BINARY(v128,v128)) \
		visitOp(0xd0a0,f64x2_max,"f64x2.max",NoImm,BINARY(v128,v128)) \
		visitOp(0xd0a1,f64x2_div,"f64x2.div",NoImm,BINARY(v128,v128)) \
		visitOp(0xd0a2,f64x2_sqrt,"f64x2.sqrt",NoImm,UNARY(v128,v128)) \
		visitOp(0xd0a3,f64x2_convert_s_i64x2,"f64x2.convert_s/i64x2",NoImm,UNARY(v128,v128)) \
		visitOp(0xd0a4,f64x2_convert_u_i64x2,"f64x2.convert_u/i64x2",NoImm,UNARY(v128,v128)) \
		\
		visitOp(0xd0a5,b8x16_build,"b8x16.build",NoImm,BUILDVECTOR(i32,16,b8x16)) \
		visitOp(0xd0a6,b8x16_splat,"b8x16.splat",NoImm,UNARY(i32,b8x16)) \
		visitOp(0xd0a7,b8x16_extract_lane,"b8x16.extract_lane",LaneIndexImm<16>,UNARY(b8x16,i32)) \
		visitOp(0xd0a8,b8x16_replace_lane,"b8x16.replace_lane",LaneIndexImm<16>,REPLACELANE(i32,b8x16)) \
		visitOp(0xd0a9,b8x16_and,"b8x16.and",NoImm,BINARY(b8x16,b8x16)) \
		visitOp(0xd0aa,b8x16_or,"b8x16.or",NoImm,BINARY(b8x16,b8x16)) \
		visitOp(0xd0ab,b8x16_xor,"b8x16.xor",NoImm,BINARY(b8x16,b8x16)) \
		visitOp(0xd0ac,b8x16_not,"b8x16.not",NoImm,UNARY(b8x16,b8x16)) \
		visitOp(0xd0ad,b8x16_any_true,"b8x16.any_true",NoImm,UNARY(b8x16,i32)) \
		visitOp(0xd0ae,b8x16_all_true,"b8x16.all_true",NoImm,UNARY(b8x16,i32)) \
		\
		visitOp(0xd0af,b16x8_build,"b16x8.build",NoImm,BUILDVECTOR(i32,8,b16x8)) \
		visitOp(0xd0b0,b16x8_splat,"b16x8.splat",NoImm,UNARY(i32,b16x8)) \
		visitOp(0xd0b1,b16x8_extract_lane,"b16x8.extract_lane",LaneIndexImm<8>,UNARY(b16x8,i32)) \
		visitOp(0xd0b2,b16x8_replace_lane,"b16x8.replace_lane",LaneIndexImm<8>,REPLACELANE(i32,b16x8)) \
		visitOp(0xd0b3,b16x8_and,"b16x8.and",NoImm,BINARY(b16x8,b16x8)) \
		visitOp(0xd0b4,b16x8_or,"b16x8.or",NoImm,BINARY(b16x8,b16x8)) \
		visitOp(0xd0b5,b16x8_xor,"b16x8.xor",NoImm,BINARY(b16x8,b16x8)) \
		visitOp(0xd0b6,b16x8_not,"b16x8.not",NoImm,UNARY(b16x8,b16x8)) \
		visitOp(0xd0b7,b16x8_any_true,"b16x8.any_true",NoImm,UNARY(b16x8,i32)) \
		visitOp(0xd0b8,b16x8_all_true,"b16x8.all_true",NoImm,UNARY(b16x8,i32)) \
		\
		visitOp(0xd0b9,b32x4_build,"b32x4.build",NoImm,BUILDVECTOR(i32,4,b32x4)) \
		visitOp(0xd0ba,b32x4_splat,"b32x4.splat",NoImm,UNARY(i32,b32x4)) \
		visitOp(0xd0bb,b32x4_extract_lane,"b32x4.extract_lane",LaneIndexImm<4>,UNARY(b32x4,i32)) \
		visitOp(0xd0bc,b32x4_replace_lane,"b32x4.replace_lane",LaneIndexImm<4>,REPLACELANE(i32,b32x4)) \
		visitOp(0xd0bd,b32x4_and,"b32x4.and",NoImm,BINARY(b32x4,b32x4)) \
		visitOp(0xd0be,b32x4_or,"b32x4.or",NoImm,BINARY(b32x4,b32x4)) \
		visitOp(0xd0bf,b32x4_xor,"b32x4.xor",NoImm,BINARY(b32x4,b32x4)) \
		visitOp(0xd0c0,b32x4_not,"b32x4.not",NoImm,UNARY(b32x4,b32x4)) \
		visitOp(0xd0c1,b32x4_any_true,"b32x4.any_true",NoImm,UNARY(b32x4,i32)) \
		visitOp(0xd0c2,b32x4_all_true,"b32x4.all_true",NoImm,UNARY(b32x4,i32)) \
		\
		visitOp(0xd0c3,b64x2_build,"b64x2.build",NoImm,BUILDVECTOR(i32,2,b64x2)) \
		visitOp(0xd0c4,b64x2_splat,"b64x2.splat",NoImm,UNARY(i32,b64x2)) \
		visitOp(0xd0c5,b64x2_extract_lane,"b64x2.extract_lane",LaneIndexImm<2>,UNARY(b64x2,i32)) \
		visitOp(0xd0c6,b64x2_replace_lane,"b64x2.replace_lane",LaneIndexImm<2>,REPLACELANE(i32,b64x2)) \
		visitOp(0xd0c7,b64x2_and,"b64x2.and",NoImm,BINARY(b64x2,b64x2)) \
		visitOp(0xd0c8,b64x2_or,"b64x2.or",NoImm,BINARY(b64x2,b64x2)) \
		visitOp(0xd0c9,b64x2_xor,"b64x2.xor",NoImm,BINARY(b64x2,b64x2)) \
		visitOp(0xd0ca,b64x2_not,"b64x2.not",NoImm,UNARY(b64x2,b64x2)) \
		visitOp(0xd0cb,b64x2_any_true,"b64x2.any_true",NoImm,UNARY(b64x2,i32)) \
		visitOp(0xd0cc,b64x2_all_true,"b64x2.all_true",NoImm,UNARY(b64x2,i32))
	#endif

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

	PACKED_STRUCT(
	template<typename Imm>
	struct OpcodeAndImm
	{
		Opcode opcode;
		Imm imm;
	});

	// Specialize for the empty immediate structs so they don't take an extra byte of space.
	template<>
	struct OpcodeAndImm<NoImm>
	{
		union
		{
			Opcode opcode;
			NoImm imm;
		};
	};
	template<>
	struct OpcodeAndImm<MemoryImm>
	{
		union
		{
			Opcode opcode;
			MemoryImm imm;
		};
	};

	// Decodes an operator from an input stream and dispatches by opcode.
	struct OperatorDecoderStream
	{
		OperatorDecoderStream(const std::vector<uint8>& codeBytes)
		: nextByte(codeBytes.data()), end(codeBytes.data()+codeBytes.size()) {}

		operator bool() const { return nextByte < end; }

		template<typename Visitor>
		typename Visitor::Result decodeOp(Visitor& visitor)
		{
			assert(nextByte + sizeof(Opcode) <= end);
			Opcode opcode = *(Opcode*)nextByte;
			switch(opcode)
			{
			#define VISIT_OPCODE(opcode,name,nameString,Imm,...) \
				case Opcode::name: \
				{ \
					assert(nextByte + sizeof(OpcodeAndImm<Imm>) <= end); \
					OpcodeAndImm<Imm>* encodedOperator = (OpcodeAndImm<Imm>*)nextByte; \
					nextByte += sizeof(OpcodeAndImm<Imm>); \
					return visitor.name(encodedOperator->imm); \
				}
			ENUM_OPERATORS(VISIT_OPCODE)
			#undef VISIT_OPCODE
			default:
				nextByte += sizeof(Opcode);
				return visitor.unknown(opcode);
			}
		}

		template<typename Visitor>
		typename Visitor::Result decodeOpWithoutConsume(Visitor& visitor)
		{
			const uint8* savedNextByte = nextByte;
			typename Visitor::Result result = decodeOp(visitor);
			nextByte = savedNextByte;
			return result;
		}

	private:

		const uint8* nextByte;
		const uint8* end;
	};

	// Encodes an operator to an output stream.
	struct OperatorEncoderStream
	{
		OperatorEncoderStream(Serialization::OutputStream& inByteStream): byteStream(inByteStream) {}

		#define VISIT_OPCODE(_,name,nameString,Imm,...) \
			void name(Imm imm = {}) \
			{ \
				OpcodeAndImm<Imm>* encodedOperator = (OpcodeAndImm<Imm>*)byteStream.advance(sizeof(OpcodeAndImm<Imm>)); \
				encodedOperator->opcode = Opcode::name; \
				encodedOperator->imm = imm; \
			}
		ENUM_OPERATORS(VISIT_OPCODE)
		#undef VISIT_OPCODE

	private:
		Serialization::OutputStream& byteStream;
	};

	IR_API const char* getOpcodeName(Opcode opcode);
}