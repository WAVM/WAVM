#include "WebAssemblyTextSymbols.h"
#include "Core/SExpressions.h"

namespace WebAssemblyText
{
	#define I8LowerCaseString "i8"
	#define I16LowerCaseString "i16"
	#define I32LowerCaseString "i32"
	#define I64LowerCaseString "i64"
	#define F32LowerCaseString "f32"
	#define F64LowerCaseString "f64"
	#define VoidLowerCaseString "void"

	// Declare an array, indexed by the symbol enum, containing the symbol string.
	const char* wastSymbols[(size_t)Symbol::num] =
	{
		#define AST_TYPE(typeName,className,symbol) typeName##LowerCaseString "." #symbol,
		#define AST_TYPE_PAIR(typeName1,typeName2,symbol) typeName1##LowerCaseString "." #symbol "/" typeName2##LowerCaseString,
		#define ALIGNED_AST_TYPE(typeName,className,symbol) \
			typeName##LowerCaseString "." #symbol, \
			typeName##LowerCaseString "." #symbol "/1", \
			typeName##LowerCaseString "." #symbol "/2", \
			typeName##LowerCaseString "." #symbol "/4", \
			typeName##LowerCaseString "." #symbol "/8", \
			typeName##LowerCaseString "." #symbol "/16", \
			typeName##LowerCaseString "." #symbol "/32", \
			typeName##LowerCaseString "." #symbol "/64", \
			typeName##LowerCaseString "." #symbol "/128", \
			typeName##LowerCaseString "." #symbol "/256",
		#define WAST_SYMBOL(symbol) #symbol ,
		#define TYPED_WAST_SYMBOL(symbol) #symbol , ENUM_AST_TYPES(AST_TYPE,symbol)
		#define ALIGNED_TYPED_WAST_SYMBOL(symbol) #symbol , ENUM_AST_TYPES(ALIGNED_AST_TYPE,symbol)
		#define BITYPED_WAST_SYMBOL(symbol) #symbol , ENUM_AST_TYPE_PAIRS(AST_TYPE_PAIR,symbol)
		ENUM_OPCODE_SYMBOLS()
		#undef AST_TYPE
		#undef AST_TYPE_PAIR
		#undef ALIGNED_AST_TYPE
		#undef WAST_SYMBOL
		#undef TYPED_WAST_SYMBOL
		#undef ALIGNED_TYPED_WAST_SYMBOL
		#undef BITYPED_WAST_SYMBOL
	};

	const SExp::SymbolIndexMap& getWASTSymbolIndexMap()
	{
		static bool isInitialized = false;
		static SExp::SymbolIndexMap symbolIndexMap;
		if(!isInitialized)
		{
			auto numSymbols = sizeof(wastSymbols) / sizeof(wastSymbols[0]);
			for(uintptr symbolIndex = 0;symbolIndex < numSymbols;++symbolIndex) { symbolIndexMap[wastSymbols[symbolIndex]] = symbolIndex; }
			isInitialized = true;
		}
		return symbolIndexMap;
	}
}
