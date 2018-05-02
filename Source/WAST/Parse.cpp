#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Unicode.h"
#include "IR/Module.h"
#include "Lexer.h"
#include "Parse.h"
#include "WAST.h"

#include <cstdarg>
#include <cstdio>
#include <string>

using namespace IR;

namespace WAST
{
	void findClosingParenthesis(CursorState* cursor,const Token* openingParenthesisToken)
	{
		// Skip over tokens until the ')' closing the current parentheses nesting depth is found.
		Uptr depth = 1;
		while(depth > 0)
		{
			switch(cursor->nextToken->type)
			{
			default:
				++cursor->nextToken;
				break;
			case t_leftParenthesis:
				++cursor->nextToken;
				++depth;
				break;
			case t_rightParenthesis:
				++cursor->nextToken;
				--depth;
				break;
			case t_eof:
				parseErrorf(cursor->parseState,openingParenthesisToken,"reached end of input while trying to find closing parenthesis");
				throw FatalParseException();
			}
		}
	}

	static void parseErrorf(ParseState* parseState,Uptr charOffset,const char* messageFormat,va_list messageArguments)
	{
		// Format the message.
		char messageBuffer[1024];
		int numPrintedChars = std::vsnprintf(messageBuffer,sizeof(messageBuffer),messageFormat,messageArguments);
		if(numPrintedChars >= 1023 || numPrintedChars < 0) { Errors::unreachable(); }
		messageBuffer[numPrintedChars] = 0;

		// Add the error to the cursor's error list.
		parseState->unresolvedErrors.emplace_back(charOffset,messageBuffer);
	}
	void parseErrorf(ParseState* parseState,Uptr charOffset,const char* messageFormat,...)
	{
		va_list messageArguments;
		va_start(messageArguments,messageFormat);
		parseErrorf(parseState,charOffset,messageFormat,messageArguments);
		va_end(messageArguments);
	}
	void parseErrorf(ParseState* parseState,const char* nextChar,const char* messageFormat,...)
	{
		va_list messageArguments;
		va_start(messageArguments,messageFormat);
		parseErrorf(parseState,nextChar - parseState->string,messageFormat,messageArguments);
		va_end(messageArguments);
	}
	void parseErrorf(ParseState* parseState,const Token* nextToken,const char* messageFormat,...)
	{
		va_list messageArguments;
		va_start(messageArguments,messageFormat);
		parseErrorf(parseState,nextToken->begin,messageFormat,messageArguments);
		va_end(messageArguments);
	}

	void require(CursorState* cursor,TokenType type)
	{
		if(cursor->nextToken->type != type)
		{
			parseErrorf(cursor->parseState,cursor->nextToken,"expected %s",describeToken(type));
			throw RecoverParseException();
		}
		++cursor->nextToken;
	}
	
	bool tryParseValueType(CursorState* cursor,ValueType& outValueType)
	{
		switch(cursor->nextToken->type)
		{
		case t_i32: ++cursor->nextToken; outValueType = ValueType::i32; return true;
		case t_i64: ++cursor->nextToken; outValueType = ValueType::i64; return true;
		case t_f32: ++cursor->nextToken; outValueType = ValueType::f32; return true;
		case t_f64: ++cursor->nextToken; outValueType = ValueType::f64; return true;
		case t_v128: ++cursor->nextToken; outValueType = ValueType::v128; return true;
		default:
			outValueType = ValueType::any;
			return false;
		};
	}
	
	bool tryParseResultType(CursorState* cursor,ResultType& outResultType)
	{
		return tryParseValueType(cursor,*(ValueType*)&outResultType);
	}

	ValueType parseValueType(CursorState* cursor)
	{
		ValueType result;
		if(!tryParseValueType(cursor,result))
		{
			parseErrorf(cursor->parseState,cursor->nextToken,"expected value type");
			throw RecoverParseException();
		}
		return result;
	}

	const FunctionType* parseFunctionType(CursorState* cursor,NameToIndexMap& outLocalNameToIndexMap,std::vector<std::string>& outLocalDisassemblyNames)
	{
		std::vector<ValueType> parameters;
		ResultType ret = ResultType::none;

		// Parse the function parameters.
		while(tryParseParenthesizedTagged(cursor,t_param,[&]
		{
			Name parameterName;
			if(tryParseName(cursor,parameterName))
			{
				// (param <name> <type>)
				bindName(cursor->parseState,outLocalNameToIndexMap,parameterName,parameters.size());
				parameters.push_back(parseValueType(cursor));
				outLocalDisassemblyNames.push_back(parameterName.getString());
			}
			else
			{
				// (param <type>*)
				ValueType parameterType;
				while(tryParseValueType(cursor,parameterType))
				{
					parameters.push_back(parameterType);
					outLocalDisassemblyNames.push_back(std::string());
				};
			}
		}));

		// Parse <= 1 result type: (result <value type>*)*
		while(cursor->nextToken[0].type == t_leftParenthesis
		&& cursor->nextToken[1].type == t_result)
		{
			parseParenthesized(cursor,[&]
			{
				require(cursor,t_result);

				ResultType resultElementType;
				const Token* elementToken = cursor->nextToken;
				while(tryParseResultType(cursor,resultElementType))
				{
					if(ret != ResultType::none) { parseErrorf(cursor->parseState,elementToken,"function type cannot have more than 1 result element"); }
					ret = resultElementType;
				};
			});
		};

		return FunctionType::get(ret,parameters);
	}
	
	UnresolvedFunctionType parseFunctionTypeRefAndOrDecl(CursorState* cursor,NameToIndexMap& outLocalNameToIndexMap,std::vector<std::string>& outLocalDisassemblyNames)
	{
		// Parse an optional function type reference.
		Reference functionTypeRef;
		if(cursor->nextToken[0].type == t_leftParenthesis
		&& cursor->nextToken[1].type == t_type)
		{
			parseParenthesized(cursor,[&]
			{
				require(cursor,t_type);
				if(!tryParseNameOrIndexRef(cursor,functionTypeRef))
				{
					parseErrorf(cursor->parseState,cursor->nextToken,"expected type name or index");
					throw RecoverParseException();
				}
			});
		}

		// Parse the explicit function parameters and result type.
		const FunctionType* explicitFunctionType = parseFunctionType(cursor,outLocalNameToIndexMap,outLocalDisassemblyNames);

		UnresolvedFunctionType result;
		result.reference = functionTypeRef;
		result.explicitType = explicitFunctionType;
		return result;
	}

	IndexedFunctionType resolveFunctionType(ModuleState* moduleState,const UnresolvedFunctionType& unresolvedType)
	{
		if(!unresolvedType.reference)
		{
			return getUniqueFunctionTypeIndex(moduleState,unresolvedType.explicitType);
		}
		else
		{
			// Resolve the referenced type.
			const U32 referencedFunctionTypeIndex = resolveRef(
				moduleState->parseState,
				moduleState->typeNameToIndexMap,
				moduleState->module.types.size(),
				unresolvedType.reference);

			// Validate that if the function definition has both a type reference and explicit parameter/result type declarations, they match.
			const bool hasExplicitParametersOrResultType = unresolvedType.explicitType != FunctionType::get();
			if(hasExplicitParametersOrResultType)
			{
				if(referencedFunctionTypeIndex != UINT32_MAX
				&& moduleState->module.types[referencedFunctionTypeIndex] != unresolvedType.explicitType)
				{
					parseErrorf(
						moduleState->parseState,
						unresolvedType.reference.token,
						"referenced function type (%s) does not match declared parameters and results (%s)",
						asString(moduleState->module.types[referencedFunctionTypeIndex]).c_str(),
						asString(unresolvedType.explicitType).c_str());
				}
			}

			return {referencedFunctionTypeIndex};
		}
	}

	IndexedFunctionType getUniqueFunctionTypeIndex(ModuleState* moduleState,const FunctionType* functionType)
	{
		// If this type is not in the module's type table yet, add it.
		U32& functionTypeIndex = moduleState->functionTypeToIndexMap.getOrAdd(functionType,UINT32_MAX);
		if(functionTypeIndex == UINT32_MAX)
		{
			errorUnless(moduleState->module.types.size() < UINT32_MAX);
			functionTypeIndex = U32(moduleState->module.types.size());
			moduleState->module.types.push_back(functionType);
			moduleState->disassemblyNames.types.push_back(std::string());
		}
		return IndexedFunctionType {functionTypeIndex};
	}
	
	bool tryParseName(CursorState* cursor,Name& outName)
	{
		if(cursor->nextToken->type != t_name) { return false; }

		const char* firstChar = cursor->parseState->string + cursor->nextToken->begin;
		const char* nextChar = firstChar;
		wavmAssert(*nextChar == '$');
		++nextChar;

		// Find the first non-name character.
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
				
		outName = Name(firstChar + 1,U32(nextChar - firstChar - 1), cursor->nextToken->begin);

		wavmAssert(U32(nextChar - cursor->parseState->string) > cursor->nextToken->begin + 1);
		++cursor->nextToken;
		wavmAssert(U32(nextChar - cursor->parseState->string) <= cursor->nextToken->begin);
		wavmAssert(U32(nextChar - firstChar) <= UINT32_MAX);
		return true;
	}

	bool tryParseNameOrIndexRef(CursorState* cursor,Reference& outRef)
	{
		outRef.token = cursor->nextToken;
		if(tryParseName(cursor,outRef.name)) { outRef.type = Reference::Type::name; return true; }
		else if(tryParseI32(cursor,outRef.index)) { outRef.type = Reference::Type::index; return true; }
		return false;
	}

	U32 parseAndResolveNameOrIndexRef(CursorState* cursor,const NameToIndexMap& nameToIndexMap,Uptr maxIndex,const char* context)
	{
		Reference ref;
		if(!tryParseNameOrIndexRef(cursor,ref))
		{
			parseErrorf(cursor->parseState,cursor->nextToken,"expected %s name or index",context);
			throw RecoverParseException();
		}
		return resolveRef(cursor->parseState,nameToIndexMap,maxIndex,ref);
	}

	void bindName(ParseState* parseState,NameToIndexMap& nameToIndexMap,const Name& name,Uptr index)
	{
		errorUnless(index <= UINT32_MAX);

		if(name)
		{
			if(!nameToIndexMap.add(name, U32(index)))
			{
				const HashMapPair<Name, U32>* nameIndexPair = nameToIndexMap.getPair(name);
				wavmAssert(nameIndexPair);
				const TextFileLocus previousDefinitionLocus = calcLocusFromOffset(
					parseState->string,
					parseState->lineInfo,
					nameIndexPair->key.getSourceOffset()
					);
				parseErrorf(
					parseState,
					name.getSourceOffset(),
					"redefinition of name defined at %s",
					previousDefinitionLocus.describe().c_str());
				nameToIndexMap.set(name,U32(index));
			}
		}
	}

	U32 resolveRef(ParseState* parseState,const NameToIndexMap& nameToIndexMap,Uptr maxIndex,const Reference& ref)
	{
		switch(ref.type)
		{
		case Reference::Type::index:
		{
			if(ref.index >= maxIndex)
			{
				parseErrorf(parseState,ref.token,"invalid index");
				return UINT32_MAX;
			}
			return ref.index;
		}
		case Reference::Type::name:
		{
			const HashMapPair<Name, U32>* nameIndexPair = nameToIndexMap.getPair(ref.name);
			if(!nameIndexPair)
			{
				parseErrorf(parseState,ref.token,"unknown name");
				return UINT32_MAX;
			}
			else
			{
				return nameIndexPair->value;
			}
		}
		default: Errors::unreachable();
		};
	}
	
	bool tryParseHexit(const char*& nextChar,U8& outValue)
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
	
	static void parseCharEscapeCode(const char*& nextChar,ParseState* parseState,std::string& outString)
	{
		U8 firstNibble;
		if(tryParseHexit(nextChar,firstNibble))
		{
			// Parse an 8-bit literal from two hexits.
			U8 secondNibble;
			if(!tryParseHexit(nextChar,secondNibble)) { parseErrorf(parseState,nextChar,"expected hexit"); }
			outString += char(firstNibble * 16 + secondNibble);
		}
		else
		{
			switch(*nextChar)
			{
			case 't':	outString += '\t'; ++nextChar; break;
			case 'n':	outString += '\n'; ++nextChar; break;
			case 'r':	outString += '\r'; ++nextChar; break;
			case '\"':	outString += '\"'; ++nextChar; break;
			case '\'':	outString += '\''; ++nextChar; break;
			case '\\':	outString += '\\'; ++nextChar; break;
			case 'u':
			{
				// \u{...} - Unicode codepoint from hexadecimal number
				if(nextChar[1] != '{') { parseErrorf(parseState,nextChar,"expected '{'"); }
				nextChar += 2;
				
				// Parse the hexadecimal number.
				const char* firstHexit = nextChar;
				U32 codepoint = 0;
				U8 hexit = 0;
				while(tryParseHexit(nextChar,hexit))
				{
					if(codepoint > (UINT32_MAX - hexit) / 16)
					{
						codepoint = UINT32_MAX;
						while(tryParseHexit(nextChar,hexit)) {};
						break;
					}
					wavmAssert(codepoint * 16 + hexit >= codepoint);
					codepoint = codepoint * 16 + hexit;
				}

				// Check that it denotes a valid Unicode codepoint.
				if((codepoint >= 0xD800 && codepoint <= 0xDFFF)
				|| codepoint >= 0x110000)
				{
					parseErrorf(parseState,firstHexit,"invalid Unicode codepoint");
					codepoint = 0x1F642;
				}

				// Encode the codepoint as UTF-8.
				Unicode::encodeUTF8CodePoint(codepoint,outString);

				if(*nextChar != '}') { parseErrorf(parseState,nextChar,"expected '}'"); }
				++nextChar;
				break;
			}
			default:
				outString += '\\';
				++nextChar;
				parseErrorf(parseState,nextChar,"invalid escape code");
				break;
			}
		}
	}

	bool tryParseString(CursorState* cursor,std::string& outString)
	{
		if(cursor->nextToken->type != t_string)
		{
			return false;
		}

		// Parse a string literal; the lexer has already rejected unterminated strings,
		// so this just needs to copy the characters and evaluate escape codes.
		const char* nextChar = cursor->parseState->string + cursor->nextToken->begin;
		assert(*nextChar == '\"');
		++nextChar;
		while(true)
		{
			switch(*nextChar)
			{
			case '\\':
			{
				++nextChar;
				parseCharEscapeCode(nextChar,cursor->parseState,outString);
				break;
			}
			case '\"':
				++cursor->nextToken;
				assert(cursor->parseState->string + cursor->nextToken->begin > nextChar);
				return true;
			default:
				outString += *nextChar++;
				break;
			};
		};
	}

	std::string parseUTF8String(CursorState* cursor)
	{
		const Token* stringToken = cursor->nextToken;
		std::string result;
		if(!tryParseString(cursor,result))
		{
			parseErrorf(cursor->parseState,stringToken,"expected string literal");
			throw RecoverParseException();
		}

		// Check that the string is a valid UTF-8 encoding.
		const U8* endChar = (const U8*)result.data() + result.size();
		const U8* nextChar = Unicode::validateUTF8String((const U8*)result.data(),endChar);
		if(nextChar != endChar)
		{
			const Uptr charOffset = stringToken->begin + (nextChar - (const U8*)result.data()) + 1;
			parseErrorf(cursor->parseState,charOffset,"invalid UTF-8 encoding");
		}

		return result;
	}
}