#include "WAVM.h"
#include "SExpressions.h"
#include <initializer_list>
#include <fstream>

namespace SExp
{
	struct StreamState
	{
		StreamState(const char* inString)
		:	next(inString)
		{}

		void advance()
		{
			switch(*next)
			{
			case 0: throw;
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
		string.shrink(arena);
		auto symbolIndexIt = symbolIndexMap.find(string.c_str());
		if(symbolIndexIt == symbolIndexMap.end())
		{
			node->type = NodeType::UnindexedSymbol;
			node->string = string.c_str();
			node->stringLength = string.length();
		}
		else { node->symbol = symbolIndexIt->second; }

		return node;
	}

	Node* parseNumber(StreamState& state,Memory::Arena& arena)
	{
		auto startLocus = state.getLocus();
		uint64_t accumulator = 0;
		uint64_t denominator = 1;
		bool hasDecimalPoint = false;

		bool negative = false;
		if(state.get() == '+' || state.get() == '-')
		{
			negative = state.get() == '-';
			state.advance();
		}

		while(true)
		{
			auto nextChar = state.get();
			if(isDigit(nextChar))
			{
				if(accumulator * 10 / 10 != accumulator) { throw; }
				accumulator *= 10;
				accumulator += nextChar - '0';
				if(hasDecimalPoint)
				{
					denominator *= 10;
				}
			}
			else if(nextChar == '.')
			{
				if(hasDecimalPoint)
				{
					throw;
				}
				else
				{
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
			auto node = new(arena)Node(startLocus,NodeType::Decimal);
			node->decimal = (double)accumulator / denominator;
			node->endLocus = state.getLocus();
			return node;
		}
		else
		{
			auto node = new(arena)Node(startLocus,NodeType::Int);
			if(negative && -(-(int64_t)accumulator) != (int64_t)accumulator) { throw; }
			node->integer = negative ? -(int64_t)accumulator : +accumulator;
			node->endLocus = state.getLocus();
			return node;
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
				state.advance();
				if(state.get() != ';')
				{
					throw;
				}
				state.advance();

				// Skip line comments.
				while(state.get() != '\n' && state.get() != 0) state.advance();
			}
			else if(nextChar == '(')
			{
				state.advance();

				if(state.get() == ';')
				{
					do
					{
						state.skipToNext(';');
						state.advance();
						if(state.get() == 0)
						{
							throw;
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
						throw;
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
			else if(isDigit(nextChar) || nextChar == '+' || nextChar == '-')
			{
				*nextNodePtr = parseNumber(state,arena);
				nextNodePtr = &(*nextNodePtr)->nextSibling;
			}
			else if(isSymbolCharacter(nextChar))
			{
				*nextNodePtr = parseSymbol(state,arena,symbolIndexMap);
				nextNodePtr = &(*nextNodePtr)->nextSibling;
			}
			else
			{
				throw;
			}
		}
	}

	Node* parse(const char* string,Memory::Arena& arena,const SymbolIndexMap& symbolIndexMap)
	{
		StreamState state(string);
		Node* firstRootNode = nullptr;
		parseRecursive(state,arena,symbolIndexMap,&firstRootNode);
		return firstRootNode;
	}

	void printRecursive(SExp::Node* node,const char* symbolStrings[],std::string& outString,const char* newline)
	{
		do
		{
			switch(node->type)
			{
			case NodeType::Tree:
				outString += "(";
				printRecursive(node->children,symbolStrings,outString,(std::string(newline) + "\t").c_str());
				outString += ")";
				break;
			case NodeType::Symbol: outString += symbolStrings[node->symbol]; break;
			case NodeType::UnindexedSymbol: outString += node->string; break;
			case NodeType::String: outString += '\"'; outString += node->string; outString += '\"'; break;
			case NodeType::Error: outString += node->error; break;
			case NodeType::Int: outString += std::to_string(node->integer); break;
			case NodeType::Decimal: outString += std::to_string(node->decimal); break;
			default: throw;
			};
			outString += newline;
			node = node->nextSibling;
		}
		while(node);
	}

	std::string print(SExp::Node* rootNode,const char* symbolStrings[])
	{
		std::string result;
		printRecursive(rootNode,symbolStrings,result,"\n");
		return result;
	}
}