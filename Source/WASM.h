#pragma once

#include <cstdint>
#include <cassert>
#include <vector>

namespace WASM
{

	// =================================================================================================
	// Magic serialization constants

	static const uint32_t MagicNumber = 0x6d736177;

	enum class Stmt : uint8_t
	{
		SetLoc,
		SetGlo,
		I32Store8,
		I32StoreOff8,
		I32Store16,
		I32StoreOff16,
		I32Store32,
		I32StoreOff32,
		F32Store,
		F32StoreOff,
		F64Store,
		F64StoreOff,
		CallInt,
		CallInd,
		CallImp,
		Ret,
		Block,
		IfThen,
		IfElse,
		While,
		Do,
		Label,
		Break,
		BreakLabel,
		Continue,
		ContinueLabel,
		Switch,

		Bad
	};

	enum class StmtWithImm : uint8_t
	{
		SetLoc,
		SetGlo,
		Reseved1,
		Reseved2,

		Bad
	};

	enum class SwitchCase : uint8_t
	{
		Case0,
		Case1,
		CaseN,
		Default0,
		Default1,
		DefaultN
	};

	enum class I32 : uint8_t
	{
		LitPool,
		LitImm,
		GetLoc,
		GetGlo,
		SetLoc,
		SetGlo,
		SLoad8,
		SLoadOff8,
		ULoad8,
		ULoadOff8,
		SLoad16,
		SLoadOff16,
		ULoad16,
		ULoadOff16,
		Load32,
		LoadOff32,
		Store8,
		StoreOff8,
		Store16,
		StoreOff16,
		Store32,
		StoreOff32,
		CallInt,
		CallInd,
		CallImp,
		Cond,
		Comma,
		FromF32,
		FromF64,
		Neg,
		Add,
		Sub,
		Mul,
		SDiv,
		UDiv,
		SMod,
		UMod,
		BitNot,
		BitOr,
		BitAnd,
		BitXor,
		Lsh,
		ArithRsh,
		LogicRsh,
		Clz,
		LogicNot,
		EqI32,
		EqF32,
		EqF64,
		NEqI32,
		NEqF32,
		NEqF64,
		SLeThI32,
		ULeThI32,
		LeThF32,
		LeThF64,
		SLeEqI32,
		ULeEqI32,
		LeEqF32,
		LeEqF64,
		SGrThI32,
		UGrThI32,
		GrThF32,
		GrThF64,
		SGrEqI32,
		UGrEqI32,
		GrEqF32,
		GrEqF64,
		SMin,
		UMin,
		SMax,
		UMax,
		Abs,

		Bad
	};

	enum class I32WithImm : uint8_t
	{
		LitPool,
		LitImm,
		GetLoc,
		Reserved,

		Bad
	};

	enum class F32 : uint8_t
	{
		LitPool,
		LitImm,
		GetLoc,
		GetGlo,
		SetLoc,
		SetGlo,
		Load,
		LoadOff,
		Store,
		StoreOff,
		CallInt,
		CallInd,
		Cond,
		Comma,
		FromS32,
		FromU32,
		FromF64,
		Neg,
		Add,
		Sub,
		Mul,
		Div,
		Abs,
		Ceil,
		Floor,
		Sqrt,

		Bad
	};

	enum class F32WithImm : uint8_t
	{
		LitPool,
		GetLoc,
		Reserved0,
		Reserved1,

		Bad
	};

	enum class F64 : uint8_t
	{
		LitPool,
		LitImm,
		GetLoc,
		GetGlo,
		SetLoc,
		SetGlo,
		Load,
		LoadOff,
		Store,
		StoreOff,
		CallInt,
		CallInd,
		CallImp,
		Cond,
		Comma,
		FromS32,
		FromU32,
		FromF32,
		Neg,
		Add,
		Sub,
		Mul,
		Div,
		Mod,
		Min,
		Max,
		Abs,
		Ceil,
		Floor,
		Sqrt,
		Cos,
		Sin,
		Tan,
		ACos,
		ASin,
		ATan,
		ATan2,
		Exp,
		Ln,
		Pow,

		Bad
	};

	enum class F64WithImm : uint8_t
	{
		LitPool,
		GetLoc,
		Reserved0,
		Reserved1,

		Bad
	};

	enum class Void : uint8_t
	{
		CallInt,
		CallInd,
		CallImp,

		Bad
	};

	enum class Type : uint8_t
	{
		I32,
		F32,
		F64
	};

	enum class VaReturnTypes : uint8_t
	{
		I32 = 0x1,
		F32 = 0x2,
		F64 = 0x4,
	};

	inline VaReturnTypes operator|(VaReturnTypes lhs,VaReturnTypes rhs) { return VaReturnTypes(uint8_t(lhs) | uint8_t(rhs)); }
	inline bool operator&(VaReturnTypes lhs,VaReturnTypes rhs) { return bool((uint8_t(lhs) & uint8_t(rhs)) != 0); }

	enum class VaReturnTypesWithImm : uint8_t
	{
		OnlyI32,
		Reserved0,
		Reserved1,
		Reserved2
	};

	enum class ReturnType : uint8_t
	{
		I32 = uint8_t(Type::I32),
		F32 = uint8_t(Type::F32),
		F64 = uint8_t(Type::F64),
		Void
	};

	inline ReturnType to_ReturnType(Type t)
	{
		return ReturnType(t);
	}

	static const uint8_t HasImmFlag = 0x80;
	static_assert(uint8_t(I32::Bad) <= HasImmFlag,"MSB reserved to distinguish I32 from I32WithImm");
	static_assert(uint8_t(F32::Bad) <= HasImmFlag,"MSB reserved to distinguish F32 from F32WithImm");
	static_assert(uint8_t(F64::Bad) <= HasImmFlag,"MSB reserved to distinguish F64 from F64WithImm");

	static const unsigned OpWithImmBits = 2;
	static const uint32_t OpWithImmLimit = 1 << OpWithImmBits;
	static_assert(uint8_t(I32WithImm::Bad) <= OpWithImmLimit,"I32WithImm op fits");
	static_assert(uint8_t(F32WithImm::Bad) <= OpWithImmLimit,"F32WithImm op fits");
	static_assert(uint8_t(F64WithImm::Bad) <= OpWithImmLimit,"F64WithImm op fits");

	static const unsigned ImmBits = 5;
	static const uint32_t ImmLimit = 1 << ImmBits;
	static_assert(1 + OpWithImmBits + ImmBits == 8,"Bits of immediate op should add up to a byte");

	static inline uint8_t PackOpWithImm(uint8_t op,uint8_t imm)
	{
		assert(op < OpWithImmLimit);
		assert(imm < ImmLimit);
		return HasImmFlag | (uint8_t(op) << ImmBits) | imm;
	}

	template <class TWithImm>
	static inline void UnpackOpWithImm(uint8_t byte,TWithImm* op,uint8_t *imm)
	{
		assert(byte & HasImmFlag);
		*op = TWithImm((byte >> ImmBits) & (OpWithImmLimit - 1));
		*imm = byte & (ImmLimit - 1);
	}

	enum class ExportFormat : uint8_t
	{
		Default,
		Record
	};
}
