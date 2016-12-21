#include "Core/Core.h"
#include "Core/SExpressions.h"
#include "Core/Floats.h"
#include <initializer_list>
#include <climits>
#include <cerrno>
#include <cstdlib>
#include "math.h"
#include "stdlib.h"
#include "string.h"

namespace DavidGay
{
	#define IEEE_8087
	#define NO_HEX_FP
	#define NO_INFNAN_CHECK
	#define strtod parseDecimalf64
	#define dtoa printDecimalf64

	#ifdef _MSC_VER
		#pragma warning(push)
		#pragma warning(disable : 4244 4083 4706 4701 4703)
	#elif defined(__GNUC__) && !defined(__clang__)
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wsign-compare"
		#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
		#define Long int
	#else
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wsign-compare"
		#define Long int
	#endif

	#include "../ThirdParty/dtoa.c"

	#ifdef _MSC_VER
		#pragma warning(pop)
	#else
		#pragma GCC diagnostic pop
		#undef Long
	#endif

	#undef IEEE_8087
	#undef NO_HEX_FP
	#undef NO_STRTOD_BIGCOMP
	#undef NO_INFNAN_CHECK
	#undef strtod
	#undef dtoa
};

namespace SExp
{
	struct FatalParseException
	{
		Core::TextFileLocus locus;
		std::string message;
		FatalParseException(const Core::TextFileLocus& inLocus,std::string&& inMessage)
		: locus(inLocus), message(std::move(inMessage)) {}
	};

	struct StreamState
	{
		const char* next;
		Core::TextFileLocus locus;

		StreamState(const char* inString)
		:	next(inString)
		{}
		
		inline char peek() const
		{
			return *next;
		}
		FORCEINLINE char consume(bool KnownToBeNonWhitespace = false)
		{
			auto result = *next;
			advance(KnownToBeNonWhitespace);
			return result;
		}
		Core::TextFileLocus getLocus() const { return locus; }
		
		FORCEINLINE void advance(bool KnownToBeNonWhitespace = false)
		{
			if(KnownToBeNonWhitespace)
			{
				locus.characters++;
			}
			else
			{
				switch(*next)
				{
				case 0: throw FatalParseException(locus,"unexpected end of file");
				case '\n': locus.newlines++; locus.tabs = 0; locus.characters = 0; break;
				case '\t': locus.tabs++; break;
				default: locus.characters++; break;
				};
			}
			next++;
		}
		void advanceToPtr(const char* newNext)
		{
			while(next != newNext)
			{
				advance();
			};
		}

		// Tries to skip to the next instance of a character, excluding instances between nested parentheses.
		// If successful, returns true. Will stop skipping and return false if it hits the end of the string or
		// a closing parenthesis that doesn't match a skipped opening parenthesis.
		bool advancePastNext(char c,bool ignoreParentheses)
		{
			uint32 parenthesesDepth = 0;
			while(true)
			{
				auto nextChar = peek();
				if(nextChar == 0)
				{
					return false;
				}
				else if(nextChar == c && (parenthesesDepth == 0 || ignoreParentheses))
				{
					advance(true);
					return true;
				}
				else if(nextChar == '(')
				{
					++parenthesesDepth;
				}
				else if(nextChar == ')')
				{
					if(parenthesesDepth == 0 && !ignoreParentheses)
					{
						throw FatalParseException(locus,"unexpected ')'");
					}
					--parenthesesDepth;
				}
				advance();
			};
		}
	};

	FORCEINLINE bool isWhitespace(char c)
	{
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	FORCEINLINE bool isDigit(char c)
	{
		return c >= '0' && c <= '9';
	}

	FORCEINLINE bool isSymbolCharacter(char c)
	{
		return !isWhitespace(c) && c != '(' && c != ')' && c != ';' && c != '\"' && c != '=' && c != ':' && c != 0;
	}

	FORCEINLINE bool isNameCharacter(char c)
	{
		return (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			|| c=='_' || c=='\'' || c=='+' || c=='-' || c=='*' || c=='/' || c=='\\' || c=='^' || c=='~' || c=='='
			|| c=='<' || c=='>' || c=='!' || c=='?' || c=='@' || c=='#' || c=='$' || c=='%' || c=='&' || c=='|'
			|| c==':' || c=='`' || c=='.';
	}

	FORCEINLINE bool parseHexit(char c,char& outValue)
	{
		if(c >= '0' && c <= '9')
		{
			outValue = c - '0';
			return true;
		}
		else if(c >= 'a' && c <= 'f')
		{
			outValue = c - 'a' + 10;
			return true;
		}
		else if(c >= 'A' && c <= 'F')
		{
			outValue = c - 'A' + 10;
			return true;
		}
		else
		{
			return false;
		}
	}

	FORCEINLINE uint64 shlSaturate(uint64 left,uint64 right)
	{
		return right >= 64 ? 0 : left << right;
	}
	FORCEINLINE uint64 shrSaturate(uint64 left,uint64 right)
	{
		return right >= 64 ? 0 : left >> right;
	}

	struct ParseContext
	{
		StreamState& state;
		MemoryArena::Arena& arena;
		const SymbolIndexMap& symbolIndexMap;

		ParseContext(StreamState& inState,MemoryArena::Arena& inArena,const SymbolIndexMap& inSymbolIndexMap)
		: state(inState), arena(inArena), symbolIndexMap(inSymbolIndexMap) {}
		
		bool parseCharEscapeCode(char& outCharacter)
		{
			switch(state.peek())
			{
			case 'n':	outCharacter = '\n'; break;
			case 't': outCharacter = '\t'; break;
			case '\\': outCharacter = '\\'; break;
			case '\'': outCharacter = '\''; break;
			case '\"': outCharacter = '\"'; break;
			default:
			{
				char firstValue;
				if(!parseHexit(state.peek(),firstValue))
				{
					return false;
				}
				state.advance(true);
				char secondValue;
				if(!parseHexit(state.peek(),secondValue))
				{
					return false;
				}
				outCharacter = firstValue * 16 + secondValue;
				break;
			}
			}
			return true;
		}

		SNode* createError(const Core::TextFileLocus& locus,const char* message)
		{
			auto result = new(arena) SNode(locus,SNodeType::Error);
			result->error = message;
			return result;
		}

		SNode* parseQuotedString()
		{
			const Core::TextFileLocus& startLocus = state.getLocus();
			state.advance(true);

			MemoryArena::String string;
			while(true)
			{
				const char nextChar = state.peek();
				if(nextChar == '\n' || nextChar == 0)
				{
					string.reset(arena);
					auto locus = state.getLocus();
					state.advancePastNext('\"',true);
					return createError(locus,"unexpected newline or end of file in quoted string");
				}
				else if(nextChar == '\\')
				{
					state.advance(true);
					char escapedChar;
					if(!parseCharEscapeCode(escapedChar))
					{
						string.reset(arena);
						auto locus = state.getLocus();
						state.advancePastNext('\"',true);
						return createError(locus,"invalid escape code in quoted string");
					}
					string.append(arena,escapedChar);
					state.advance(true);
				}
				else
				{
					state.advance(true);
					if(nextChar == '\"') { break; }
					else { string.append(arena,nextChar); }
				}
			}

			string.shrink(arena);
			auto node = new(arena)SNode(startLocus,SNodeType::String);
			node->string = string.c_str();
			node->stringLength = string.length();
			node->endLocus = state.getLocus();
			return node;
		}

		bool parseKeyword(const char* keyword)
		{
			StreamState savedState = state;
			for(uintp charIndex = 0;keyword[charIndex];++charIndex)
			{
				if(state.consume() != keyword[charIndex])
				{
					state = savedState;
					return false;
				}
			}
			return true;
		}

		uint64 parseHexInteger(uint64& outValue)
		{
			StreamState savedState = state;
			outValue = 0;
			uintp numMatchedCharacters = 0;
			while(true)
			{
				char hexit = 0;
				if(!parseHexit(state.peek(),hexit))
				{
					return numMatchedCharacters;
				}
				state.advance(true);
				++numMatchedCharacters;

				if(outValue > std::numeric_limits<uint64>::max() / 16)
				{
					state = savedState;
					return 0;
				}
				outValue *= 16;
				outValue += hexit;
			}
		}

		bool parseDecimalInteger(uint64& outValue)
		{
			errno = 0;
			const char* i64End = state.next;
			outValue = std::strtoull(state.next,const_cast<char**>(&i64End),10);
			if(i64End == state.next) { return false; }
			if(errno == ERANGE) { return false; }
			state.advanceToPtr(i64End);
			return true;
		}
	
		SNode* parseNaN(const Core::TextFileLocus& startLocus,bool isNegative)
		{
			Floats::F64Components F64Components;
			F64Components.bits.sign = isNegative;
			F64Components.bits.exponent = 0x7ff;
			F64Components.bits.significand = 1ull << 51;

			Floats::F32Components F32Components;
			F32Components.bits.sign = isNegative;
			F32Components.bits.exponent = 0xff;
			F32Components.bits.significand = 1ull << 22;

			if(state.peek() == ':')
			{
				state.advance(true);

				uint64 significandBits = 0;
				if(!parseKeyword("0x"))
				{
					state.advancePastNext(')',false);
					return createError(state.getLocus(),"expected hexadecimal NaN significand");
				}
				if(!parseHexInteger(significandBits))
				{
					return createError(state.getLocus(),"expected hexadecimal NaN significand");
				}
				if(significandBits == 0)
				{
					return createError(state.getLocus(),"NaN significand must be non-zero");
				}

				F64Components.bits.significand = significandBits;
				F32Components.bits.significand = significandBits;
			}

			auto node = new(arena) SNode(startLocus,SNodeType::Float);
			node->endLocus = state.getLocus();
			node->f64 = F64Components.value;
			node->f32 = F32Components.value;
			return node;
		}

		SNode* parseInfinity(const Core::TextFileLocus& startLocus,bool isNegative)
		{
			// Floating point infinite is represented by max exponent with a zero significand.
			Floats::F64Components F64Components;
			F64Components.bits.sign = isNegative ? 1 : 0;
			F64Components.bits.exponent = 0x7ff;
			F64Components.bits.significand = 0;
			Floats::F32Components F32Components;
			F32Components.bits.sign = isNegative ? 1 : 0;
			F32Components.bits.exponent = 0xff;
			F32Components.bits.significand = 0;

			auto node = new(arena) SNode(startLocus,SNodeType::Float);
			node->endLocus = state.getLocus();
			node->f64 = F64Components.value;
			node->f32 = F32Components.value;
			return node;
		}

		SNode* parseHexNumber(const Core::TextFileLocus& startLocus,bool isNegative)
		{
			uint64 integerPart;
			if(!parseHexInteger(integerPart)) { return createError(startLocus,"expected hex digits"); }

			bool hasDecimalPoint = false;
			uint64 fractionalPart = 0;
			if(parseKeyword("."))
			{
				// Shift the fractional part so the MSB is in the MSB of the float64 significand.
				auto numFractionalHexits = parseHexInteger(fractionalPart);
				fractionalPart <<= 52 - numFractionalHexits * 4;
				hasDecimalPoint = true;
			}
			
			int64 exponent = 0;
			bool hasExponent = false;
			if(parseKeyword("p") || parseKeyword("P"))
			{
				hasExponent = true;

				// Parse an optional exponent sign.
				bool isExponentNegative = false;
				if(state.peek() == '-' || state.peek() == '+') { isExponentNegative = state.consume(true) == '-'; }

				// Parse a decimal exponent.
				uint64 unsignedExponent = 0;
				if(!parseDecimalInteger(unsignedExponent))
				{
					return createError(state.getLocus(),"expected exponent decimal");
				}
				exponent = isExponentNegative ? -(int64)unsignedExponent : unsignedExponent;
				
				if(exponent < -1022 || exponent > 1023)
				{
					return createError(state.getLocus(),"exponent must be between -1022 and +1023");
				}
			}
		
			// If there wasn't a fractional part, or exponent, or negative zero, then just create an integer node.
			if(!fractionalPart && !exponent && !(isNegative && !integerPart && (hasDecimalPoint || hasExponent)))
			{
				auto node = new(arena) SNode(state.getLocus(),isNegative ? SNodeType::SignedInt : SNodeType::UnsignedInt);
				node->endLocus = state.getLocus();
				node->i64 = integerPart;
				return node;
			}
			else
			{
				if(!integerPart && !fractionalPart)
				{
					// If both the integer and fractional part are zero, just return zero.
					auto node = new(arena) SNode(state.getLocus(),SNodeType::Float);
					node->endLocus = state.getLocus();
					node->f64 = isNegative ? -0.0 : 0.0;
					node->f32 = isNegative ? -0.0f : 0.0f;
					return node;
				}
				else
				{
					// Normalize the integer and fractional parts so that integerPart=1.
					while(integerPart > 1)
					{
						if(fractionalPart & 1)
						{
							return createError(startLocus,"number has more bits than can be represented by 64-bit float");
						}
						if(exponent >= 1022)
						{
							return createError(startLocus,"normalized exponent must be between -1022 and +1023");
						}
						fractionalPart = ((integerPart & 1) << 51) | (fractionalPart >> 1);
						integerPart >>= 1;
						++exponent;
					};
					while(integerPart < 1)
					{
						if(exponent <= -1022)
						{
							// For subnormals (integerPart=0 fractionalPart!=0 exponent=-1022), change the encoded exponent to -1023.
							exponent = -1023;
							break;
						}
						integerPart = (fractionalPart >> 51) & 1;
						fractionalPart = (fractionalPart << 1) & ((1ull<<52)-1);
						--exponent;
					};
				}

				// Encode the float and create a node for it.
				Floats::F64Components F64Components;
				F64Components.bits.sign = isNegative ? 1 : 0;
				F64Components.bits.exponent = exponent + 1023;
				F64Components.bits.significand = fractionalPart;
				auto node = new(arena) SNode(state.getLocus(),SNodeType::Float);
				node->endLocus = state.getLocus();
				node->f64 = F64Components.value;
				node->f32 = (float32)F64Components.value;
				return node;
			}
		}

		SNode* parseNumber()
		{
			/*
				Syntax from the WebAssembly/spec interpreter number lexer
				let sign = ('+' | '-')?
				let num = sign digit+
				let hexnum = sign "0x" hexdigit+
				let int = num | hexnum
				let float = (num '.' digit+)
				  | num ('.' digit+)? ('e' | 'E') num
				  | sign "0x" hexdigit+ '.'? hexdigit* 'p' sign digit+
				  | sign "infinity"
				  | sign "nan"
				  | sign "nan(0x" hexdigit+ ")"
			*/
			auto startLocus = state.getLocus();

			// Parse the sign.
			bool isNegative = false;
			if(state.peek() == '-' || state.peek() == '+')
			{
				isNegative = state.consume(true) == '-';
			}

			// Handle nan, infinity, and hexadecimal numbers.
			if(parseKeyword("nan")) { return parseNaN(startLocus,isNegative); }
			else if(parseKeyword("infinity")) { return parseInfinity(startLocus,isNegative); }
			else if(parseKeyword("0x") || parseKeyword("0X")) { return parseHexNumber(startLocus,isNegative); }
			else
			{
				// Parse a decimal integer.
				const char* firstNumberChar = state.next;
				uint64 i64 = 0;
				bool i64Overflow = false;
				while(isDigit(*state.next))
				{
					if(i64 > (UINT64_MAX - (*state.next - '0')) / 10)
					{
						i64Overflow = true;
					}

					i64 *= 10;
					i64 += (*state.next - '0');
					state.advance(true);
				};

				if(*state.next == '.' || *state.next == 'e' || *state.next == 'E')
				{
					// Use David Gay's strtod to parse a floating point number.
					const char* f64End;
					auto f64 = DavidGay::parseDecimalf64(firstNumberChar,const_cast<char**>(&f64End));
					if (f64End == firstNumberChar)
					{
						state.advance();
						return createError(startLocus,"Unable to parse floating point number");
					}
					state.advanceToPtr(f64End);

					auto node = new(arena) SNode(startLocus,SNodeType::Float);
					node->endLocus = state.getLocus();
					node->f64 = isNegative ? -f64 : f64;
					node->f32 = (float32)node->f64;
					return node;
				}
				else if(i64Overflow)
				{
					return createError(startLocus,"number is too large for 64-bit integer");
				}
				else
				{
					auto node = new(arena) SNode(startLocus,isNegative ? SNodeType::SignedInt : SNodeType::UnsignedInt);
					node->endLocus = state.getLocus();
					node->i64 = i64;
					return node;
				}
			}
		}
	
		SNode* parseAttribute(SNode* symbolSNode)
		{
			state.advance(true);

			auto value = parseSNode();

			auto attributeSNode = new(arena) SNode(symbolSNode->startLocus,SNodeType::Attribute);
			attributeSNode->children = symbolSNode;
			attributeSNode->children->nextSibling = value;
			attributeSNode->endLocus = state.getLocus();
			return attributeSNode;
		}

		SNode* parseSymbol()
		{
			auto startLocus = state.getLocus();

			const char* symbolStart = state.next;
			do
			{
				state.advance(true);
			}
			while(isSymbolCharacter(state.peek()));
			MemoryArena::String symbolString;
			symbolString.append(arena,symbolStart,state.next - symbolStart);

			// If the symbol is nan or infinity, parse it as a number.
			if(!strcmp(symbolString.c_str(),"nan")) { return parseNaN(startLocus,false); }
			else if(!strcmp(symbolString.c_str(),"infinity")) { return parseInfinity(startLocus,false); }

			// Look up the symbol string in the index map.
			auto symbolIndexIt = symbolIndexMap.find(symbolString.c_str());
			SNode* symbolSNode;
			if(symbolIndexIt == symbolIndexMap.end())
			{
				symbolSNode = new(arena)SNode(startLocus,SNodeType::UnindexedSymbol);
				symbolSNode->endLocus = state.getLocus();
				symbolSNode->string = symbolString.c_str();
				symbolSNode->stringLength = symbolString.length();
			}
			else
			{
				// If the symbol was in the index map, discard the memory for the string and just store it as an index.
				symbolString.reset(arena);
				symbolSNode = new(arena)SNode(startLocus,SNodeType::Symbol);
				symbolSNode->endLocus = state.getLocus();
				symbolSNode->symbol = symbolIndexIt->second;
			}

			// If the symbol is followed by an equals sign, parse an attribute.
			if(state.peek() == '=') { return parseAttribute(symbolSNode); }
			else { return symbolSNode; }
		}

		SNode* parseName()
		{
			auto startLocus = state.getLocus();

			// skip the $.
			state.advance(true);

			// Consume characters until a non-name character is reached.
			const char* nameStart = state.next;
			while(isNameCharacter(state.peek()))
			{
				state.advance(true);
			}

			// Copy the name's characters to the result arena.
			MemoryArena::String nameString;
			nameString.append(arena,nameStart,state.next - nameStart);

			// Create the name node.
			SNode* nameSNode;
			nameSNode = new(arena)SNode(startLocus,SNodeType::Name);
			nameSNode->endLocus = state.getLocus();
			nameSNode->string = nameString.c_str();
			nameSNode->stringLength = nameString.length();
			return nameSNode;
		}

		void parseBlockComment(const Core::TextFileLocus& startLocus)
		{
			uintp commentNestingLevel = 1;
			while(commentNestingLevel > 0)
			{
				switch(state.peek())
				{
				case '(':
					state.advance(true);
					if(state.peek() == ';')
					{
						state.advance(true);
						++commentNestingLevel;
					}
					break;
				case ';':
					state.advance(true);
					if(state.peek() == ')')
					{
						state.advance(true);
						--commentNestingLevel;
					}
					break;
				case 0:
					throw FatalParseException(startLocus,"reached end of file without finding end of block comment");
				default:
					state.advance();
				}
			}
		}
	
		SNode* parseSNode()
		{
			while(true)
			{
				const char nextChar = state.peek();
				if(nextChar == 0)
				{
					return nullptr;
				}
				else if(isWhitespace(nextChar))
				{
					// Skip whitespace.
					state.advance();
				}
				else if(nextChar == ';')
				{
					// Parse a line comment.
					state.advance(true);
					if(state.peek() != ';')
					{
						throw FatalParseException(state.getLocus(),std::string("expected ';' following ';' but found '") + nextChar + "'");
					}

					while(state.peek() != '\n' && state.peek() != 0) state.advance();
				}
				else if(nextChar == '(')
				{
					auto startLocus = state.getLocus();
					state.advance(true);

					if(state.peek() == ';')
					{
						state.advance(true);
						// Parse a block comment.
						parseBlockComment(startLocus);
					}
					else
					{
						// Recursively parse child nodes.
						auto newSNode = new(arena)SNode(state.getLocus());
						parseSNodeSequence(&newSNode->children);
						if(state.peek() != ')')
						{
							throw FatalParseException(state.getLocus(),std::string("expected ')' following S-expression child nodes but found '") + state.peek() + "'");
						}
						state.advance(true);
						newSNode->endLocus = state.getLocus();
						return newSNode;
					}
				}
				else if(nextChar == '\"')
				{
					// Parse a quoted symbol.
					return parseQuotedString();
				}
				else if(isDigit(nextChar) || nextChar == '+' || nextChar == '-' || nextChar == '.')
				{
					// Parse a number.
					return parseNumber();
				}
				else if(nextChar == '$')
				{
					// Parse a name.
					return parseName();
				}
				else if(isSymbolCharacter(nextChar))
				{
					// Parse a symbol.
					return parseSymbol();
				}
				else
				{
					return nullptr;
				}
			}
		}

		void parseSNodeSequence(SNode** nextSNodePtr)
		{
			while(true)
			{
				auto node = parseSNode();
				if(!node) { break; }
				*nextSNodePtr = node;
				nextSNodePtr = &(*nextSNodePtr)->nextSibling;
			}
		}
	};

	SNode* parse(const char* string,MemoryArena::Arena& arena,const SymbolIndexMap& symbolIndexMap)
	{
		Core::Timer parseTimer;

		StreamState state(string);
		ParseContext parseContext(state,arena,symbolIndexMap);
		try
		{
			SNode* result = nullptr;
			parseContext.parseSNodeSequence(&result);
			if(state.peek() != 0)
			{
				auto errorMessage = std::string("expected ';' or '(' but found '") + state.peek() + "'";
				result = parseContext.createError(state.getLocus(),arena.copyToArena(errorMessage.c_str(),errorMessage.length() + 1));
			}
			Log::logRatePerSecond("Parsed S-expressions",parseTimer,float64(state.next - string) / 1024.0 / 1024.0,"MB");
			return result;
		}
		catch(FatalParseException exception)
		{
			auto result = parseContext.createError(exception.locus,arena.copyToArena(exception.message.c_str(),exception.message.length() + 1));
			return result;
		}
	}
}
