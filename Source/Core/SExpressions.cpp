#include "Core.h"
#include "SExpressions.h"
#include <initializer_list>
#include <fstream>
#include <limits>

namespace SExp
{
	struct FatalParseException
	{
		TextFileLocus locus;
		std::string message;
		FatalParseException(const TextFileLocus& inLocus,std::string&& inMessage)
		: locus(inLocus), message(std::move(inMessage)) {}
	};

	struct StreamState
	{
		StreamState(const char* inString)
		:	next(inString)
		{}

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
		char get() const
		{
			return *next;
		}
		TextFileLocus getLocus() const { return locus; }

		// Tries to skip to the next instance of a character, excluding instances between nested parentheses.
		// If successful, returns true. Will stop skipping and return false if it hits the end of the string or
		// a closing parenthesis that doesn't match a skipped opening parenthesis.
		bool skipToNext(char c)
		{
			uint32_t parenthesesDepth = 0;
			while(true)
			{
				auto nextChar = get();
				if(nextChar == 0)
				{
					return false;
				}
				else if(nextChar == c && parenthesesDepth == 0)
				{
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

	private:
		const char* next;
		TextFileLocus locus;
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

	bool parseHexDigit(char c,char& outValue)
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
		switch(state.get())
		{
		case 'n':	outCharacter = '\n'; break;
		case 't': outCharacter = '\t'; break;
		case '\\': outCharacter = '\\'; break;
		case '\'': outCharacter = '\''; break;
		case '\"': outCharacter = '\"'; break;
		default:
		{
			char firstValue;
			if(!parseHexDigit(state.get(),firstValue))
			{
				return false;
			}
			state.advance();
			char secondValue;
			if(!parseHexDigit(state.get(),secondValue))
			{
				return false;
			}
			outCharacter = firstValue * 16 + secondValue;
			break;
		}
		}
		return true;
	}

	Node* parseQuotedString(StreamState& state,Memory::Arena& arena)
	{
		auto node = new(arena)Node(state.getLocus(),NodeType::String);
		state.advance();

		Memory::ArenaString string;
		while(true)
		{
			const char nextChar = state.get();
			if(nextChar == '\n' || nextChar == 0)
			{
				string.reset(arena);
				node->endLocus = state.getLocus();
				node->type = NodeType::Error;
				node->error = "unexpected newline or end of file in quoted string";
				state.skipToNext('\"');
				return node;
			}
			else if(nextChar == '\\')
			{
				state.advance();
				char escapedChar;
				if(!parseCharEscapeCode(state,escapedChar))
				{
					string.reset(arena);
					node->endLocus = state.getLocus();
					node->type = NodeType::Error;
					node->error = "invalid escape code in quoted string";
					state.skipToNext('\"');
					return node;
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
		node->string = string.c_str();
		node->stringLength = string.length();
		node->endLocus = state.getLocus();

		return node;
	}

	Node* parseSymbol(StreamState& state,Memory::Arena& arena,const SymbolIndexMap& symbolIndexMap)
	{
		auto node = new(arena)Node(state.getLocus(),NodeType::Symbol);

		Memory::ArenaString string;
		do
		{
			string.append(arena,state.get());
			state.advance();
		}
		while(isSymbolCharacter(state.get()));

		node->endLocus = state.getLocus();

		// Look up the symbol string in the index map.
		auto symbolIndexIt = symbolIndexMap.find(string.c_str());
		if(symbolIndexIt == symbolIndexMap.end())
		{
			string.shrink(arena);
			node->type = NodeType::UnindexedSymbol;
			node->string = string.c_str();
			node->stringLength = string.length();
		}
		else
		{
			// If the symbol was in the index map, discard the memory for the string and just store it as an index.
			string.reset(arena);
			node->symbol = symbolIndexIt->second;
		}

		return node;
	}

	Node* parseNumber(StreamState& state,Memory::Arena& arena)
	{
		auto startLocus = state.getLocus();

		bool isNegative = false;
		if(state.get() == '+' || state.get() == '-')
		{
			isNegative = state.get() == '-';
			state.advance();
		}

		// First parse the digits and decimal point.
		Memory::ArenaString digits;
		uintptr_t numIntegerDigits = 0;
		uintptr_t base = 10;
		bool hasDecimalPoint = false;
		while(true)
		{
			auto nextChar = state.get();
			if(isDigit(nextChar))
			{
				digits.append(arena,nextChar - '0');
			}
			else if(nextChar == '.')
			{
				if(hasDecimalPoint)
				{
					auto node = new(arena) Node(state.getLocus(),NodeType::Error);
					node->error = "number must have no more than one decimal point";
					return node;
				}
				else
				{
					numIntegerDigits = digits.length();
					hasDecimalPoint = true;
				}
			}
			else
			{
				break;
			}
			state.advance();
		}
		
		if(hasDecimalPoint)
		{
			// If the number has a decimal point, decode its digits into a float64.
			double accumulator = 0.0;
			double digitFactor = isNegative ? -1.0 : 1.0;
			for(uintptr_t digitIndex = 0;digitIndex < digits.length();++digitIndex)
			{
				if(digitIndex < numIntegerDigits)
				{
					// Detect if the double is about to overflow its maximum value.
					if(std::abs(accumulator) > std::numeric_limits<double>::max() / base) 
					{
						auto node = new(arena) Node(state.getLocus(),NodeType::Error);
						node->error = "number is too large to represent as a 64-bit float";
						return node;
					}
					accumulator *= base;
				}
				else { digitFactor /= base; }
				accumulator += digitFactor * digits[digitIndex];
			}
			digits.reset(arena);

			auto node = new(arena)Node(startLocus,NodeType::Decimal);
			node->decimal = accumulator;
			return node;
		}
		else
		{
			// If the number doesn't have a decimal point, decode its digits into an int64.
			uint64_t accumulator = 0;
			bool notEnoughBits = false;
			for(uintptr_t digitIndex = 0;digitIndex < digits.length();++digitIndex)
			{
				// Detect if the uint64_t is about to overflow.
				if(accumulator * base / base != accumulator) { notEnoughBits = true; }
				accumulator *= base;
				accumulator += digits[digitIndex];
			}
			digits.reset(arena);
			if(isNegative && -(-(int64_t)accumulator) != (int64_t)accumulator) { notEnoughBits = true; }			
			if(notEnoughBits)
			{
				auto node = new(arena) Node(state.getLocus(),NodeType::Error);
				node->error = "number is too large to represent as a 64-bit integer";
				return node;
			}
			else
			{
				auto node = new(arena)Node(startLocus,NodeType::Int);
				node->integer = isNegative ? -(int64_t)accumulator : +accumulator;
				return node;
			}
		}
	}

	void parseRecursive(StreamState& state,Memory::Arena& arena,const SymbolIndexMap& symbolIndexMap,Node** nextNodePtr)
	{
		while(true)
		{
			const char nextChar = state.get();
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
				if(state.get() != ';')
				{
					throw new(arena) FatalParseException(state.getLocus(),std::string("expected ';' following ';' but found '") + nextChar + "'");
				}

				while(state.get() != '\n' && state.get() != 0) state.advance();
			}
			else if(nextChar == '(')
			{
				state.advance();

				if(state.get() == ';')
				{
					// Parse a block comment.
					do
					{
						state.skipToNext(';');
						state.advance();
						if(state.get() == 0)
						{
							throw new(arena) FatalParseException(state.getLocus(),"reached end of file while parsing block comment");
						}
					}
					while(state.get() != ')');
					state.advance();
				}
				else
				{
					// Recursively parse child nodes.
					auto newNode = new(arena)Node(state.getLocus());
					parseRecursive(state,arena,symbolIndexMap,&newNode->children);
					if(state.get() != ')')
					{
						throw new(arena) FatalParseException(state.getLocus(),std::string("expected ')' following S-expression child nodes but found '") + nextChar + "'");
					}
					state.advance();
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
			auto errorNode = new(arena) Node(exception->locus,NodeType::Error);
			errorNode->error = arena.copyToArena(exception->message.c_str(),exception->message.length() + 1);
			delete exception;
			return errorNode;
		}
	}

	char nibbleToHex(uint8_t value) { return value < 10 ? ('0' + value) : 'a' + value - 10; }

	std::string escapeString(const char* string,size_t numChars)
	{
		std::string result;
		for(uintptr_t charIndex = 0;charIndex < numChars;++charIndex)
		{
			auto c = string[charIndex];
			if(c == '\\') { result += "\\\\"; }
			else if(c == '\"') { result += "\\\""; }
			else if(c == '\n') { result += "\\n"; }
			else if(c < 0x20 || c > 0x7e)
			{
				result += '\\';
				result += nibbleToHex((c & 0xf0) >> 4);
				result += nibbleToHex((c & 0x0f) >> 0);
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
			case NodeType::Int: childStrings.emplace_back(std::move(std::to_string(node->integer))); break;
			case NodeType::Decimal: childStrings.emplace_back(std::move(std::to_string(node->decimal))); break;
			default: throw;
			};
			node = node->nextSibling;
		}
		while(node);
		
		size_t totalChildLength = 0;
		for(auto string : childStrings) { totalChildLength += string.length(); }
		auto isMultiLine = hasMultiLineSubtree || totalChildLength > 100;

		for(uintptr_t childIndex = 0;childIndex < childStrings.size();++childIndex)
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