#pragma once

#include <cstdint>

#include "ASTTypes.h"

namespace AST
{
	#define ENUM_AST_OPS_Any() \
		AST_OP(error) \
		AST_OP(getLocal) AST_OP(loadGlobal) \
		AST_OP(callDirect) AST_OP(callImport) AST_OP(callIndirect) \
		AST_OP(loop) AST_OP(switch_) AST_OP(ifElse) AST_OP(label) AST_OP(sequence) \
		AST_OP(branch) AST_OP(ret)

	#define ENUM_AST_UNARY_OPS_Int() \
		AST_OP(neg) \
		AST_OP(abs) \
		AST_OP(not) \
		AST_OP(clz) \
		AST_OP(ctz) \
		AST_OP(popcnt)

	#define ENUM_AST_BINARY_OPS_Int() \
		AST_OP(add) \
		AST_OP(sub) \
		AST_OP(mul) \
		AST_OP(divs) \
		AST_OP(divu) \
		AST_OP(rems) \
		AST_OP(remu) \
		AST_OP(mins) \
		AST_OP(minu) \
		AST_OP(maxs) \
		AST_OP(maxu) \
		AST_OP(and) \
		AST_OP(or) \
		AST_OP(xor) \
		AST_OP(shl) \
		AST_OP(shr) \
		AST_OP(sar)

	#define ENUM_AST_CAST_OPS_Int() \
		AST_OP(wrap) \
		AST_OP(truncSignedFloat) \
		AST_OP(truncUnsignedFloat) \
		AST_OP(sext) \
		AST_OP(zext) \
		AST_OP(reinterpretFloat) \
		AST_OP(reinterpretBool)

	#define ENUM_AST_OPS_Int() \
		ENUM_AST_OPS_Any() \
		ENUM_AST_UNARY_OPS_Int() \
		ENUM_AST_BINARY_OPS_Int() \
		ENUM_AST_CAST_OPS_Int() \
		AST_OP(lit) \
		AST_OP(loadMemory)

	#define ENUM_AST_UNARY_OPS_Float() \
		AST_OP(neg) \
		AST_OP(abs) \
		AST_OP(ceil) \
		AST_OP(floor) \
		AST_OP(trunc) \
		AST_OP(nearestInt) \
		AST_OP(cos) \
		AST_OP(sin) \
		AST_OP(sqrt) \
		AST_OP(exp) \
		AST_OP(log)

	#define ENUM_AST_BINARY_OPS_Float() \
		AST_OP(pow) \
		AST_OP(add) \
		AST_OP(sub) \
		AST_OP(mul) \
		AST_OP(div) \
		AST_OP(rem) \
		AST_OP(min) \
		AST_OP(max) \
		AST_OP(copySign)

	#define ENUM_AST_CAST_OPS_Float() \
		AST_OP(convertSignedInt) \
		AST_OP(convertUnsignedInt) \
		AST_OP(promote) \
		AST_OP(demote) \
		AST_OP(reinterpretInt)

	#define ENUM_AST_OPS_Float() \
		ENUM_AST_OPS_Any() \
		ENUM_AST_UNARY_OPS_Float() \
		ENUM_AST_BINARY_OPS_Float() \
		ENUM_AST_CAST_OPS_Float() \
		AST_OP(lit) \
		AST_OP(loadMemory)

	#define ENUM_AST_UNARY_OPS_Bool() \
		AST_OP(not)

	#define ENUM_AST_BINARY_OPS_Bool() \
		AST_OP(and) \
		AST_OP(or)

	#define ENUM_AST_COMPARISON_OPS() \
		AST_OP(eq) AST_OP(neq) \
		AST_OP(lts) AST_OP(ltu) AST_OP(lt) \
		AST_OP(les) AST_OP(leu) AST_OP(le) \
		AST_OP(gts) AST_OP(gtu) AST_OP(gt) \
		AST_OP(ges) AST_OP(geu) AST_OP(ge)

	#define ENUM_AST_OPS_Bool() \
		ENUM_AST_OPS_Any() \
		ENUM_AST_UNARY_OPS_Bool() \
		ENUM_AST_BINARY_OPS_Bool() \
		ENUM_AST_COMPARISON_OPS() \
		AST_OP(lit)

	#define ENUM_AST_OPS_Void() \
		ENUM_AST_OPS_Any() \
		AST_OP(setLocal) AST_OP(storeGlobal) AST_OP(storeMemory) \
		AST_OP(discardResult) AST_OP(nop)

	// Define the ClassOp enums: AnyOp, IntOp, etc.
	#define AST_OP(op) op,
	enum class AnyOp : uint8_t		{ ENUM_AST_OPS_Any() };
	enum class IntOp : uint8_t		{ ENUM_AST_OPS_Int() };
	enum class FloatOp : uint8_t	{ ENUM_AST_OPS_Float() };
	enum class BoolOp : uint8_t	{ ENUM_AST_OPS_Bool() };
	enum class VoidOp : uint8_t	{ ENUM_AST_OPS_Void() };
	#undef AST_OP
	
	// Used to represent an opcode as a type for compile-time specialization.
	template<typename Class> struct OpTypes;
	#define AST_OP(op) struct op {};
	#define AST_TYPECLASS(className) template<> struct OpTypes<className##Class> { ENUM_AST_OPS_##className() };
	ENUM_AST_TYPECLASSES()
	#undef AST_OP
	#undef AST_TYPECLASS

	// Define the getOpName function for each ClassOp enum.
	#define AST_TYPECLASS(className) const char* getOpName(className##Op op);
	ENUM_AST_TYPECLASSES()
	#undef AST_TYPECLASS
}