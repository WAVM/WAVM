#include "Core/Core.h"
#include "WAST.h"
#include "Lexer.h"
#include "IR/Module.h"
#include "Parse.h"

#include <cstdarg>
#include <cstdio>

#define XXH_FORCE_NATIVE_FORMAT 1
#define XXH_PRIVATE_API
#include "../ThirdParty/xxhash/xxhash.h"

using namespace IR;

namespace WAST
{
	void findClosingParenthesis(ParseState& state,const Token* openingParenthesisToken)
	{
		// Skip over tokens until the ')' closing the current parentheses nesting depth is found.
		uintp depth = 1;
		while(depth > 0)
		{
			switch(state.nextToken->type)
			{
			default:
				++state.nextToken;
				break;
			case t_leftParenthesis:
				++state.nextToken;
				++depth;
				break;
			case t_rightParenthesis:
				++state.nextToken;
				--depth;
				break;
			case t_eof:
				parseErrorf(state,openingParenthesisToken,"reached end of input while trying to find closing parenthesis");
				throw FatalParseException();
			}
		}
	}

	void parseErrorf(ParseState& state,uintp charOffset,const char* messageFormat,va_list messageArguments)
	{
		// Format the message.
		char messageBuffer[1024];
		int numPrintedChars = std::vsnprintf(messageBuffer,sizeof(messageBuffer),messageFormat,messageArguments);
		if(numPrintedChars >= 1023 || numPrintedChars < 0) { Core::unreachable(); }
		messageBuffer[numPrintedChars] = 0;

		// Add the error to the state's error list.
		state.errors.emplace_back(charOffset,messageBuffer);
	}
	void parseErrorf(ParseState& state,uintp charOffset,const char* messageFormat,...)
	{
		va_list messageArguments;
		va_start(messageArguments,messageFormat);
		parseErrorf(state,charOffset,messageFormat,messageArguments);
		va_end(messageArguments);
	}
	void parseErrorf(ParseState& state,const char* nextChar,const char* messageFormat,...)
	{
		va_list messageArguments;
		va_start(messageArguments,messageFormat);
		parseErrorf(state,nextChar - state.string,messageFormat,messageArguments);
		va_end(messageArguments);
	}
	void parseErrorf(ParseState& state,const Token* nextToken,const char* messageFormat,...)
	{
		va_list messageArguments;
		va_start(messageArguments,messageFormat);
		parseErrorf(state,nextToken->begin,messageFormat,messageArguments);
		va_end(messageArguments);
	}

	void require(ParseState& state,TokenType type)
	{
		if(state.nextToken->type != type)
		{
			parseErrorf(state,state.nextToken,"expected %s",describeToken(type));
			throw RecoverParseException();
		}
		++state.nextToken;
	}
	
	bool tryParseValueType(ParseState& state,ValueType& outValueType)
	{
		switch(state.nextToken->type)
		{
		case t_i32: ++state.nextToken; outValueType = ValueType::i32; return true;
		case t_i64: ++state.nextToken; outValueType = ValueType::i64; return true;
		case t_f32: ++state.nextToken; outValueType = ValueType::f32; return true;
		case t_f64: ++state.nextToken; outValueType = ValueType::f64; return true;
		#if ENABLE_SIMD_PROTOTYPE
		case t_v128: ++state.nextToken; outValueType = ValueType::v128; return true;
		case t_b8x16: ++state.nextToken; outValueType = ValueType::b8x16; return true;
		case t_b16x8: ++state.nextToken; outValueType = ValueType::b16x8; return true;
		case t_b32x4: ++state.nextToken; outValueType = ValueType::b32x4; return true;
		case t_b64x2: ++state.nextToken; outValueType = ValueType::b64x2; return true;
		#endif
		default:
			outValueType = ValueType::any;
			return false;
		};
	}
	
	bool tryParseResultType(ParseState& state,ResultType& outResultType)
	{
		return tryParseValueType(state,*(ValueType*)&outResultType);
	}

	ValueType parseValueType(ParseState& state)
	{
		ValueType result;
		if(!tryParseValueType(state,result))
		{
			parseErrorf(state,state.nextToken,"expected value type");
			throw RecoverParseException();
		}
		return result;
	}

	const FunctionType* parseFunctionType(ModuleParseState& state,NameToIndexMap& outLocalNameToIndexMap,std::vector<std::string>& outLocalDisassemblyNames)
	{
		std::vector<ValueType> parameters;
		ResultType ret = ResultType::none;

		// Parse the function parameters.
		while(tryParseParenthesizedTagged(state,t_param,[&]
		{
			Name parameterName;
			if(tryParseName(state,parameterName))
			{
				// (param <name> <type>)
				bindName(state,outLocalNameToIndexMap,parameterName,parameters.size());
				parameters.push_back(parseValueType(state));
				outLocalDisassemblyNames.push_back(parameterName.getString());
			}
			else
			{
				// (param <type>*)
				ValueType parameterType;
				while(tryParseValueType(state,parameterType))
				{
					parameters.push_back(parameterType);
					outLocalDisassemblyNames.push_back(std::string());
				};
			}
		}));

		// Parse an optional result type.
		tryParseParenthesizedTagged(state,t_result,[&]
		{
			if(!tryParseResultType(state,ret))
			{
				parseErrorf(state,state.nextToken,"expected result type");
				throw RecoverParseException();
			}
		});

		return FunctionType::get(ret,parameters);
	}
	
	IndexedFunctionType parseFunctionTypeRef(ModuleParseState& state)
	{
		if(state.nextToken[0].type == t_leftParenthesis
		&& state.nextToken[1].type == t_type)
		{
			// Parse a reference by name or index to some type in the module's type table.
			IndexedFunctionType result = {UINTPTR_MAX};
			parseParenthesized(state,[&]
			{
				require(state,t_type);

				const Token* refToken = state.nextToken;
				result.index = parseAndResolveNameOrIndexRef(state,state.typeNameToIndexMap,"type");

				// Callers may use the returned index to look up a FunctionType from the module's type table,
				// so make sure that the index is valid.
				if(result.index != UINTPTR_MAX && result.index >= state.module.types.size())
				{
					parseErrorf(state,refToken,"invalid type reference");
					result = getUniqueFunctionTypeIndex(state,FunctionType::get());
				}
			});
			assert(result.index < state.module.types.size());
			return result;
		}
		else
		{
			// Parse an inline function type: (param ...)* (result ...)?
			NameToIndexMap parameterNameToIndexMap;
			std::vector<std::string> localDisassemblyNames;
			return getUniqueFunctionTypeIndex(state,parseFunctionType(state,parameterNameToIndexMap,localDisassemblyNames));
		}
	}

	IndexedFunctionType getUniqueFunctionTypeIndex(ModuleParseState& state,const FunctionType* functionType)
	{
		// If this type is not in the module's type table yet, add it.
		auto functionTypeToIndexMapIt = state.functionTypeToIndexMap.find(functionType);
		if(functionTypeToIndexMapIt != state.functionTypeToIndexMap.end())
		{
			return IndexedFunctionType {functionTypeToIndexMapIt->second};
		}
		else
		{
			const uintp functionTypeIndex = state.module.types.size();
			state.module.types.push_back(functionType);
			state.disassemblyNames.types.push_back(std::string());
			state.functionTypeToIndexMap.emplace(functionType,functionTypeIndex);
			return IndexedFunctionType {functionTypeIndex};
		}
	}

	uint32 Name::calcHash(const char* begin,uint32 numChars)
	{
		// Use xxHash32 to hash names. xxHash64 is theoretically faster for long strings on 64-bit machines,
		// but I did not find it to be faster for typical name lengths.
		return XXH32(begin,numChars,0);
	}

	bool tryParseName(ParseState& state,Name& outName)
	{
		if(state.nextToken->type != t_name) { return false; }

		// Find the first non-name character.
		const char* firstChar = state.string + state.nextToken->begin;;
		const char* nextChar = firstChar;
		assert(*nextChar == '$');
		++nextChar;
		while(true)
		{
			const char c = *nextChar;
			if((c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			|| c=='_' || c=='\'' || c=='+' || c=='-' || c=='*' || c=='/' || c=='\\' || c=='^' || c=='~' || c=='='
			|| c=='<' || c=='>' || c=='!' || c=='?' || c=='@' || c=='#' || c=='$' || c=='%' || c=='&' || c=='|'
			|| c==':' || c=='`' || c=='.')
			{
				++nextChar;
			}
			else
			{
				break;
			}
		};

		assert(nextChar - state.string > state.nextToken->begin + 1);
		++state.nextToken;
		assert(nextChar - state.string <= state.nextToken->begin);
		assert(nextChar - firstChar <= UINT32_MAX);
		outName = Name(firstChar,uint32(nextChar - firstChar));
		return true;
	}

	bool tryParseNameOrIndexRef(ParseState& state,Reference& outRef)
	{
		outRef.token = state.nextToken;
		if(tryParseName(state,outRef.name)) { outRef.type = Reference::Type::name; return true; }
		else if(tryParseI64(state,outRef.index)) { outRef.type = Reference::Type::index; return true; }
		return false;
	}

	uintp parseAndResolveNameOrIndexRef(ParseState& state,const NameToIndexMap& nameToIndexMap,const char* context)
	{
		Reference ref;
		if(!tryParseNameOrIndexRef(state,ref))
		{
			parseErrorf(state,state.nextToken,"expected %s name or index",context);
			throw RecoverParseException();
		}
		return resolveRef(state,nameToIndexMap,ref);
	}

	void bindName(ParseState& state,NameToIndexMap& nameToIndexMap,const Name& name,uintp index)
	{
		if(name)
		{
			auto mapIt = nameToIndexMap.find(name);
			if(mapIt != nameToIndexMap.end())
			{
				const TextFileLocus previousDefinitionLocus = calcLocusFromOffset(state.string,	state.lineInfo,mapIt->first.getCharOffset(state.string));
				parseErrorf(state,name.getCharOffset(state.string),"redefinition of name defined at %s",previousDefinitionLocus.describe().c_str());
			}
			nameToIndexMap.emplace(name,index);
		}
	}

	uintp resolveRef(ParseState& state,const NameToIndexMap& nameToIndexMap,const Reference& ref)
	{
		switch(ref.type)
		{
		case Reference::Type::index: return ref.index;
		case Reference::Type::name:
		{
			auto nameToIndexMapIt = nameToIndexMap.find(ref.name);
			if(nameToIndexMapIt == nameToIndexMap.end())
			{
				parseErrorf(state,ref.token,"unknown name");
				return UINTPTR_MAX;
			}
			else
			{
				return nameToIndexMapIt->second;
			}
		}
		default: Core::unreachable();
		};
	}
	
	bool tryParseHexit(const char*& nextChar,uint8& outValue)
	{
		if(*nextChar >= '0' && *nextChar <= '9') { outValue = *nextChar - '0'; }
		else if(*nextChar >= 'a' && *nextChar <= 'f') { outValue = *nextChar - 'a' + 10; }
		else if(*nextChar >= 'A' && *nextChar <= 'F') { outValue = *nextChar - 'A' + 10; }
		else
		{
			outValue = 0;
			return false;
		}
		++nextChar;
		return true;
	}
	
	static void parseCharEscapeCode(const char*& nextChar,ParseState& state,char& outEscapedChar)
	{
		uint8 firstNibble;
		if(tryParseHexit(nextChar,firstNibble))
		{
			// Parse an 8-bit literal from two hexits.
			uint8 secondNibble;
			if(!tryParseHexit(nextChar,secondNibble)) { parseErrorf(state,nextChar,"expected hexit"); }
			outEscapedChar = firstNibble * 16 + secondNibble;
		}
		else
		{
			switch(*nextChar)
			{
			case 'n':	outEscapedChar = '\n'; ++nextChar; break;
			case 't': outEscapedChar = '\t'; ++nextChar; break;
			case '\\': outEscapedChar = '\\'; ++nextChar; break;
			case '\'': outEscapedChar = '\''; ++nextChar; break;
			case '\"': outEscapedChar = '\"'; ++nextChar; break;
			default:
				outEscapedChar = '\\';
				++nextChar;
				parseErrorf(state,nextChar,"invalid escape code");
				break;
			}
		}
	}

	bool tryParseString(ParseState& state,std::string& outString)
	{
		if(state.nextToken->type != t_string)
		{
			return false;
		}

		// Parse a string literal; the lexer has already rejected unterminated strings,
		// so this just needs to copy the characters and evaluate escape codes.
		const char* nextChar = state.string + state.nextToken->begin;
		assert(*nextChar == '\"');
		++nextChar;
		while(true)
		{
			switch(*nextChar)
			{
			case '\\':
			{
				++nextChar;
				char escapedChar = 0;
				parseCharEscapeCode(nextChar,state,escapedChar);
				outString += escapedChar;
				break;
			}
			case '\"':
				++state.nextToken;
				assert(state.string + state.nextToken->begin > nextChar);
				return true;
			default:
				outString += *nextChar++;
				break;
			};
		};
	}

	std::string parseUTF8String(ParseState& state)
	{
		const Token* stringToken = state.nextToken;
		std::string result;
		if(!tryParseString(state,result))
		{
			parseErrorf(state,stringToken,"expected string literal");
			throw RecoverParseException();
		}

		// Check that the string is a valid UTF-8 encoding.
		// The valid ranges are taken from table 3-7 in the Unicode Standard 9.0:
		// "Well-Formed UTF-8 Byte Sequences"
		const uint8* endChar = (const uint8*)result.data() + result.size();
		const uint8* nextChar = (const uint8*)result.data();
		while(nextChar != endChar)
		{
			if(*nextChar < 0x80) { ++nextChar; }
			else if(*nextChar >= 0xc2 && *nextChar <= 0xdf)
			{
				if(nextChar + 1 >= endChar
				|| nextChar[1] < 0x80 || nextChar[1] > 0xbf) { goto invalid; }
				nextChar += 2;
			}
			else if(*nextChar == 0xe0)
			{
				if(nextChar + 2 >= endChar
				|| nextChar[1] < 0xa0 || nextChar[1] > 0xbf
				|| nextChar[2] < 0x80 || nextChar[2] > 0xbf) { goto invalid; }
				nextChar += 3;
			}
			else if(*nextChar == 0xed)
			{
				if(nextChar + 2 >= endChar
				|| nextChar[1] < 0xa0 || nextChar[1] > 0x9f
				|| nextChar[2] < 0x80 || nextChar[2] > 0xbf) { goto invalid; }
				nextChar += 3;
			}
			else if(*nextChar >= 0xe1 && *nextChar <= 0xef)
			{
				if(nextChar + 2 >= endChar
				|| nextChar[1] < 0x80 || nextChar[1] > 0xbf
				|| nextChar[2] < 0x80 || nextChar[2] > 0xbf) { goto invalid; }
				nextChar += 3;
			}
			else if(*nextChar == 0xf0)
			{
				if(nextChar + 3 >= endChar
				|| nextChar[1] < 0x90 || nextChar[1] > 0xbf
				|| nextChar[2] < 0x80 || nextChar[2] > 0xbf
				|| nextChar[3] < 0x80 || nextChar[3] > 0xbf) { goto invalid; }
				nextChar += 4;
			}
			else if(*nextChar >= 0xf1 && *nextChar <= 0xf3)
			{
				if(nextChar + 3 >= endChar
				|| nextChar[1] < 0x90 || nextChar[1] > 0xbf
				|| nextChar[2] < 0x80 || nextChar[2] > 0xbf
				|| nextChar[3] < 0x80 || nextChar[3] > 0xbf) { goto invalid; }
				nextChar += 4;
			}
			else if(*nextChar == 0xf4)
			{
				if(nextChar + 3 >= endChar
				|| nextChar[1] < 0x80 || nextChar[1] > 0x8f
				|| nextChar[2] < 0x80 || nextChar[2] > 0xbf
				|| nextChar[3] < 0x80 || nextChar[3] > 0xbf) { goto invalid; }
				nextChar += 4;
			}
			else { goto invalid; }
		}
		
		return result;

	invalid:
		const uintp charOffset = stringToken->begin + (nextChar - (const uint8*)result.data()) + 1;
		parseErrorf(state,charOffset,"invalid UTF-8 encoding");
		return result;
	}
}