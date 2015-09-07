#pragma once

#include "Memory.h"

#include <cstdint>
#include <string>
#include <map>

namespace SExp
{
	// The type of a S-expression node.
	enum class NodeType : uint8_t
	{
		Symbol,
		String,
		UnindexedSymbol,
		Int,
		Decimal,
		Error,
		Tree
	};

	// A location in a text file.
	struct TextFileLocus
	{
		uint32_t newlines;
		uint16_t tabs;
		uint16_t characters;

		TextFileLocus(): newlines(0), tabs(0), characters(0) {}

		std::string describe(uint32_t spacesPerTab = 4) const
		{
			return "(" + std::to_string(newlines + 1) + ":" + std::to_string(tabs * spacesPerTab + characters + 1) + ")";
		}
	};

	// A node in a tree of S-expressions
	template<typename Symbol>
	struct GenericNode
	{
		// The type of the node. Determines how to interpret the union.
		NodeType type;
		union
		{
			const char* error;
			Symbol symbol;
			const char* string;
			int64_t integer;
			double decimal;
			GenericNode* children;
		};
		// If type==NodeType::String, contains the length of the string.
		// string[stringLength] will be zero, but stringLength may not be the first zero in the string.
		size_t stringLength;
		// The next node with the same parent.
		GenericNode* nextSibling;
		// The start of this node in the source file.
		TextFileLocus startLocus;
		// The end of this node in the source file.
		TextFileLocus endLocus;

		GenericNode(const TextFileLocus& inStartLocus = TextFileLocus(),NodeType inType = NodeType::Tree)
		:	type(inType)
		,	children(nullptr)
		,	stringLength(0)
		,	nextSibling(nullptr)
		,	startLocus(inStartLocus)
		,	endLocus(inStartLocus)
		{}
	};
	typedef GenericNode<uintptr_t> Node;

	// Iterates over sibling nodes in a S-expression tree.
	template<typename Symbol>
	struct NodeIt
	{
		GenericNode<Symbol>* node;
		SExp::TextFileLocus previousLocus;

		NodeIt(): node(nullptr) {}
		explicit NodeIt(GenericNode<Symbol>* inNode,SExp::TextFileLocus inPreviousLocus = SExp::TextFileLocus())
		: node(inNode), previousLocus(inPreviousLocus) {}

		NodeIt& operator++()
		{
			assert(node);
			previousLocus = node->endLocus;
			node = node->nextSibling;
			return *this;
		}
		NodeIt operator++(int)
		{
			NodeIt copy = *this;
			++(*this);
			return copy;
		}
		
		NodeIt getChildIt() const
		{
			assert(node->type == SExp::NodeType::Tree);
			return NodeIt(node->children,node->startLocus);
		}

		GenericNode<Symbol>* operator->() const { return node; }
		operator GenericNode<Symbol>*() const { return node; }
		operator bool() const { return node != nullptr; }
	};

	// Parses a S-expression tree from a string, allocating nodes from arena, and using symbolIndexMap to map symbols to indices.
	struct StringCompareFunctor { bool operator()(const char* left,const char* right) { return stricmp(left,right) < 0; } };
	typedef std::map<const char*,uintptr_t,StringCompareFunctor> SymbolIndexMap;
	Node* parse(const char* string,Memory::Arena& arena,const SymbolIndexMap& symbolIndexMap);
	template<typename Symbol> GenericNode<Symbol>* parse(const char* string,Memory::Arena& arena,const SymbolIndexMap& symbolIndexMap)
	{
		assert(sizeof(Symbol) == sizeof(uintptr_t));
		return (GenericNode<Symbol>*)parse(string,arena,symbolIndexMap);
	}

	// Prints a S-expression tree to a string.
	std::string print(SExp::Node* rootNode,const char* symbolStrings[]);
}