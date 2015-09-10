#pragma once

#include "Memory.h"

#include <cstdint>
#include <string>
#include <map>

#pragma warning (disable:4512)	// assignment operator could not be generated

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
	struct Node
	{
		// The type of the node. Determines how to interpret the union.
		NodeType type;
		union
		{
			const char* error;
			uintptr_t symbol;
			const char* string;
			int64_t integer;
			double decimal;
			Node* children;
		};
		// If type==NodeType::String, contains the length of the string.
		// string[stringLength] will be zero, but stringLength may not be the first zero in the string.
		size_t stringLength;
		// The next node with the same parent.
		Node* nextSibling;
		// The start of this node in the source file.
		TextFileLocus startLocus;
		// The end of this node in the source file.
		TextFileLocus endLocus;

		Node(const TextFileLocus& inStartLocus = TextFileLocus(),NodeType inType = NodeType::Tree)
		:	type(inType)
		,	children(nullptr)
		,	stringLength(0)
		,	nextSibling(nullptr)
		,	startLocus(inStartLocus)
		,	endLocus(inStartLocus)
		{}
	};

	// Iterates over sibling nodes in a S-expression tree.
	struct NodeIt
	{
		Node* node;
		SExp::TextFileLocus previousLocus;

		NodeIt(): node(nullptr) {}
		explicit NodeIt(Node* inNode,SExp::TextFileLocus inPreviousLocus = SExp::TextFileLocus())
		: node(inNode), previousLocus(inPreviousLocus) {}

		NodeIt& operator++()
		{
			if(node)
			{
				previousLocus = node->endLocus;
				node = node->nextSibling;
			}
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

		Node* operator->() const { return node; }
		operator Node*() const { return node; }
		operator bool() const { return node != nullptr; }
	};

	// An output stream for S-expression nodes.
	struct NodeOutputStream
	{
		// Used to distinguish an unindexed symbol from a quoted string, which may contain zeroes other than its terminator.
		struct StringAtom
		{
			const char* string;
			size_t length;
			StringAtom(const char* inString,size_t inLength) : string(inString), length(inLength) {}
			StringAtom(const std::string& inString) : string(inString.c_str()), length(inString.length()) {}
		};
		
		NodeOutputStream(Memory::Arena& inArena): arena(inArena), rootNode(nullptr), nextNodeLink(&rootNode) {}

		// Appends a subtree node, and sets the stream to append to its children.
		void enterSubtree()
		{
			Node* subtreeNode = new(arena) Node();
			*nextNodeLink = subtreeNode;
			nextNodeLink = &subtreeNode->children;
		}

		// Append a substream's nodes.
		NodeOutputStream& operator<<(const NodeOutputStream& substream)
		{
			append(substream.getRoot());
			return *this;
		}
		// Append a S-expression node.
		NodeOutputStream& operator<<(Node* node)
		{
			append(node);
			return *this;
		}
		// Append an unindexed symbol.
		NodeOutputStream& operator<<(const char* string)
		{
			auto stringLength = strlen(string);
			auto symbolNode = new(arena) Node();
			symbolNode->type = SExp::NodeType::UnindexedSymbol;
			symbolNode->string = arena.copyToArena(string,stringLength + 1);;
			symbolNode->stringLength = stringLength;
			append(symbolNode);
			return *this;
		}
		NodeOutputStream& operator<<(const std::string& string) { return *this << string.c_str(); }
		NodeOutputStream& operator<<(bool b) { appendInt(b ? 1 : 0); return *this; }
		NodeOutputStream& operator<<(uint8_t i) { appendInt(i); return *this; }
		NodeOutputStream& operator<<(uint16_t i) { appendInt(i); return *this; }
		NodeOutputStream& operator<<(uint32_t i) { appendInt(i); return *this; }
		NodeOutputStream& operator<<(uint64_t i) { appendInt(i); return *this; }
		NodeOutputStream& operator<<(intptr_t i) { appendInt(i); return *this; }
		NodeOutputStream& operator<<(double d)
		{
			auto decimalNode = new(arena) Node();
			decimalNode->type = SExp::NodeType::Decimal;
			decimalNode->decimal = d;
			append(decimalNode);
			return *this;
		}
		
		NodeOutputStream& operator<<(const StringAtom& string)
		{
			auto intNode = new(arena) Node();
			intNode->type = SExp::NodeType::String;
			intNode->string = string.string;
			intNode->stringLength = string.length;
			append(intNode);
			return *this;
		}
		
		Memory::Arena& getArena() { return arena; }
		Node* getRoot() const { return rootNode; }

	private:
		Memory::Arena& arena;
		Node* rootNode;
		Node** nextNodeLink;

		void append(Node* node)
		{
			*nextNodeLink = node;
			while(node->nextSibling) { node = node->nextSibling; };
			nextNodeLink = &node->nextSibling;
		}

		template<typename T> void appendInt(T i)
		{
			auto intNode = new(arena) Node();
			intNode->type = SExp::NodeType::Int;
			intNode->integer = i;
			append(intNode);
		}
	};

	// Parses a S-expression tree from a string, allocating nodes from arena, and using symbolIndexMap to map symbols to indices.
	struct StringCompareFunctor { bool operator()(const char* left,const char* right) { return strcmp(left,right) < 0; } };
	typedef std::map<const char*,uintptr_t,StringCompareFunctor> SymbolIndexMap;
	Node* parse(const char* string,Memory::Arena& arena,const SymbolIndexMap& symbolIndexMap);

	// Prints a S-expression tree to a string.
	std::string print(SExp::Node* rootNode,const char* symbolStrings[]);
}