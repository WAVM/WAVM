#include "Core/Core.h"
#include "NFA.h"
#include "Regexp.h"
#include "WAST.h"
#include "Lexer.h"
#include "IR/Operators.h"

#ifdef _DEBUG
	#include <fstream>
#endif

namespace WAST
{
	const char* describeToken(TokenType tokenType)
	{
		assert(tokenType < numTokenTypes);
		static const char* tokenDescriptions[] =
		{
			// This ENUM_TOKENS must come before the literalTokenPairs definition that redefines VISIT_OPERATOR_TOKEN.
			#define VISIT_TOKEN(name,description) description,
			ENUM_TOKENS()
			#undef VISIT_TOKEN
		};
		return tokenDescriptions[tokenType];
	}
	
	struct StaticData
	{
		NFA::Machine nfaMachine;
		StaticData();
	};

	static void addLiteralToNFA(const char* string,NFA::Builder* builder,NFA::StateIndex initialState,NFA::StateIndex finalState)
	{
		// Add the literal to the NFA, one character at a time, reusing existing states that are reachable by the same string.
		for(const char* nextChar = string;*nextChar;++nextChar)
		{
			NFA::StateIndex nextState = NFA::getNonTerminalEdge(builder,initialState,*nextChar);
			if(nextState < 0 || nextChar[1] == 0)
			{
				nextState = nextChar[1] == 0 ? finalState : addState(builder);
				NFA::addEdge(builder,initialState,NFA::CharSet(*nextChar),nextState);
			}
			initialState = nextState;
		}
	}

	StaticData::StaticData()
	{
		static const std::pair<TokenType,const char*> regexpTokenPairs[] =
		{
			{t_decimalInt,"[+\\-]?\\d+"},
			{t_decimalFloat,"[+\\-]?\\d+\\.\\d*([eE][+\\-]?\\d+)?"},
			{t_decimalFloat,"[+\\-]?\\d+[eE][+\\-]?\\d+"},

			{t_hexInt,"[+\\-]?0[xX][\\da-fA-F]+"},
			{t_hexFloat,"[+\\-]?0[xX][\\da-fA-F]+\\.[\\da-fA-F]*([pP][+\\-]?\\d+)?"},
			{t_hexFloat,"[+\\-]?0[xX][\\da-fA-F]+[pP][+\\-]?\\d+"},

			{t_floatNaN,"[+\\-]?nan(:0[xX][\\da-fA-F]+)?"},
			{t_floatInf,"[+\\-]?inf(inity)?"},

			{t_string,"\"([^\"\n\\\\]*(\\\\([^0-9a-fA-F]|[0-9a-fA-F][0-9a-fA-F])))*\""},

			{t_name,"\\$[a-zA-Z0-9\'_+*/~=<>!?@#$%&|:`.\\-\\^\\\\]+"},
		};

		static const std::pair<TokenType,const char*> literalTokenPairs[] =
		{
			{t_leftParenthesis,"("},
			{t_rightParenthesis,")"},
			{t_equals,"="},

			#define VISIT_TOKEN(name,description) {t_##name,#name},
			ENUM_LITERAL_TOKENS()
			#undef VISIT_TOKEN

			#undef VISIT_OPERATOR_TOKEN
			#define VISIT_OPERATOR_TOKEN(_,name,nameString,...) {t_##name,nameString},
			ENUM_OPERATORS(VISIT_OPERATOR_TOKEN)
			#undef VISIT_OPERATOR_TOKEN
		};
	
		Core::Timer timer;

		NFA::Builder* nfaBuilder = NFA::createBuilder();
		
		for(auto regexpTokenPair : regexpTokenPairs)
		{
			Regexp::addToNFA(regexpTokenPair.second,nfaBuilder,0,NFA::maximumTerminalStateIndex - (NFA::StateIndex)regexpTokenPair.first);
		}
		
		for(auto literalTokenPair : literalTokenPairs)
		{
			addLiteralToNFA(literalTokenPair.second,nfaBuilder,0,NFA::maximumTerminalStateIndex - (NFA::StateIndex)literalTokenPair.first);
		}

		if(_DEBUG)
		{
			std::ofstream debugGraphStream("nfaGraph.dot");
			debugGraphStream << NFA::dumpNFAGraphViz(nfaBuilder).c_str();
			debugGraphStream.close();
		}

		nfaMachine = NFA::Machine(nfaBuilder);

		if(_DEBUG)
		{
			std::ofstream debugGraphStream("dfaGraph.dot");
			debugGraphStream << nfaMachine.dumpDFAGraphViz().c_str();
			debugGraphStream.close();
		}

		Log::logTimer("built lexer tables",timer);
	}

	struct LineInfo
	{
		uint32* lineStarts;
		uint32 numLineStarts;
	};

	inline bool isRecoveryPointChar(char c)
	{
		switch(c)
		{
		// Recover lexing at the next whitespace or parenthesis.
		case ' ': case '\t': case '\r': case '\n': case '\f':
		case '(': case ')':
			return true;
		default:
			return false;
		};
	}

	Token* lex(const char* string,size_t stringLength,LineInfo*& outLineInfo)
	{
		static StaticData staticData;
		
		Core::Timer timer;

		if(stringLength > UINT32_MAX)
		{
			Core::errorf("cannot lex strings with more than %u characters",UINT32_MAX);
		}
		
		// Allocate enough memory up front for a token and newline for each character in the input string.
		Token* tokens = (Token*)malloc(sizeof(Token) * stringLength);
		uint32* lineStarts = (uint32*)malloc(sizeof(uint32) * (stringLength + 1));

		Token* nextToken = tokens;
		uint32* nextLineStart = lineStarts;
		*nextLineStart++ = 0;

		const char* nextChar = string;
		while(true)
		{
			// Skip whitespace and comments (keeping track of newlines).
			while(true)
			{
				switch(*nextChar)
				{
				// Single line comments.
				case ';':
					if(nextChar[1] != ';') { goto doneSkippingWhitespace; }
					else
					{
						nextChar += 2;
						while(*nextChar)
						{
							if(*nextChar == '\n')
							{
								// Emit a line start for the newline.
								*nextLineStart++ = uint32(nextChar - string + 1);
								++nextChar;
								break;
							}
							++nextChar;
						};
					}
					break;
				// Delimited (possibly multi-line) comments.
				case '(':
					if(nextChar[1] != ';') { goto doneSkippingWhitespace; }
					else
					{
						const char* firstCommentChar = nextChar;
						nextChar += 2;
						uint32 commentDepth = 1;
						while(commentDepth)
						{
							if(nextChar[0] == ';' && nextChar[1] == ')')
							{
								--commentDepth;
								nextChar += 2;
							}
							else if(nextChar[0] == '(' && nextChar[1] == ';')
							{
								++commentDepth;
								nextChar += 2;
							}
							else if(*nextChar == 0)
							{
								// Emit an unterminated comment token.
								nextToken->type = t_unterminatedComment;
								nextToken->begin = uint32(firstCommentChar - string);
								++nextToken;
								goto doneSkippingWhitespace;
							}
							else
							{
								if(*nextChar == '\n')
								{
									// Emit a line start for the newline.
									*nextLineStart++ = uint32(nextChar - string);
								}
								++nextChar;
							}
						};
					}
					break;
				// Whitespace.
				case '\n':
					*nextLineStart++ = uint32(nextChar - string + 1);
					++nextChar;
					break;
				case ' ': case '\t': case '\r': case '\f':
					++nextChar;
					break;
				default:
					goto doneSkippingWhitespace;
				};
			}
			doneSkippingWhitespace:

			// Once we reach a non-whitespace, non-comment character, feed characters into the NFA until it reaches a terminal state.
			nextToken->begin = uint32(nextChar - string);
			NFA::StateIndex terminalState = staticData.nfaMachine.feed(nextChar);
			if(terminalState != NFA::unmatchedCharacterTerminal)
			{
				nextToken->type = TokenType(NFA::maximumTerminalStateIndex - (NFA::StateIndex)terminalState);
				++nextToken;
			}
			else
			{
				if(*(string + nextToken->begin) != 0)
				{
					// Emit an unrecognized token.
					nextToken->type = t_unrecognized;
					++nextToken;

					// Advance until a recovery point.
					while(!isRecoveryPointChar(*nextChar)) { ++nextChar; }
				}
				else
				{
					break;
				}
			}
		}

		// Emit an end token to mark the end of the token stream.
		nextToken->type = t_eof;
		++nextToken;
		
		// Emit an extra line start for the end of the file, so you can find the end of a line with lineStarts[line + 1].
		*nextLineStart++ = uint32(nextChar - string);

		// Shrink the line start and token arrays to the final number of tokens/lines.
		const size_t numLineStarts = nextLineStart - lineStarts;
		const size_t numTokens = nextToken - tokens;
		lineStarts = (uint32*)realloc(lineStarts,sizeof(uint32) * numLineStarts);
		tokens = (Token*)realloc(tokens,sizeof(Token) * numTokens);

		// Create the LineInfo object that encapsulates the line start information.
		outLineInfo = new LineInfo {lineStarts,uint32(numLineStarts)};

		Log::logRatePerSecond("lexed WAST file",timer,stringLength/1024.0/1024.0,"MB");
		Log::printf(Log::Category::metrics,"lexer produced %u tokens (%.1fMB)\n",numTokens,numTokens*sizeof(Token)/1024.0/1024.0);

		return tokens;
	}
	
	void freeTokens(Token* tokens)
	{
		free(tokens);
	}

	void freeLineInfo(LineInfo* lineInfo)
	{
		free(lineInfo->lineStarts);
		delete lineInfo;
	}
	
	static uintp getLineOffset(const LineInfo* lineInfo,uintp lineIndex)
	{
		errorUnless(lineIndex < lineInfo->numLineStarts);
		return lineInfo->lineStarts[lineIndex];
	}

	TextFileLocus calcLocusFromOffset(const char* string,const LineInfo* lineInfo,uintp charOffset)
	{
		// Binary search the line starts for the last one before charIndex.
		uintp minLineIndex = 0;
		uintp maxLineIndex = lineInfo->numLineStarts - 1;
		while(maxLineIndex > minLineIndex)
		{
			const uintp medianLineIndex = (minLineIndex + maxLineIndex + 1) / 2;
			if(charOffset < lineInfo->lineStarts[medianLineIndex])
			{
				maxLineIndex = medianLineIndex - 1;
			}
			else if(charOffset > lineInfo->lineStarts[medianLineIndex])
			{
				minLineIndex = medianLineIndex;
			}
			else
			{
				minLineIndex = maxLineIndex = medianLineIndex;
			}
		};
		TextFileLocus result;
		result.newlines = (uint32)minLineIndex;

		// Count tabs and and spaces from the beginning of the line to charIndex.
		for(uint32 index = lineInfo->lineStarts[result.newlines];index < charOffset;++index)
		{
			if(string[index] == '\t') { ++result.tabs; }
			else { ++result.characters; }
		}

		// Copy the full source line into the TextFileLocus for context.
		const uintp lineStartOffset = getLineOffset(lineInfo,result.newlines);
		uintp lineEndOffset = getLineOffset(lineInfo,result.newlines+1);
		if(string[lineEndOffset-1] == '\n') { --lineEndOffset; }
		result.sourceLine = std::string(string + lineStartOffset,string + lineEndOffset);

		return result;
	}
}