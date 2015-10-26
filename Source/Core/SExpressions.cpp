#include "Core/Core.h"
#include "Core/SExpressions.h"
#include "Core/Floats.h"
#include <initializer_list>
#include <climits>
#include <cerrno>
#include <cstdlib>

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
		
		char peek() const
		{
			return *next;
		}
		char consume()
		{
			auto result = *next;
			advance();
			return result;
		}
		Core::TextFileLocus getLocus() const { return locus; }
		
		void advance()
		{
			switch(*next)
			{
			case 0: throw new FatalParseException(locus,"unexpected end of file");
			case '\n': locus.newlines++; locus.tabs = 0; locus.characters = 0; break;
			case '\t': locus.tabs++; break;
			default: locus.characters++; break;
			};
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
		bool advancePastNext(char c)
		{
			uint32 parenthesesDepth = 0;
			while(true)
			{
				auto nextChar = peek();
				if(nextChar == 0)
				{
					return false;
				}
				else if(nextChar == c && parenthesesDepth == 0)
				{
					advance();
					return true;
				}
				else if(nextChar == '(')
				{
					++parenthesesDepth;
				}
				else if(nextChar == ')')
				{
					if(parenthesesDepth == 0)
					{
						return false;
					}
					--parenthesesDepth;
				}
				advance();
			};
		}
	};

	bool isWhitespace(char c)
	{
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	bool isDigit(char c)
	{
		return c >= '0' && c <= '9';
	}

	bool isSymbolCharacter(char c)
	{
		return !isWhitespace(c) && c != '(' && c != ')' && c != ';' && c != '\"';
	}

	bool parseHexit(char c,char& outValue)
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

	bool parseCharEscapeCode(StreamState& state,char& outCharacter)
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
			state.advance();
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

	bool parseChar(StreamState& state,char c)
	{
		if(state.peek() != c) { return false; }
		else
		{
			state.consume();
			return true;
		}
	}
	

	Node* createError(const Core::TextFileLocus& locus,Memory::Arena& arena,const char* message)
	{
		auto result = new(arena) Node(locus,NodeType::Error);
		result->error = message;
		return result;
	}

	Node* parseQuotedString(StreamState& state,Memory::Arena& arena)
	{
		state.advance();

		Memory::ArenaString string;
		while(true)
		{
			const char nextChar = state.peek();
			if(nextChar == '\n' || nextChar == 0)
			{
				string.reset(arena);
				auto locus = state.getLocus();
				state.advancePastNext('\"');
				return createError(locus,arena,"unexpected newline or end of file in quoted string");
			}
			else if(nextChar == '\\')
			{
				state.advance();
				char escapedChar;
				if(!parseCharEscapeCode(state,escapedChar))
				{
					string.reset(arena);
					auto locus = state.getLocus();
					state.advancePastNext('\"');
					return createError(locus,arena,"invalid escape code in quoted string");
				}
				string.append(arena,escapedChar);
				state.advance();
			}
			else
			{
				state.advance();
				if(nextChar == '\"') { break; }
				else { string.append(arena,nextChar); }
			}
		}

		string.shrink(arena);
		auto node = new(arena)Node(state.getLocus(),NodeType::String);
		node->string = string.c_str();
		node->stringLength = string.length();
		node->endLocus = state.getLocus();
		return node;
	}

	bool parseKeyword(StreamState& state,const char* keyword)
	{
		StreamState savedState = state;
		for(uintptr charIndex = 0;keyword[charIndex];++charIndex)
		{
			if(state.consume() != keyword[charIndex])
			{
				state = savedState;
				return false;
			}
		}
		return true;
	}

	uint64 parseHexInteger(StreamState& state,uint64& outValue)
	{
		StreamState savedState = state;
		outValue = 0;
		uintptr numMatchedCharacters = 0;
		while(true)
		{
			char hexit = 0;
			if(!parseHexit(state.peek(),hexit))
			{
				return numMatchedCharacters;
			}
			state.consume();
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

	bool parseDecimalInteger(StreamState& state,uint64& outValue)
	{
		errno = 0;
		const char* i64End = state.next;
		outValue = std::strtoull(state.next,const_cast<char**>(&i64End),10);
		if(i64End == state.next) { return false; }
		if(errno == ERANGE) { return false; }
		state.advanceToPtr(i64End);
		return true;
	}
	
	uint64 shlSaturate(uint64 left,uint64 right)
	{
		return right >= 64 ? 0 : left << right;
	}
	uint64 shrSaturate(uint64 left,uint64 right)
	{
		return right >= 64 ? 0 : left >> right;
	}

	Node* parseNaN(const Core::TextFileLocus& startLocus,StreamState& state,Memory::Arena& arena,bool isNegative)
	{
		Floats::F64Components f64Components;
		f64Components.bits.sign = isNegative;
		f64Components.bits.exponent = 0x7ff;
		f64Components.bits.significand = 1ull << 51;

		Floats::F32Components f32Components;
		f32Components.bits.sign = isNegative;
		f32Components.bits.exponent = 0xff;
		f32Components.bits.significand = 1ull << 22;

		if(state.peek() == '(')
		{
			state.consume();

			uint64 significandBits = 0;
			if(!parseKeyword(state,"0x"))
			{
				state.advancePastNext(')');
				return createError(state.getLocus(),arena,"expected hexadecimal NaN significand");
			}
			if(!parseHexInteger(state,significandBits))
			{
				return createError(state.getLocus(),arena,"expected hexadecimal NaN significand");
			}
			if(significandBits == 0)
			{
				return createError(state.getLocus(),arena,"NaN significand must be non-zero");
			}
			if(state.consume() != ')')
			{
				return createError(state.getLocus(),arena,"expected ')'");
			}

			f64Components.bits.significand = significandBits;
			f32Components.bits.significand = significandBits;
		}

		auto node = new(arena) Node(startLocus,NodeType::Float);
		node->endLocus = state.getLocus();
		node->f64 = f64Components.value;
		node->f32 = f32Components.value;
		return node;
	}

	Node* parseInfinity(const Core::TextFileLocus& startLocus,StreamState& state,Memory::Arena& arena,bool isNegative)
	{
		// Floating point infinite is represented by max exponent with a zero significand.
		Floats::F64Components f64Components;
		f64Components.bits.sign = isNegative ? 1 : 0;
		f64Components.bits.exponent = 0x7ff;
		f64Components.bits.significand = 0;
		Floats::F32Components f32Components;
		f32Components.bits.sign = isNegative ? 1 : 0;
		f32Components.bits.exponent = 0xff;
		f32Components.bits.significand = 0;

		auto node = new(arena) Node(startLocus,NodeType::Float);
		node->endLocus = state.getLocus();
		node->f64 = f64Components.value;
		node->f32 = f32Components.value;
		return node;
	}

	Node* parseHexNumber(const Core::TextFileLocus& startLocus,StreamState& state,Memory::Arena& arena,bool isNegative)
	{
		uint64 integerPart;
		if(!parseHexInteger(state,integerPart)) { return createError(state.getLocus(),arena,"expected hex digits"); }

		bool hasDecimalPoint = false;
		uint64 fractionalPart = 0;
		if(parseKeyword(state,"."))
		{
			// Shift the fractional part so the MSB is in the MSB of the float64 significand.
			auto numFractionalHexits = parseHexInteger(state,fractionalPart);
			fractionalPart <<= 52 - numFractionalHexits * 4;
			hasDecimalPoint = true;
		}
			
		int64 exponent = 0;
		bool hasExponent = false;
		if(parseKeyword(state,"p") || parseKeyword(state,"P"))
		{
			hasExponent = true;

			// Parse an optional exponent sign.
			bool isExponentNegative = false;
			if(state.peek() == '-' || state.peek() == '+') { isExponentNegative = state.consume() == '-'; }

			// Parse a decimal exponent.
			uint64 unsignedExponent = 0;
			if(!parseDecimalInteger(state,unsignedExponent))
			{
				return createError(state.getLocus(),arena,"expected exponent decimal");
			}
			exponent = isExponentNegative ? -(int64)unsignedExponent : unsignedExponent;
				
			if(exponent < -1022 || exponent > 1023)
			{
				return createError(state.getLocus(),arena,"exponent must be between -1022 and +1023");
			}
		}
		
		// If there wasn't a fractional part, or exponent, or negative zero, then just create an integer node.
		if(!fractionalPart && !exponent && !(isNegative && !integerPart && (hasDecimalPoint || hasExponent)))
		{
			auto node = new(arena) Node(state.getLocus(),isNegative ? NodeType::SignedInt : NodeType::UnsignedInt);
			node->endLocus = state.getLocus();
			node->i64 = isNegative ? -(int64)integerPart : integerPart;
			return node;
		}
		else
		{
			if(!integerPart)
			{
				if(!fractionalPart)
				{
					// If both the integer and fractional part are zero, just return zero.
					auto node = new(arena) Node(state.getLocus(),NodeType::Float);
					node->endLocus = state.getLocus();
					node->f64 = isNegative ? -0.0 : 0.0;
					node->f32 = isNegative ? -0.0f : 0.0f;
					return node;
				}
				else if(exponent != -1022)
				{
					return createError(startLocus,arena,"exponent on subnormal hexadecimal float must be -1022");
				}
				else
				{
					// For subnormals (integerPart=0 fractionalPart!=0 exponent=-1022), change the encoded exponent to -1023.
					exponent = -1023;
				}
			}
			else if(integerPart != 1)
			{
				return createError(startLocus,arena,"hexadecimal float must start with 0x1. or 0x0.");
			}

			// Encode the float and create a node for it.
			Floats::F64Components f64Components;
			f64Components.bits.sign = isNegative ? 1 : 0;
			f64Components.bits.exponent = exponent + 1023;
			f64Components.bits.significand = fractionalPart;
			auto node = new(arena) Node(state.getLocus(),NodeType::Float);
			node->endLocus = state.getLocus();
			node->f64 = f64Components.value;
			node->f32 = (float32)f64Components.value;
			return node;
		}
	}

	Node* parseNumber(StreamState& state,Memory::Arena& arena)
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
			isNegative = state.consume() == '-';
		}

		// Handle nan, infinity, and hexadecimal numbers.
		if(parseKeyword(state,"nan")) { return parseNaN(startLocus,state,arena,isNegative); }
		else if(parseKeyword(state,"infinity")) { return parseInfinity(startLocus,state,arena,isNegative); }
		else if(parseKeyword(state,"0x") || parseKeyword(state,"0X")) { return parseHexNumber(startLocus,state,arena,isNegative); }
		else
		{
			// For decimals, just use the std float parsing code.
			const char* f64End = state.next;
			float64 f64 = std::strtod(state.next,const_cast<char**>(&f64End));
			const char* i64End = state.next;
			uint64 i64 = std::strtoull(state.next,const_cast<char**>(&i64End),0);

			// Between the float and the integer parser, use whichever consumed more input, favoring integers if they're equal.
			if(f64End > i64End)
			{
				state.advanceToPtr(f64End);
				auto node = new(arena) Node(state.getLocus(),NodeType::Float);
				node->endLocus = state.getLocus();
				node->f64 = isNegative ? -f64 : f64;
				node->f32 = (float32)node->f64;
				return node;
			}
			else
			{
				state.advanceToPtr(i64End);
				auto node = new(arena) Node(state.getLocus(),isNegative ? NodeType::SignedInt : NodeType::UnsignedInt);
				node->endLocus = state.getLocus();
				node->i64 = isNegative ? -(int64)i64 : i64;
				return node;
			}
		}
	}
	
	Node* parseSymbol(StreamState& state,Memory::Arena& arena,const SymbolIndexMap& symbolIndexMap)
	{
		auto startLocus = state.getLocus();

		Memory::ArenaString string;
		do
		{
			string.append(arena,state.peek());
			state.advance();
		}
		while(isSymbolCharacter(state.peek()));

		// If the symbol is nan or infinity, parse it as a number.
		if(!strcmp(string.c_str(),"nan")) { return parseNaN(startLocus,state,arena,false); }
		else if(!strcmp(string.c_str(),"infinity")) { return parseInfinity(startLocus,state,arena,false); }

		// Look up the symbol string in the index map.
		auto symbolIndexIt = symbolIndexMap.find(string.c_str());
		if(symbolIndexIt == symbolIndexMap.end())
		{
			string.shrink(arena);
			auto node = new(arena)Node(startLocus,NodeType::UnindexedSymbol);
			node->endLocus = state.getLocus();
			node->string = string.c_str();
			node->stringLength = string.length();
			return node;
		}
		else
		{
			// If the symbol was in the index map, discard the memory for the string and just store it as an index.
			string.reset(arena);
			auto node = new(arena)Node(startLocus,NodeType::Symbol);
			node->endLocus = state.getLocus();
			node->symbol = symbolIndexIt->second;
			return node;
		}
	}

	void parseRecursive(StreamState& state,Memory::Arena& arena,const SymbolIndexMap& symbolIndexMap,Node** nextNodePtr)
	{
		while(true)
		{
			const char nextChar = state.peek();
			if(nextChar == 0)
			{
				break;
			}
			else if(isWhitespace(nextChar))
			{
				// Skip whitespace.
				state.advance();
			}
			else if(nextChar == ';')
			{
				// Parse a line comment.
				state.advance();
				if(state.peek() != ';')
				{
					throw new FatalParseException(state.getLocus(),std::string("expected ';' following ';' but found '") + nextChar + "'");
				}

				while(state.peek() != '\n' && state.peek() != 0) state.advance();
			}
			else if(nextChar == '(')
			{
				state.advance();

				if(state.peek() == ';')
				{
					// Parse a block comment.
					do
					{
						if(!state.advancePastNext(';'))
						{
							throw new FatalParseException(state.getLocus(),"reached end of file while parsing block comment");
						}
					}
					while(state.peek() != ')');
					state.advance();
				}
				else
				{
					// Recursively parse child nodes.
					auto newNode = new(arena)Node(state.getLocus());
					parseRecursive(state,arena,symbolIndexMap,&newNode->children);
					if(state.peek() != ')')
					{
						throw new FatalParseException(state.getLocus(),std::string("expected ')' following S-expression child nodes but found '") + nextChar + "'");
					}
					state.advance();
					newNode->endLocus = state.getLocus();
					*nextNodePtr = newNode;
					nextNodePtr = &newNode->nextSibling;
				}
			}
			else if(nextChar == ')')
			{
				// Finish parsing this node.
				break;
			}
			else if(nextChar == '\"')
			{
				// Parse a quoted symbol.
				*nextNodePtr = parseQuotedString(state,arena);
				nextNodePtr = &(*nextNodePtr)->nextSibling;
			}
			else if(isDigit(nextChar) || nextChar == '+' || nextChar == '-' || nextChar == '.')
			{
				// Parse a number.
				*nextNodePtr = parseNumber(state,arena);
				nextNodePtr = &(*nextNodePtr)->nextSibling;
			}
			else
			{
				// Parse a symbol.
				assert(isSymbolCharacter(nextChar));
				*nextNodePtr = parseSymbol(state,arena,symbolIndexMap);
				nextNodePtr = &(*nextNodePtr)->nextSibling;
			}
		}
	}

	Node* parse(const char* string,Memory::Arena& arena,const SymbolIndexMap& symbolIndexMap)
	{
		try
		{
			StreamState state(string);
			Node* firstRootNode = nullptr;
			parseRecursive(state,arena,symbolIndexMap,&firstRootNode);
			return firstRootNode;
		}
		catch(FatalParseException* exception)
		{
			auto result = createError(exception->locus,arena,arena.copyToArena(exception->message.c_str(),exception->message.length() + 1));
			delete exception;
			return result;
		}
	}

	char nibbleToHexChar(uint8 value) { return value < 10 ? ('0' + value) : 'a' + value - 10; }

	std::string escapeString(const char* string,size_t numChars)
	{
		std::string result;
		for(uintptr charIndex = 0;charIndex < numChars;++charIndex)
		{
			auto c = string[charIndex];
			if(c == '\\') { result += "\\\\"; }
			else if(c == '\"') { result += "\\\""; }
			else if(c == '\n') { result += "\\n"; }
			else if(c < 0x20 || c > 0x7e)
			{
				result += '\\';
				result += nibbleToHexChar((c & 0xf0) >> 4);
				result += nibbleToHexChar((c & 0x0f) >> 0);
			}
			else { result += c; }
		}
		return result;
	}

	bool printRecursive(SExp::Node* initialNode,const char* symbolStrings[],std::string& outString,const std::string& newline)
	{
		SExp::Node* node = initialNode;
		std::vector<std::string> childStrings;
		bool hasMultiLineSubtree = false;
		do
		{
			switch(node->type)
			{
			case NodeType::Tree:
			{
				std::string subtreeString;
				subtreeString += "(";
				bool isSubtreeMultiLine = printRecursive(node->children,symbolStrings,subtreeString,newline + '\t');
				if(isSubtreeMultiLine) { subtreeString += newline; hasMultiLineSubtree = true; }
				subtreeString += ")";
				childStrings.push_back(std::move(subtreeString));
				break;
			}
			case NodeType::Symbol: childStrings.emplace_back(symbolStrings[node->symbol]); break;
			case NodeType::UnindexedSymbol: childStrings.emplace_back(node->string); break;
			case NodeType::String: childStrings.emplace_back(std::move(std::string() + '\"' + escapeString(node->string,node->stringLength) + '\"')); break;
			case NodeType::Error: childStrings.emplace_back(node->error); break;
			case NodeType::SignedInt: childStrings.emplace_back(std::move(std::to_string(node->i64))); break;
			case NodeType::UnsignedInt: childStrings.emplace_back(std::move(std::to_string(node->u64))); break;
			case NodeType::Float: childStrings.emplace_back(std::move(Floats::asString(node->f64))); break;
			default: throw;
			};
			node = node->nextSibling;
		}
		while(node);
		
		size_t totalChildLength = 0;
		for(auto string : childStrings) { totalChildLength += string.length(); }
		auto isMultiLine = hasMultiLineSubtree || totalChildLength > 120;

		for(uintptr childIndex = 0;childIndex < childStrings.size();++childIndex)
		{
			outString += childStrings[childIndex];
			if(childIndex + 1 < childStrings.size()) { outString += isMultiLine ? newline : " "; }
		}

		return isMultiLine;
	}

	std::string print(SExp::Node* rootNode,const char* symbolStrings[])
	{
		std::string result;
		printRecursive(rootNode,symbolStrings,result,"\n");
		return result;
	}
}
