#pragma once

#include "Inline/BasicTypes.h"
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
		U32 targetDepth;
	};

	struct BranchTableImm
	{
		Uptr defaultTargetDepth;
		Uptr branchTableIndex; // An index into the FunctionDef's branchTables array.
	};

	template<typename Value>
	struct LiteralImm
	{
		Value value;
	};

	template<bool isGlobal>
	struct GetOrSetVariableImm
	{
		U32 variableIndex;
	};

	struct CallImm
	{
		U32 functionIndex;
	};

	struct CallIndirectImm
	{
		IndexedFunctionType type;
	};

	template<Uptr naturalAlignmentLog2>
	struct LoadOrStoreImm
	{
		U8 alignmentLog2;
		U32 offset;
	};

	#if ENABLE_SIMD_PROTOTYPE
	template<Uptr numLanes>
	struct LaneIndexImm
	{
		U8 laneIndex;
	};
	
	template<Uptr numLanes>
	struct SwizzleImm
	{
		U8 laneIndices[numLanes];
	};
	
	template<Uptr numLanes>
	struct ShuffleImm
	{
		U8 laneIndices[numLanes];
	};
	#endif

	#if ENABLE_THREADING_PROTOTYPE
	struct LaunchThreadImm {};
	
	template<Uptr naturalAlignmentLog2>
	struct AtomicLoadOrStoreImm
	{
		U8 alignmentLog2;
		U32 offset;
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
		visitOp(0x20,get_local,"get_local",GetOrSetVariableImm<false>,GETLOCAL) \
		visitOp(0x21,set_local,"set_local",GetOrSetVariableImm<false>,SETLOCAL) \
		visitOp(0x22,tee_local,"tee_local",GetOrSetVariableImm<false>,TEELOCAL) \
		visitOp(0x23,get_global,"get_global",GetOrSetVariableImm<true>,GETGLOBAL) \
		visitOp(0x24,set_global,"set_global",GetOrSetVariableImm<true>,SETGLOBAL)
	
	/*
		Possible signatures used by ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS:
		NULLARY(T) : () -> T
		UNARY(T,U) : T -> U
		BINARY(T,U) : (T,T) -> U
		LOAD(T) : i32 -> T
		STORE(T) : (i32,T) -> ()
		VECTORSELECT(V,B) : (V,V,B) -> V
        REPLACELANE(S,V) : (V,S) -> V
		COMPAREEXCHANGE(T) : (i32,T,T) -> T
		WAIT(T) : (i32,T,f64) -> i32
		LAUNCHTHREAD : (i32,i32,i32) -> ()
		ATOMICRMW : (i32,T) -> T
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
		visitOp(0x41,i32_const,"i32.const",LiteralImm<I32>,NULLARY(i32)) \
		visitOp(0x42,i64_const,"i64.const",LiteralImm<I64>,NULLARY(i64)) \
		visitOp(0x43,f32_const,"f32.const",LiteralImm<F32>,NULLARY(f32)) \
		visitOp(0x44,f64_const,"f64.const",LiteralImm<F64>,NULLARY(f64)) \
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
		ENUM_SIMD_OPERATORS(visitOp) \
		ENUM_THREADING_OPERATORS(visitOp)

	#if !ENABLE_SIMD_PROTOTYPE
	#define ENUM_SIMD_OPERATORS(visitOp)
	#else
	#define ENUM_SIMD_OPERATORS(visitOp) \
		visitOp(0xfd00,v128_const,"v128.const",LiteralImm<V128>,NULLARY(v128)) \
		\
		visitOp(0xfd01,v128_and,"v128.and",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd02,v128_or,"v128.or",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd03,v128_xor,"v128.xor",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd04,v128_not,"v128.not",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd05,v128_load,"v128.load",LoadOrStoreImm<4>,LOAD(v128)) \
		visitOp(0xfd06,v128_store,"v128.store",LoadOrStoreImm<4>,STORE(v128)) \
		\
		visitOp(0xfd07,v8x16_select,"v8x16.select",NoImm,VECTORSELECT(v128,b8x16)) \
		visitOp(0xfd08,v8x16_swizzle,"v8x16.swizzle",SwizzleImm<16>,UNARY(v128,v128)) \
		visitOp(0xfd09,v8x16_shuffle,"v8x16.shuffle",ShuffleImm<16>,BINARY(v128,v128)) \
		\
		visitOp(0xfd0b,i8x16_splat,"i8x16.splat",NoImm,UNARY(i32,v128)) \
		visitOp(0xfd0c,i8x16_extract_lane_s,"i8x16.extract_lane_s",LaneIndexImm<16>,UNARY(v128,i32)) \
		visitOp(0xfd0d,i8x16_extract_lane_u,"i8x16.extract_lane_u",LaneIndexImm<16>,UNARY(v128,i32)) \
		visitOp(0xfd0e,i8x16_replace_lane,"i8x16.replace_lane",LaneIndexImm<16>,REPLACELANE(i32,v128)) \
		visitOp(0xfd0f,i8x16_add,"i8x16.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd10,i8x16_sub,"i8x16.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd11,i8x16_mul,"i8x16.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd12,i8x16_neg,"i8x16.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd13,i8x16_add_sat_s,"i8x16.add_sat_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd14,i8x16_add_sat_u,"i8x16.add_sat_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd15,i8x16_sub_sat_s,"i8x16.sub_sat_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd16,i8x16_sub_sat_u,"i8x16.sub_sat_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd17,i8x16_shl,"i8x16.shl",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd18,i8x16_shr_s,"i8x16.shr_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd19,i8x16_shr_u,"i8x16.shr_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd1a,i8x16_eq,"i8x16.eq",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xfd1b,i8x16_ne,"i8x16.ne",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xfd1c,i8x16_lt_s,"i8x16.lt_s",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xfd1d,i8x16_lt_u,"i8x16.lt_u",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xfd1e,i8x16_le_s,"i8x16.le_s",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xfd1f,i8x16_le_u,"i8x16.le_u",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xfd20,i8x16_gt_s,"i8x16.gt_s",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xfd21,i8x16_gt_u,"i8x16.gt_u",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xfd22,i8x16_ge_s,"i8x16.ge_s",NoImm,BINARY(v128,b8x16)) \
		visitOp(0xfd23,i8x16_ge_u,"i8x16.ge_u",NoImm,BINARY(v128,b8x16)) \
		\
		visitOp(0xfd24,v16x8_select,"v16x8.select",NoImm,VECTORSELECT(v128,b16x8)) \
		visitOp(0xfd25,v16x8_swizzle,"v16x8.swizzle",SwizzleImm<8>,UNARY(v128,v128)) \
		visitOp(0xfd26,v16x8_shuffle,"v16x8.shuffle",ShuffleImm<8>,BINARY(v128,v128)) \
		\
		visitOp(0xfd28,i16x8_splat,"i16x8.splat",NoImm,UNARY(i32,v128)) \
		visitOp(0xfd29,i16x8_extract_lane_s,"i16x8.extract_lane_s",LaneIndexImm<8>,UNARY(v128,i32)) \
		visitOp(0xfd2a,i16x8_extract_lane_u,"i16x8.extract_lane_u",LaneIndexImm<8>,UNARY(v128,i32)) \
		visitOp(0xfd2b,i16x8_replace_lane,"i16x8.replace_lane",LaneIndexImm<8>,REPLACELANE(i32,v128)) \
		visitOp(0xfd2c,i16x8_add,"i16x8.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd2d,i16x8_sub,"i16x8.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd2e,i16x8_mul,"i16x8.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd2f,i16x8_neg,"i16x8.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd30,i16x8_add_sat_s,"i16x8.add_sat_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd31,i16x8_add_sat_u,"i16x8.add_sat_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd32,i16x8_sub_sat_s,"i16x8.sub_sat_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd33,i16x8_sub_sat_u,"i16x8.sub_sat_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd34,i16x8_shl,"i16x8.shl",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd35,i16x8_shr_s,"i16x8.shr_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd36,i16x8_shr_u,"i16x8.shr_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd37,i16x8_eq,"i16x8.eq",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xfd38,i16x8_ne,"i16x8.ne",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xfd39,i16x8_lt_s,"i16x8.lt_s",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xfd3a,i16x8_lt_u,"i16x8.lt_u",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xfd3b,i16x8_le_s,"i16x8.le_s",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xfd3c,i16x8_le_u,"i16x8.le_u",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xfd3d,i16x8_gt_s,"i16x8.gt_s",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xfd3e,i16x8_gt_u,"i16x8.gt_u",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xfd3f,i16x8_ge_s,"i16x8.ge_s",NoImm,BINARY(v128,b16x8)) \
		visitOp(0xfd40,i16x8_ge_u,"i16x8.ge_u",NoImm,BINARY(v128,b16x8)) \
		\
		visitOp(0xfd41,v32x4_select,"v32x4.select",NoImm,VECTORSELECT(v128,b32x4)) \
		visitOp(0xfd42,v32x4_swizzle,"v32x4.swizzle",SwizzleImm<4>,UNARY(v128,v128)) \
		visitOp(0xfd43,v32x4_shuffle,"v32x4.shuffle",ShuffleImm<4>,BINARY(v128,v128)) \
		\
		visitOp(0xfd44,v32x4_load1,"v32x4.load1",LoadOrStoreImm<2>,LOAD(v128)) \
		visitOp(0xfd45,v32x4_load2,"v32x4.load2",LoadOrStoreImm<2>,LOAD(v128)) \
		visitOp(0xfd46,v32x4_load3,"v32x4.load3",LoadOrStoreImm<2>,LOAD(v128)) \
		visitOp(0xfd47,v32x4_store1,"v32x4.store1",LoadOrStoreImm<2>,STORE(v128)) \
		visitOp(0xfd48,v32x4_store2,"v32x4.store2",LoadOrStoreImm<2>,STORE(v128)) \
		visitOp(0xfd49,v32x4_store3,"v32x4.store3",LoadOrStoreImm<2>,STORE(v128)) \
		\
		visitOp(0xfd4b,i32x4_splat,"i32x4.splat",NoImm,UNARY(i32,v128)) \
		visitOp(0xfd4c,i32x4_extract_lane,"i32x4.extract_lane",LaneIndexImm<4>,UNARY(v128,i32)) \
		visitOp(0xfd4d,i32x4_replace_lane,"i32x4.replace_lane",LaneIndexImm<4>,REPLACELANE(i32,v128)) \
		visitOp(0xfd4e,i32x4_add,"i32x4.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd4f,i32x4_sub,"i32x4.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd50,i32x4_mul,"i32x4.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd51,i32x4_neg,"i32x4.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd52,i32x4_shl,"i32x4.shl",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd53,i32x4_shr_s,"i32x4.shr_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd54,i32x4_shr_u,"i32x4.shr_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd55,i32x4_eq,"i32x4.eq",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd56,i32x4_ne,"i32x4.ne",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd57,i32x4_lt_s,"i32x4.lt_s",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd58,i32x4_lt_u,"i32x4.lt_u",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd59,i32x4_le_s,"i32x4.le_s",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd5a,i32x4_le_u,"i32x4.le_u",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd5b,i32x4_gt_s,"i32x4.gt_s",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd5c,i32x4_gt_u,"i32x4.gt_u",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd5d,i32x4_ge_s,"i32x4.ge_s",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd5e,i32x4_ge_u,"i32x4.ge_u",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd5f,i32x4_trunc_s_f32x4,"i32x4.trunc_s/f32x4",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd60,i32x4_trunc_u_f32x4,"i32x4.trunc_u/f32x4",NoImm,UNARY(v128,v128)) \
		\
		visitOp(0xfd62,f32x4_splat,"f32x4.splat",NoImm,UNARY(f32,v128)) \
		visitOp(0xfd63,f32x4_extract_lane,"f32x4.extract_lane",LaneIndexImm<4>,UNARY(v128,f32)) \
		visitOp(0xfd64,f32x4_replace_lane,"f32x4.replace_lane",LaneIndexImm<4>,REPLACELANE(f32,v128)) \
		visitOp(0xfd65,f32x4_add,"f32x4.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd66,f32x4_sub,"f32x4.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd67,f32x4_mul,"f32x4.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd68,f32x4_neg,"f32x4.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd69,f32x4_eq,"f32x4.eq",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd6a,f32x4_ne,"f32x4.ne",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd6b,f32x4_lt,"f32x4.lt",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd6c,f32x4_le,"f32x4.le",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd6d,f32x4_gt,"f32x4.gt",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd6e,f32x4_ge,"f32x4.ge",NoImm,BINARY(v128,b32x4)) \
		visitOp(0xfd6f,f32x4_abs,"f32x4.abs",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd70,f32x4_min,"f32x4.min",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd71,f32x4_max,"f32x4.max",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd72,f32x4_div,"f32x4.div",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd73,f32x4_sqrt,"f32x4.sqrt",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd74,f32x4_convert_s_i32x4,"f32x4.convert_s/i32x4",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd75,f32x4_convert_u_i32x4,"f32x4.convert_u/i32x4",NoImm,UNARY(v128,v128)) \
		\
		visitOp(0xfd76,v64x2_select,"v64x2.select",NoImm,VECTORSELECT(v128,b64x2)) \
		visitOp(0xfd77,v64x2_swizzle,"v64x2.swizzle",SwizzleImm<2>,UNARY(v128,v128)) \
		visitOp(0xfd78,v64x2_shuffle,"v64x2.shuffle",ShuffleImm<2>,BINARY(v128,v128)) \
		\
		visitOp(0xfd7a,i64x2_splat,"i64x2.splat",NoImm,UNARY(i64,v128)) \
		visitOp(0xfd7b,i64x2_extract_lane,"i64x2.extract_lane",LaneIndexImm<2>,UNARY(v128,i64)) \
		visitOp(0xfd7c,i64x2_replace_lane,"i64x2.replace_lane",LaneIndexImm<2>,REPLACELANE(i64,v128)) \
		visitOp(0xfd7d,i64x2_add,"i64x2.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd7e,i64x2_sub,"i64x2.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd7f,i64x2_mul,"i64x2.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd80,i64x2_neg,"i64x2.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd81,i64x2_shl,"i64x2.shl",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd82,i64x2_shr_s,"i64x2.shr_s",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd83,i64x2_shr_u,"i64x2.shr_u",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd84,i64x2_eq,"i64x2.eq",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd85,i64x2_ne,"i64x2.ne",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd86,i64x2_lt_s,"i64x2.lt_s",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd87,i64x2_lt_u,"i64x2.lt_u",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd88,i64x2_le_s,"i64x2.le_s",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd89,i64x2_le_u,"i64x2.le_u",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd8a,i64x2_gt_s,"i64x2.gt_s",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd8b,i64x2_gt_u,"i64x2.gt_u",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd8c,i64x2_ge_s,"i64x2.ge_s",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd8d,i64x2_ge_u,"i64x2.ge_u",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd8e,i64x2_trunc_s_f64x2,"i64x2.trunc_s/f64x2",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd8f,i64x2_trunc_u_f64x2,"i64x2.trunc_u/f64x2",NoImm,UNARY(v128,v128)) \
		\
		visitOp(0xfd91,f64x2_splat,"f64x2.splat",NoImm,UNARY(f64,v128)) \
		visitOp(0xfd92,f64x2_extract_lane,"f64x2.extract_lane",LaneIndexImm<2>,UNARY(v128,f64)) \
		visitOp(0xfd93,f64x2_replace_lane,"f64x2.replace_lane",LaneIndexImm<2>,REPLACELANE(f64,v128)) \
		visitOp(0xfd94,f64x2_add,"f64x2.add",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd95,f64x2_sub,"f64x2.sub",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd96,f64x2_mul,"f64x2.mul",NoImm,BINARY(v128,v128)) \
		visitOp(0xfd97,f64x2_neg,"f64x2.neg",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd98,f64x2_eq,"f64x2.eq",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd99,f64x2_ne,"f64x2.ne",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd9a,f64x2_lt,"f64x2.lt",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd9b,f64x2_le,"f64x2.le",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd9c,f64x2_gt,"f64x2.gt",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd9d,f64x2_ge,"f64x2.ge",NoImm,BINARY(v128,b64x2)) \
		visitOp(0xfd9e,f64x2_abs,"f64x2.abs",NoImm,UNARY(v128,v128)) \
		visitOp(0xfd9f,f64x2_min,"f64x2.min",NoImm,BINARY(v128,v128)) \
		visitOp(0xfda0,f64x2_max,"f64x2.max",NoImm,BINARY(v128,v128)) \
		visitOp(0xfda1,f64x2_div,"f64x2.div",NoImm,BINARY(v128,v128)) \
		visitOp(0xfda2,f64x2_sqrt,"f64x2.sqrt",NoImm,UNARY(v128,v128)) \
		visitOp(0xfda3,f64x2_convert_s_i64x2,"f64x2.convert_s/i64x2",NoImm,UNARY(v128,v128)) \
		visitOp(0xfda4,f64x2_convert_u_i64x2,"f64x2.convert_u/i64x2",NoImm,UNARY(v128,v128)) \
		\
		visitOp(0xfda5,b8x16_const,"b8x16.const",LiteralImm<B8x16>,NULLARY(b8x16)) \
		visitOp(0xfda6,b8x16_splat,"b8x16.splat",NoImm,UNARY(i32,b8x16)) \
		visitOp(0xfda7,b8x16_extract_lane,"b8x16.extract_lane",LaneIndexImm<16>,UNARY(b8x16,i32)) \
		visitOp(0xfda8,b8x16_replace_lane,"b8x16.replace_lane",LaneIndexImm<16>,REPLACELANE(i32,b8x16)) \
		visitOp(0xfda9,b8x16_and,"b8x16.and",NoImm,BINARY(b8x16,b8x16)) \
		visitOp(0xfdaa,b8x16_or,"b8x16.or",NoImm,BINARY(b8x16,b8x16)) \
		visitOp(0xfdab,b8x16_xor,"b8x16.xor",NoImm,BINARY(b8x16,b8x16)) \
		visitOp(0xfdac,b8x16_not,"b8x16.not",NoImm,UNARY(b8x16,b8x16)) \
		visitOp(0xfdad,b8x16_any_true,"b8x16.any_true",NoImm,UNARY(b8x16,i32)) \
		visitOp(0xfdae,b8x16_all_true,"b8x16.all_true",NoImm,UNARY(b8x16,i32)) \
		\
		visitOp(0xfdaf,b16x8_const,"b16x8.const",LiteralImm<B16x8>,NULLARY(b16x8)) \
		visitOp(0xfdb0,b16x8_splat,"b16x8.splat",NoImm,UNARY(i32,b16x8)) \
		visitOp(0xfdb1,b16x8_extract_lane,"b16x8.extract_lane",LaneIndexImm<8>,UNARY(b16x8,i32)) \
		visitOp(0xfdb2,b16x8_replace_lane,"b16x8.replace_lane",LaneIndexImm<8>,REPLACELANE(i32,b16x8)) \
		visitOp(0xfdb3,b16x8_and,"b16x8.and",NoImm,BINARY(b16x8,b16x8)) \
		visitOp(0xfdb4,b16x8_or,"b16x8.or",NoImm,BINARY(b16x8,b16x8)) \
		visitOp(0xfdb5,b16x8_xor,"b16x8.xor",NoImm,BINARY(b16x8,b16x8)) \
		visitOp(0xfdb6,b16x8_not,"b16x8.not",NoImm,UNARY(b16x8,b16x8)) \
		visitOp(0xfdb7,b16x8_any_true,"b16x8.any_true",NoImm,UNARY(b16x8,i32)) \
		visitOp(0xfdb8,b16x8_all_true,"b16x8.all_true",NoImm,UNARY(b16x8,i32)) \
		\
		visitOp(0xfdb9,b32x4_const,"b32x4.const",LiteralImm<B32x4>,NULLARY(b32x4)) \
		visitOp(0xfdba,b32x4_splat,"b32x4.splat",NoImm,UNARY(i32,b32x4)) \
		visitOp(0xfdbb,b32x4_extract_lane,"b32x4.extract_lane",LaneIndexImm<4>,UNARY(b32x4,i32)) \
		visitOp(0xfdbc,b32x4_replace_lane,"b32x4.replace_lane",LaneIndexImm<4>,REPLACELANE(i32,b32x4)) \
		visitOp(0xfdbd,b32x4_and,"b32x4.and",NoImm,BINARY(b32x4,b32x4)) \
		visitOp(0xfdbe,b32x4_or,"b32x4.or",NoImm,BINARY(b32x4,b32x4)) \
		visitOp(0xfdbf,b32x4_xor,"b32x4.xor",NoImm,BINARY(b32x4,b32x4)) \
		visitOp(0xfdc0,b32x4_not,"b32x4.not",NoImm,UNARY(b32x4,b32x4)) \
		visitOp(0xfdc1,b32x4_any_true,"b32x4.any_true",NoImm,UNARY(b32x4,i32)) \
		visitOp(0xfdc2,b32x4_all_true,"b32x4.all_true",NoImm,UNARY(b32x4,i32)) \
		\
		visitOp(0xfdc3,b64x2_const,"b64x2.const",LiteralImm<B64x2>,NULLARY(b64x2)) \
		visitOp(0xfdc4,b64x2_splat,"b64x2.splat",NoImm,UNARY(i32,b64x2)) \
		visitOp(0xfdc5,b64x2_extract_lane,"b64x2.extract_lane",LaneIndexImm<2>,UNARY(b64x2,i32)) \
		visitOp(0xfdc6,b64x2_replace_lane,"b64x2.replace_lane",LaneIndexImm<2>,REPLACELANE(i32,b64x2)) \
		visitOp(0xfdc7,b64x2_and,"b64x2.and",NoImm,BINARY(b64x2,b64x2)) \
		visitOp(0xfdc8,b64x2_or,"b64x2.or",NoImm,BINARY(b64x2,b64x2)) \
		visitOp(0xfdc9,b64x2_xor,"b64x2.xor",NoImm,BINARY(b64x2,b64x2)) \
		visitOp(0xfdca,b64x2_not,"b64x2.not",NoImm,UNARY(b64x2,b64x2)) \
		visitOp(0xfdcb,b64x2_any_true,"b64x2.any_true",NoImm,UNARY(b64x2,i32)) \
		visitOp(0xfdcc,b64x2_all_true,"b64x2.all_true",NoImm,UNARY(b64x2,i32))
	#endif

	#if !ENABLE_THREADING_PROTOTYPE
	#define ENUM_THREADING_OPERATORS(visitOp)
	#else
	#define ENUM_THREADING_OPERATORS(visitOp) \
		visitOp(0xfe00,is_lock_free,"is_lock_free",NoImm,UNARY(i32,i32)) \
		visitOp(0xfe01,wake,"wake",AtomicLoadOrStoreImm<2>,BINARY(i32,i32)) \
		visitOp(0xfe02,i32_wait,"i32.wait",AtomicLoadOrStoreImm<2>,WAIT(i32)) \
		visitOp(0xfe03,i64_wait,"i64.wait",AtomicLoadOrStoreImm<3>,WAIT(i64)) \
		visitOp(0xfe04,launch_thread,"launch_thread",LaunchThreadImm,LAUNCHTHREAD) \
		\
		visitOp(0xfe10,i32_atomic_rmw_xchg,"i32.atomic.rmw.xchg",AtomicLoadOrStoreImm<2>,ATOMICRMW(i32)) \
		visitOp(0xfe11,i64_atomic_rmw_xchg,"i64.atomic.rmw.xchg",AtomicLoadOrStoreImm<3>,ATOMICRMW(i64)) \
		visitOp(0xfe12,i32_atomic_rmw8_s_xchg,"i32.atomic.rmw8_s.xchg",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe13,i32_atomic_rmw8_u_xchg,"i32.atomic.rmw8_u.xchg",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe14,i32_atomic_rmw16_s_xchg,"i32.atomic.rmw16_s.xchg",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe15,i32_atomic_rmw16_u_xchg,"i32.atomic.rmw16_u.xchg",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe16,i64_atomic_rmw8_s_xchg,"i64.atomic.rmw8_s.xchg",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe17,i64_atomic_rmw8_u_xchg,"i64.atomic.rmw8_u.xchg",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe18,i64_atomic_rmw16_s_xchg,"i64.atomic.rmw16_s.xchg",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe19,i64_atomic_rmw16_u_xchg,"i64.atomic.rmw16_u.xchg",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe1a,i64_atomic_rmw32_s_xchg,"i64.atomic.rmw32_s.xchg",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		visitOp(0xfe1b,i64_atomic_rmw32_u_xchg,"i64.atomic.rmw32_u.xchg",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		\
		visitOp(0xfe1c,i32_atomic_rmw_cmpxchg,"i32.atomic.rmw.cmpxchg",AtomicLoadOrStoreImm<2>,COMPAREEXCHANGE(i32)) \
		visitOp(0xfe1d,i64_atomic_rmw_cmpxchg,"i64.atomic.rmw.cmpxchg",AtomicLoadOrStoreImm<3>,COMPAREEXCHANGE(i64)) \
		visitOp(0xfe1e,i32_atomic_rmw8_s_cmpxchg,"i32.atomic.rmw8_s.cmpxchg",AtomicLoadOrStoreImm<0>,COMPAREEXCHANGE(i32)) \
		visitOp(0xfe1f,i32_atomic_rmw8_u_cmpxchg,"i32.atomic.rmw8_u.cmpxchg",AtomicLoadOrStoreImm<0>,COMPAREEXCHANGE(i32)) \
		visitOp(0xfe20,i32_atomic_rmw16_s_cmpxchg,"i32.atomic.rmw16_s.cmpxchg",AtomicLoadOrStoreImm<1>,COMPAREEXCHANGE(i32)) \
		visitOp(0xfe21,i32_atomic_rmw16_u_cmpxchg,"i32.atomic.rmw16_u.cmpxchg",AtomicLoadOrStoreImm<1>,COMPAREEXCHANGE(i32)) \
		visitOp(0xfe22,i64_atomic_rmw8_s_cmpxchg,"i64.atomic.rmw8_s.cmpxchg",AtomicLoadOrStoreImm<0>,COMPAREEXCHANGE(i64)) \
		visitOp(0xfe23,i64_atomic_rmw8_u_cmpxchg,"i64.atomic.rmw8_u.cmpxchg",AtomicLoadOrStoreImm<0>,COMPAREEXCHANGE(i64)) \
		visitOp(0xfe24,i64_atomic_rmw16_s_cmpxchg,"i64.atomic.rmw16_s.cmpxchg",AtomicLoadOrStoreImm<1>,COMPAREEXCHANGE(i64)) \
		visitOp(0xfe25,i64_atomic_rmw16_u_cmpxchg,"i64.atomic.rmw16_u.cmpxchg",AtomicLoadOrStoreImm<1>,COMPAREEXCHANGE(i64)) \
		visitOp(0xfe26,i64_atomic_rmw32_s_cmpxchg,"i64.atomic.rmw32_s.cmpxchg",AtomicLoadOrStoreImm<2>,COMPAREEXCHANGE(i64)) \
		visitOp(0xfe27,i64_atomic_rmw32_u_cmpxchg,"i64.atomic.rmw32_u.cmpxchg",AtomicLoadOrStoreImm<2>,COMPAREEXCHANGE(i64)) \
		\
		visitOp(0xfe28,i32_atomic_load,"i32.atomic.load",AtomicLoadOrStoreImm<2>,LOAD(i32)) \
		visitOp(0xfe29,i64_atomic_load,"i64.atomic.load",AtomicLoadOrStoreImm<3>,LOAD(i64)) \
		visitOp(0xfe2a,f32_atomic_load,"f32.atomic.load",AtomicLoadOrStoreImm<2>,LOAD(f32)) \
		visitOp(0xfe2b,f64_atomic_load,"f64.atomic.load",AtomicLoadOrStoreImm<3>,LOAD(f64)) \
		visitOp(0xfe2c,i32_atomic_load8_s,"i32.atomic.load8_s",AtomicLoadOrStoreImm<0>,LOAD(i32)) \
		visitOp(0xfe2d,i32_atomic_load8_u,"i32.atomic.load8_u",AtomicLoadOrStoreImm<0>,LOAD(i32)) \
		visitOp(0xfe2e,i32_atomic_load16_s,"i32.atomic.load16_s",AtomicLoadOrStoreImm<1>,LOAD(i32)) \
		visitOp(0xfe2f,i32_atomic_load16_u,"i32.atomic.load16_u",AtomicLoadOrStoreImm<1>,LOAD(i32)) \
		visitOp(0xfe30,i64_atomic_load8_s,"i64.atomic.load8_s",AtomicLoadOrStoreImm<0>,LOAD(i64)) \
		visitOp(0xfe31,i64_atomic_load8_u,"i64.atomic.load8_u",AtomicLoadOrStoreImm<0>,LOAD(i64)) \
		visitOp(0xfe32,i64_atomic_load16_s,"i64.atomic.load16_s",AtomicLoadOrStoreImm<1>,LOAD(i64)) \
		visitOp(0xfe33,i64_atomic_load16_u,"i64.atomic.load16_u",AtomicLoadOrStoreImm<1>,LOAD(i64)) \
		visitOp(0xfe34,i64_atomic_load32_s,"i64.atomic.load32_s",AtomicLoadOrStoreImm<2>,LOAD(i64)) \
		visitOp(0xfe35,i64_atomic_load32_u,"i64.atomic.load32_u",AtomicLoadOrStoreImm<2>,LOAD(i64)) \
		visitOp(0xfe36,i32_atomic_store,"i32.atomic.store",AtomicLoadOrStoreImm<2>,STORE(i32)) \
		visitOp(0xfe37,i64_atomic_store,"i64.atomic.store",AtomicLoadOrStoreImm<3>,STORE(i64)) \
		visitOp(0xfe38,f32_atomic_store,"f32.atomic.store",AtomicLoadOrStoreImm<2>,STORE(f32)) \
		visitOp(0xfe39,f64_atomic_store,"f64.atomic.store",AtomicLoadOrStoreImm<3>,STORE(f64)) \
		visitOp(0xfe3a,i32_atomic_store8,"i32.atomic.store8",AtomicLoadOrStoreImm<0>,STORE(i32)) \
		visitOp(0xfe3b,i32_atomic_store16,"i32.atomic.store16",AtomicLoadOrStoreImm<1>,STORE(i32)) \
		visitOp(0xfe3c,i64_atomic_store8,"i64.atomic.store8",AtomicLoadOrStoreImm<0>,STORE(i64)) \
		visitOp(0xfe3d,i64_atomic_store16,"i64.atomic.store16",AtomicLoadOrStoreImm<1>,STORE(i64)) \
		visitOp(0xfe3e,i64_atomic_store32,"i64.atomic.store32",AtomicLoadOrStoreImm<2>,STORE(i64)) \
		\
		visitOp(0xfe3f,i32_atomic_rmw_add,"i32.atomic.rmw.add",AtomicLoadOrStoreImm<2>,ATOMICRMW(i32)) \
		visitOp(0xfe40,i64_atomic_rmw_add,"i64.atomic.rmw.add",AtomicLoadOrStoreImm<3>,ATOMICRMW(i64)) \
		visitOp(0xfe41,i32_atomic_rmw8_s_add,"i32.atomic.rmw8_s.add",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe42,i32_atomic_rmw8_u_add,"i32.atomic.rmw8_u.add",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe43,i32_atomic_rmw16_s_add,"i32.atomic.rmw16_s.add",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe44,i32_atomic_rmw16_u_add,"i32.atomic.rmw16_u.add",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe45,i64_atomic_rmw8_s_add,"i64.atomic.rmw8_s.add",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe46,i64_atomic_rmw8_u_add,"i64.atomic.rmw8_u.add",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe47,i64_atomic_rmw16_s_add,"i64.atomic.rmw16_s.add",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe48,i64_atomic_rmw16_u_add,"i64.atomic.rmw16_u.add",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe49,i64_atomic_rmw32_s_add,"i64.atomic.rmw32_s.add",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		visitOp(0xfe4a,i64_atomic_rmw32_u_add,"i64.atomic.rmw32_u.add",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		\
		visitOp(0xfe4b,i32_atomic_rmw_sub,"i32.atomic.rmw.sub",AtomicLoadOrStoreImm<2>,ATOMICRMW(i32)) \
		visitOp(0xfe4c,i64_atomic_rmw_sub,"i64.atomic.rmw.sub",AtomicLoadOrStoreImm<3>,ATOMICRMW(i64)) \
		visitOp(0xfe4d,i32_atomic_rmw8_s_sub,"i32.atomic.rmw8_s.sub",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe4e,i32_atomic_rmw8_u_sub,"i32.atomic.rmw8_u.sub",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe4f,i32_atomic_rmw16_s_sub,"i32.atomic.rmw16_s.sub",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe50,i32_atomic_rmw16_u_sub,"i32.atomic.rmw16_u.sub",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe51,i64_atomic_rmw8_s_sub,"i64.atomic.rmw8_s.sub",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe52,i64_atomic_rmw8_u_sub,"i64.atomic.rmw8_u.sub",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe53,i64_atomic_rmw16_s_sub,"i64.atomic.rmw16_s.sub",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe54,i64_atomic_rmw16_u_sub,"i64.atomic.rmw16_u.sub",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe55,i64_atomic_rmw32_s_sub,"i64.atomic.rmw32_s.sub",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		visitOp(0xfe56,i64_atomic_rmw32_u_sub,"i64.atomic.rmw32_u.sub",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		\
		visitOp(0xfe57,i32_atomic_rmw_and,"i32.atomic.rmw.and",AtomicLoadOrStoreImm<2>,ATOMICRMW(i32)) \
		visitOp(0xfe58,i64_atomic_rmw_and,"i64.atomic.rmw.and",AtomicLoadOrStoreImm<3>,ATOMICRMW(i64)) \
		visitOp(0xfe59,i32_atomic_rmw8_s_and,"i32.atomic.rmw8_s.and",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe5a,i32_atomic_rmw8_u_and,"i32.atomic.rmw8_u.and",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe5b,i32_atomic_rmw16_s_and,"i32.atomic.rmw16_s.and",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe5c,i32_atomic_rmw16_u_and,"i32.atomic.rmw16_u.and",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe5d,i64_atomic_rmw8_s_and,"i64.atomic.rmw8_s.and",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe5e,i64_atomic_rmw8_u_and,"i64.atomic.rmw8_u.and",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe5f,i64_atomic_rmw16_s_and,"i64.atomic.rmw16_s.and",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe60,i64_atomic_rmw16_u_and,"i64.atomic.rmw16_u.and",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe61,i64_atomic_rmw32_s_and,"i64.atomic.rmw32_s.and",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		visitOp(0xfe62,i64_atomic_rmw32_u_and,"i64.atomic.rmw32_u.and",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		\
		visitOp(0xfe63,i32_atomic_rmw_or,"i32.atomic.rmw.or",AtomicLoadOrStoreImm<2>,ATOMICRMW(i32)) \
		visitOp(0xfe64,i64_atomic_rmw_or,"i64.atomic.rmw.or",AtomicLoadOrStoreImm<3>,ATOMICRMW(i64)) \
		visitOp(0xfe65,i32_atomic_rmw8_s_or,"i32.atomic.rmw8_s.or",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe66,i32_atomic_rmw8_u_or,"i32.atomic.rmw8_u.or",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe67,i32_atomic_rmw16_s_or,"i32.atomic.rmw16_s.or",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe68,i32_atomic_rmw16_u_or,"i32.atomic.rmw16_u.or",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe69,i64_atomic_rmw8_s_or,"i64.atomic.rmw8_s.or",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe6a,i64_atomic_rmw8_u_or,"i64.atomic.rmw8_u.or",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe6b,i64_atomic_rmw16_s_or,"i64.atomic.rmw16_s.or",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe6c,i64_atomic_rmw16_u_or,"i64.atomic.rmw16_u.or",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe6d,i64_atomic_rmw32_s_or,"i64.atomic.rmw32_s.or",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		visitOp(0xfe6e,i64_atomic_rmw32_u_or,"i64.atomic.rmw32_u.or",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		\
		visitOp(0xfe6f,i32_atomic_rmw_xor,"i32.atomic.rmw.xor",AtomicLoadOrStoreImm<2>,ATOMICRMW(i32)) \
		visitOp(0xfe70,i64_atomic_rmw_xor,"i64.atomic.rmw.xor",AtomicLoadOrStoreImm<3>,ATOMICRMW(i64)) \
		visitOp(0xfe71,i32_atomic_rmw8_s_xor,"i32.atomic.rmw8_s.xor",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe72,i32_atomic_rmw8_u_xor,"i32.atomic.rmw8_u.xor",AtomicLoadOrStoreImm<0>,ATOMICRMW(i32)) \
		visitOp(0xfe73,i32_atomic_rmw16_s_xor,"i32.atomic.rmw16_s.xor",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe74,i32_atomic_rmw16_u_xor,"i32.atomic.rmw16_u.xor",AtomicLoadOrStoreImm<1>,ATOMICRMW(i32)) \
		visitOp(0xfe75,i64_atomic_rmw8_s_xor,"i64.atomic.rmw8_s.xor",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe76,i64_atomic_rmw8_u_xor,"i64.atomic.rmw8_u.xor",AtomicLoadOrStoreImm<0>,ATOMICRMW(i64)) \
		visitOp(0xfe77,i64_atomic_rmw16_s_xor,"i64.atomic.rmw16_s.xor",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe78,i64_atomic_rmw16_u_xor,"i64.atomic.rmw16_u.xor",AtomicLoadOrStoreImm<1>,ATOMICRMW(i64)) \
		visitOp(0xfe79,i64_atomic_rmw32_s_xor,"i64.atomic.rmw32_s.xor",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64)) \
		visitOp(0xfe7a,i64_atomic_rmw32_u_xor,"i64.atomic.rmw32_u.xor",AtomicLoadOrStoreImm<2>,ATOMICRMW(i64))
	#endif

	#define ENUM_NONCONTROL_OPERATORS(visitOp) \
		ENUM_PARAMETRIC_OPERATORS(visitOp) \
		ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(visitOp)

	#define ENUM_OPERATORS(visitOp) \
		ENUM_NONCONTROL_OPERATORS(visitOp) \
		ENUM_CONTROL_OPERATORS(visitOp)

	enum class Opcode : U16
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
		OperatorDecoderStream(const std::vector<U8>& codeBytes)
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