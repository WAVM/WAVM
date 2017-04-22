#pragma once

#include "Core/Core.h"
#include "WAST.h"
#include "Lexer.h"
#include "IR/Types.h"
#include "IR/Module.h"

#include <functional>
#include <map>
#include <unordered_map>

namespace WAST
{
	struct FatalParseException {};
	struct RecoverParseException {};

	// Like WAST::Error, but only has an offset in the input string instead of a full TextFileLocus.
	struct UnresolvedError
	{
		uintp charOffset;
		std::string message;
		UnresolvedError(uintp inCharOffset,std::string&& inMessage)
		: charOffset(inCharOffset), message(inMessage) {}
	};

	// The state that's threaded through the various parsers.
	struct ParseState
	{
		const char* string;
		const LineInfo* lineInfo;
		std::vector<UnresolvedError>& errors;
		const Token* nextToken;
		
		ParseState(const char* inString,const LineInfo* inLineInfo,std::vector<UnresolvedError>& inErrors,const Token* inNextToken)
		: string(inString)
		, lineInfo(inLineInfo)
		, errors(inErrors)
		, nextToken(inNextToken)
		{}
	};

	// Encapsulates a name ($whatever) parsed from the WAST file.
	// References the characters in the input string the name was parsed from,
	// so it can handle arbitrary length names in a fixed size struct, but only
	// as long as the input string isn't freed.
	// Includes a hash of the name's characters.
	struct Name
	{
		Name(): begin(nullptr), numChars(0), hash(0) {}
		Name(const char* inBegin,uint32 inNumChars)
		: begin(inBegin)
		, numChars(inNumChars)
		, hash(calcHash(inBegin,inNumChars))
		{}

		operator bool() const { return begin != nullptr; }
		std::string getString() const { return begin ? std::string(begin + 1,numChars - 1) : std::string(); }
		uintp getCharOffset(const char* string) const { return begin - string; }

		friend bool operator==(const Name& a,const Name& b)
		{
			return a.hash == b.hash && a.numChars == b.numChars && memcmp(a.begin,b.begin,a.numChars) == 0;
		}
		friend bool operator!=(const Name& a,const Name& b)
		{
			return !(a == b);
		}
	
		struct Hasher
		{
			size_t operator()(const Name& name) const
			{
				return name.hash;
			}
		};

	private:

		const char* begin;
		uint32 numChars;
		uint32 hash;

		static uint32 calcHash(const char* begin,uint32 numChars);
	};
	
	// A map from Name to index using a hash table.
	typedef std::unordered_map<Name,uintp,Name::Hasher> NameToIndexMap;
	
	// Represents a yet-to-be-resolved reference, parsed as either a name or an index.
	struct Reference
	{
		enum class Type { invalid, name, index };
		Type type;
		union
		{
			Name name;
			uint64 index;
		};
		const Token* token;
		Reference(const Name& inName): type(Type::name), name(inName) {}
		Reference(uint64 inIndex): type(Type::index), index(inIndex) {}
		Reference(): type(Type::invalid), token(nullptr) {}
		operator bool() const { return type != Type::invalid; }
	};

	// State associated with parsing a module.
	struct ModuleParseState : ParseState
	{
		IR::Module& module;

		std::map<const IR::FunctionType*,uintp> functionTypeToIndexMap;
		NameToIndexMap typeNameToIndexMap;

		NameToIndexMap functionNameToIndexMap;
		NameToIndexMap tableNameToIndexMap;
		NameToIndexMap memoryNameToIndexMap;
		NameToIndexMap globalNameToIndexMap;

		IR::DisassemblyNames disassemblyNames;

		// Thunks that are called after parsing all declarations.
		std::vector<std::function<void(ModuleParseState&)>> postDeclarationCallbacks;

		ModuleParseState(const char* inString,const LineInfo* inLineInfo,std::vector<UnresolvedError>& inErrors,const Token* inNextToken,IR::Module& inModule)
		: ParseState(inString,inLineInfo,inErrors,inNextToken)
		, module(inModule)
		{}
	};
	
	// Error handling.
	void parseErrorf(ParseState& state,uintp charOffset,const char* messageFormat,...);
	void parseErrorf(ParseState& state,const char* nextChar,const char* messageFormat,...);
	void parseErrorf(ParseState& state,const Token* nextToken,const char* messageFormat,...);

	void require(ParseState& state,TokenType type);

	// Type parsing and uniqueing
	bool tryParseValueType(ParseState& state,IR::ValueType& outValueType);
	bool tryParseResultType(ParseState& state,IR::ResultType& outResultType);
	IR::ValueType parseValueType(ParseState& state);

	const IR::FunctionType* parseFunctionType(ModuleParseState& state,NameToIndexMap& outLocalNameToIndexMap,std::vector<std::string>& outLocalDisassemblyNames);
	IR::IndexedFunctionType parseFunctionTypeRef(ModuleParseState& state);
	IR::IndexedFunctionType getUniqueFunctionTypeIndex(ModuleParseState& state,const IR::FunctionType* functionType);

	// Literal parsing.
	bool tryParseHexit(const char*& nextChar,uint8& outValue);
	
	bool tryParseI64(ParseState& state,uint64& outI64);

	uint32 parseI32(ParseState& state);
	uint64 parseI64(ParseState& state);
	float32 parseF32(ParseState& state);
	float64 parseF64(ParseState& state);

	bool tryParseString(ParseState& state,std::string& outString);

	std::string parseUTF8String(ParseState& state);

	// Name parsing and resolution.
	bool tryParseName(ParseState& state,Name& outName);
	bool tryParseNameOrIndexRef(ParseState& state,Reference& outRef);
	uintp parseAndResolveNameOrIndexRef(ParseState& state,const NameToIndexMap& nameToIndexMap,const char* context);

	void bindName(ParseState& state,NameToIndexMap& nameToIndexMap,const Name& name,uintp index);
	uintp resolveRef(ParseState& state,const NameToIndexMap& nameToIndexMap,const Reference& ref);

	// Finds the parenthesis closing the current s-expression.
	void findClosingParenthesis(ParseState& state,const Token* openingParenthesisToken);

	// Parses the surrounding parentheses for an inner parser, and handles recovery at the closing parenthesis.
	template<typename ParseInner>
	static void parseParenthesized(ParseState& state,ParseInner parseInner)
	{
		const Token* openingParenthesisToken = state.nextToken;
		require(state,t_leftParenthesis);
		try
		{
			parseInner();
			require(state,t_rightParenthesis);
		}
		catch(RecoverParseException)
		{
			findClosingParenthesis(state,openingParenthesisToken);
		}
	}

	// Tries to parse '(' tagType parseInner ')', handling recovery at the closing parenthesis.
	// Returns true if any tokens were consumed.
	template<typename ParseInner>
	static bool tryParseParenthesizedTagged(ParseState& state,TokenType tagType,ParseInner parseInner)
	{
		const Token* openingParenthesisToken = state.nextToken;
		if(state.nextToken[0].type != t_leftParenthesis || state.nextToken[1].type != tagType)
		{
			return false;
		}
		try
		{
			state.nextToken += 2;
			parseInner();
			require(state,t_rightParenthesis);
		}
		catch(RecoverParseException)
		{
			findClosingParenthesis(state,openingParenthesisToken);
		}
		return true;
	}

	// Function parsing.
	IR::FunctionDef parseFunctionDef(ModuleParseState& state,const Token* funcToken);

	// Module parsing.
	void parseModuleBody(ModuleParseState& state);
};