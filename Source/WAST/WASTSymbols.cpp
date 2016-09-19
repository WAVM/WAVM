#include "WAST.h"
#include "WASTSymbols.h"
#include "Core/SExpressions.h"

namespace WAST
{

	// Declare an array, indexed by the symbol enum, containing the symbol string.
	const char* wastSymbols[(size_t)Symbol::num] =
	{
		#define WAST_SYMBOL(symbol) #symbol,
		#define WAST_INT_OPCODE_SYMBOL(opcode) "i32." #opcode, "i64." #opcode,
		#define WAST_FLOAT_OPCODE_SYMBOL(opcode) "f32." #opcode, "f64." #opcode,
		#define WAST_NUM_OPCODE_SYMBOL(opcode) WAST_INT_OPCODE_SYMBOL(opcode) WAST_FLOAT_OPCODE_SYMBOL(opcode)
		#define WAST_CONVERSION_OPCODE_SYMBOL(destType,opcode,sourceType) #destType "." #opcode "/" #sourceType,
		ENUM_WAST_SYMBOLS()
	};

	const SExp::SymbolIndexMap& getWASTSymbolIndexMap()
	{
		static bool isInitialized = false;
		static SExp::SymbolIndexMap symbolIndexMap;
		if(!isInitialized)
		{
			auto numSymbols = sizeof(wastSymbols) / sizeof(wastSymbols[0]);
			for(uintp symbolIndex = 0;symbolIndex < numSymbols;++symbolIndex) { symbolIndexMap.emplace(std::make_pair(wastSymbols[symbolIndex], symbolIndex)); }
			isInitialized = true;
		}
		return symbolIndexMap;
	}
}
