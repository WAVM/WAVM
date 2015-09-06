#include "WebAssemblyTextSymbols.h"
#include "SExpressions.h"

namespace WebAssemblyText
{
	// Declare an array, indexed by the symbol enum, containing the symbol string.
	const char* wastSymbols[Symbols::num] =
	{
		#define AST_TYPE(typeName,className,symbol) #typeName "." #symbol,
		#define AST_TYPE_PAIR(typeName1,typeName2,symbol) #typeName1 "." #symbol "/" #typeName2,
		#define WAST_SYMBOL(symbol) #symbol ,
		#define TYPED_WAST_SYMBOL(symbol) #symbol , ENUM_AST_TYPES(AST_TYPE,symbol)
		#define BITYPED_WAST_SYMBOL(symbol) #symbol , ENUM_AST_TYPE_PAIRS(AST_TYPE_PAIR,symbol)
		ENUM_OPCODE_SYMBOLS()
		#undef AST_TYPE
		#undef AST_TYPE_PAIR
		#undef WAST_SYMBOL
		#undef TYPED_WAST_SYMBOL
		#undef BITYPED_WAST_SYMBOL
	};

	const SExp::SymbolIndexMap& getWASTSymbolIndexMap()
	{
		static bool isInitialized = false;
		static SExp::SymbolIndexMap symbolIndexMap;
		if(!isInitialized)
		{
			auto numSymbols = sizeof(wastSymbols) / sizeof(wastSymbols[0]);
			for(uintptr_t symbolIndex = 0;symbolIndex < numSymbols;++symbolIndex) { symbolIndexMap[wastSymbols[symbolIndex]] = symbolIndex; }
			isInitialized = true;
		}
		return symbolIndexMap;
	}
}