#pragma once

#include "AST/ASTTypes.h"
#include "SExpressions.h"

#include <cstdint>

namespace WebAssemblyText
{
	#define ENUM_WAST_DECL_SYMBOLS() \
		WAST_SYMBOL(module) \
		WAST_SYMBOL(func) \
		WAST_SYMBOL(global) \
		WAST_SYMBOL(table) \
		WAST_SYMBOL(export) \
		WAST_SYMBOL(import) \
		WAST_SYMBOL(memory) \
		WAST_SYMBOL(segment) \
		WAST_SYMBOL(param) \
		WAST_SYMBOL(result) \
		WAST_SYMBOL(local) \
		WAST_SYMBOL(case) \
		WAST_SYMBOL(fallthrough) \
		WAST_SYMBOL(assert_eq) \
		WAST_SYMBOL(assert_invalid) \
		WAST_SYMBOL(invoke)
	
	#define ENUM_WAST_ANY_OPCODE_SYMBOLS() \
		TYPED_WAST_SYMBOL(switch) \
		WAST_SYMBOL(call) \
		WAST_SYMBOL(call_indirect) \
		WAST_SYMBOL(if) \
		WAST_SYMBOL(loop) \
		WAST_SYMBOL(break) \
		WAST_SYMBOL(label) \
		WAST_SYMBOL(return) \
		WAST_SYMBOL(block) \
		WAST_SYMBOL(nop) \
		WAST_SYMBOL(get_local) \
		WAST_SYMBOL(load_global) \

	#define ENUM_WAST_NUMERIC_OPCODE_SYMBOLS() \
		TYPED_WAST_SYMBOL(const) \
		TYPED_WAST_SYMBOL(neg) \
		TYPED_WAST_SYMBOL(abs) \
		TYPED_WAST_SYMBOL(sub) \
		TYPED_WAST_SYMBOL(add) \
		TYPED_WAST_SYMBOL(mul) \
		BITYPED_WAST_SYMBOL(wrap) \
		BITYPED_WAST_SYMBOL(trunc_s) \
		BITYPED_WAST_SYMBOL(trunc_u) \
		BITYPED_WAST_SYMBOL(extend_s) \
		BITYPED_WAST_SYMBOL(extend_u) \
		BITYPED_WAST_SYMBOL(reinterpret)

	#define ENUM_WAST_INT_OPCODE_SYMBOLS() \
		BITYPED_WAST_SYMBOL(load_s) \
		BITYPED_WAST_SYMBOL(load_u) \
		TYPED_WAST_SYMBOL(not) \
		TYPED_WAST_SYMBOL(clz) \
		TYPED_WAST_SYMBOL(ctz) \
		TYPED_WAST_SYMBOL(popcnt) \
		TYPED_WAST_SYMBOL(div_s) \
		TYPED_WAST_SYMBOL(div_u) \
		TYPED_WAST_SYMBOL(rem_s) \
		TYPED_WAST_SYMBOL(rem_u) \
		TYPED_WAST_SYMBOL(and) \
		TYPED_WAST_SYMBOL(or) \
		TYPED_WAST_SYMBOL(xor) \
		TYPED_WAST_SYMBOL(shl) \
		TYPED_WAST_SYMBOL(shr) \
		TYPED_WAST_SYMBOL(sar)

	#define ENUM_WAST_FLOAT_OPCODE_SYMBOLS() \
		BITYPED_WAST_SYMBOL(load) \
		TYPED_WAST_SYMBOL(ceil) \
		TYPED_WAST_SYMBOL(floor) \
		TYPED_WAST_SYMBOL(trunc) \
		TYPED_WAST_SYMBOL(nearest) \
		TYPED_WAST_SYMBOL(div) \
		TYPED_WAST_SYMBOL(rem) \
		TYPED_WAST_SYMBOL(copysign) \
		TYPED_WAST_SYMBOL(min) \
		TYPED_WAST_SYMBOL(max) \
		BITYPED_WAST_SYMBOL(promote) \
		BITYPED_WAST_SYMBOL(demote) \
		BITYPED_WAST_SYMBOL(convert_s) \
		BITYPED_WAST_SYMBOL(convert_u)

	#define ENUM_WAST_BOOL_OPCODE_SYMBOLS() \
		TYPED_WAST_SYMBOL(eq) \
		TYPED_WAST_SYMBOL(neq) \
		TYPED_WAST_SYMBOL(lt_s) \
		TYPED_WAST_SYMBOL(lt_u) \
		TYPED_WAST_SYMBOL(le_s) \
		TYPED_WAST_SYMBOL(le_u) \
		TYPED_WAST_SYMBOL(gt_s) \
		TYPED_WAST_SYMBOL(gt_u) \
		TYPED_WAST_SYMBOL(ge_s) \
		TYPED_WAST_SYMBOL(ge_u) \
		TYPED_WAST_SYMBOL(lt) \
		TYPED_WAST_SYMBOL(le) \
		TYPED_WAST_SYMBOL(gt) \
		TYPED_WAST_SYMBOL(ge)
	
	#define ENUM_WAST_VOID_OPCODE_SYMBOLS() \
		WAST_SYMBOL(set_local) \
		WAST_SYMBOL(store_global) \
		BITYPED_WAST_SYMBOL(store_s) \
		BITYPED_WAST_SYMBOL(store_u) \
		BITYPED_WAST_SYMBOL(store)

	#define ENUM_WAST_TYPE_SYMBOLS() \
		WAST_SYMBOL(typeBase) \
		WAST_SYMBOL(i8) \
		WAST_SYMBOL(i16) \
		WAST_SYMBOL(i32) \
		WAST_SYMBOL(i64) \
		WAST_SYMBOL(f32) \
		WAST_SYMBOL(f64) \
		WAST_SYMBOL(bool) \
		WAST_SYMBOL(void)

	#define ENUM_OPCODE_SYMBOLS() \
		ENUM_WAST_DECL_SYMBOLS() \
		ENUM_WAST_ANY_OPCODE_SYMBOLS() \
		ENUM_WAST_NUMERIC_OPCODE_SYMBOLS() \
		ENUM_WAST_INT_OPCODE_SYMBOLS() \
		ENUM_WAST_FLOAT_OPCODE_SYMBOLS() \
		ENUM_WAST_BOOL_OPCODE_SYMBOLS() \
		ENUM_WAST_VOID_OPCODE_SYMBOLS() \
		ENUM_WAST_TYPE_SYMBOLS()

	// Declare an enum with all the symbols used by WAST.
	namespace Symbols
	{
		enum Enum : uintptr_t
		{
			#define AST_TYPE(typeName,className,symbol) _##symbol##_##typeName,
			#define AST_TYPE_PAIR(typeName1,typeName2,symbol) _##symbol##_##typeName1##_##typeName2,
			#define WAST_SYMBOL(symbol) _##symbol,
			#define TYPED_WAST_SYMBOL(symbol) _##symbol, ENUM_AST_TYPES(AST_TYPE,symbol)
			#define BITYPED_WAST_SYMBOL(symbol) _##symbol, ENUM_AST_TYPE_PAIRS(AST_TYPE_PAIR,symbol)
			ENUM_OPCODE_SYMBOLS()
			#undef AST_TYPE
			#undef AST_TYPE_PAIR
			#undef WAST_SYMBOL
			#undef TYPED_WAST_SYMBOL
			#undef BITYPED_WAST_SYMBOL
			num
		};
	}
	typedef Symbols::Enum Symbol;

	// Declare an array, indexed by the symbol enum, containing the symbol string.
	extern const char* wastSymbols[Symbols::num];
	
	// Encapsulates static initialization of the WAST symbol index map.
	const SExp::SymbolIndexMap& getWASTSymbolIndexMap();
}