#pragma once

#include "Core/Core.h"
#include "Core/MemoryArena.h"

#ifdef _WIN32
	#pragma warning (disable:4512)	// assignment operator could not be generated
#endif

namespace SExp
{
	// The type of a S-expression node.
	enum class SNodeType : uint8
	{
		Symbol,
		String,
		UnindexedSymbol,
		Name,
		SignedInt,
		UnsignedInt,
		Float,
		Error,
		Tree,
		Attribute // A tree with two children implicitly separated by an '='
	};

	// A node in a tree of S-expressions
	struct SNode
	{
		// The type of the node. Determines how to interpret the union.
		SNodeType type;
		union
		{
			const char* error;
			uintp symbol;
			const char* string;
			int64 i64;
			uint64 u64;
			float64 f64;
			SNode* children;
		};
		union
		{
			// If type==NodeType::String, contains the length of the string.
			// string[stringLength] will be zero, but stringLength may not be the first zero in the string.
			size_t stringLength;
			// If type==NodeType::Float, contains a 32-bit float representation of the node.
			float32 f32;
		};
		// The next node with the same parent.
		SNode* nextSibling;
		// The start of this node in the source file.
		Core::TextFileLocus startLocus;
		// The end of this node in the source file.
		Core::TextFileLocus endLocus;

		SNode(const Core::TextFileLocus& inStartLocus = Core::TextFileLocus(),SNodeType inType = SNodeType::Tree)
		:	type(inType)
		,	children(nullptr)
		,	stringLength(0)
		,	nextSibling(nullptr)
		,	startLocus(inStartLocus)
		,	endLocus(inStartLocus)
		{}
	};

	// Iterates over sibling nodes in a S-expression tree.
	struct SNodeIt
	{
		SNode* node;
		Core::TextFileLocus previousLocus;

		SNodeIt(): node(nullptr) {}
		explicit SNodeIt(SNode* inNode,Core::TextFileLocus inPreviousLocus = Core::TextFileLocus())
		: node(inNode), previousLocus(inPreviousLocus) {}

		SNodeIt& operator++()
		{
			if(node)
			{
				previousLocus = node->endLocus;
				node = node->nextSibling;
			}
			return *this;
		}
		SNodeIt operator++(int)
		{
			SNodeIt copy = *this;
			++(*this);
			return copy;
		}
		
		SNodeIt getChildIt() const
		{
			assert(node->type == SNodeType::Tree || node->type == SNodeType::Attribute);
			return SNodeIt(node->children,node->startLocus);
		}

		SNode* operator->() const { return node; }
		operator SNode*() const { return node; }
		operator bool() const { return node != nullptr; }
	};

	// Parses a S-expression tree from a string, allocating nodes from arena, and using symbolIndexMap to map symbols to indices.
	struct StringCompareFunctor { bool operator()(const char* left,const char* right) const
	{
		while(true)
		{
			if(*left == *right)
			{
				if(!*left) { return false; }
			}
			else if(*left < *right)
			{
				return true;
			}
			else
			{
				return false;
			}
			++left;
			++right;
		};
	}};
	typedef std::map<const char*,uintp,StringCompareFunctor> SymbolIndexMap;
	CORE_API SNode* parse(const char* string,MemoryArena::Arena& arena,const SymbolIndexMap& symbolIndexMap);

	// Parse an integer from a S-expression node.
	inline bool parseInt(SNodeIt& nodeIt,int32& outInt)
	{
		if(nodeIt && nodeIt->type == SNodeType::SignedInt && nodeIt->u64 <= (uint64)-int64(INT32_MIN))	{ outInt = (int32)-nodeIt->i64; ++nodeIt; return true; }
		if(nodeIt && nodeIt->type == SNodeType::UnsignedInt && nodeIt->u64 <= (uint64)UINT32_MAX)	{ outInt = (int32)nodeIt->u64; ++nodeIt; return true; }
		else { return false; }
	}
	inline bool parseInt(SNodeIt& nodeIt,int64& outInt)
	{
		if(nodeIt && nodeIt->type == SNodeType::SignedInt && nodeIt->u64 <= (uint64)-INT64_MIN)	{ outInt = -nodeIt->i64; ++nodeIt; return true; }
		if(nodeIt && nodeIt->type == SNodeType::UnsignedInt)									{ outInt = nodeIt->u64; ++nodeIt; return true; }
		else { return false; }
	}
	inline bool parseUnsignedInt(SNodeIt& nodeIt,uint64& outInt)
	{
		if(nodeIt && nodeIt->type == SNodeType::UnsignedInt) { outInt = nodeIt->u64; ++nodeIt; return true; }
		else { return false; }
	}

	// Parse a float from a S-expression node.
	inline bool parseFloat(SNodeIt& nodeIt,float32& outF32)
	{
		if(nodeIt && nodeIt->type == SNodeType::Float) { outF32 = nodeIt->f32;++nodeIt; return true; }
		else if(nodeIt && nodeIt->type == SNodeType::SignedInt) { outF32 = -(float32)nodeIt->u64; ++nodeIt; return true; }
		else if(nodeIt && nodeIt->type == SNodeType::UnsignedInt) { outF32 = (float32)nodeIt->u64; ++nodeIt; return true; }
		else { return false; }
	}
	inline bool parseFloat(SNodeIt& nodeIt,float64& outF64)
	{
		if(nodeIt && nodeIt->type == SNodeType::Float) { outF64 = nodeIt->f64;++nodeIt; return true; }
		else if(nodeIt && nodeIt->type == SNodeType::SignedInt) { outF64 = -(float64)nodeIt->u64; ++nodeIt; return true; }
		else if(nodeIt && nodeIt->type == SNodeType::UnsignedInt) { outF64 = (float64)nodeIt->u64; ++nodeIt; return true; }
		else { return false; }
	}

	// Parse a string from a S-expression node.
	inline bool parseString(SNodeIt& nodeIt,std::string& outString)
	{
		if(nodeIt && nodeIt->type == SNodeType::String)
		{
			outString = std::string(nodeIt->string,nodeIt->stringLength);
			++nodeIt;
			return true;
		}
		else { return false; }
	}
	
	// Parse a S-expression tree or attribute node. Upon success, outChildIt is set to the node's first child.
	inline bool parseTreelikeNode(SNodeIt nodeIt,SNodeType nodeType,SNodeIt& outChildIt)
	{
		if(nodeIt && nodeIt->type == nodeType) { outChildIt = nodeIt.getChildIt(); return true; }
		else { return false; }
	}
	inline bool parseTreeNode(SNodeIt nodeIt,SNodeIt& outChildIt) { return parseTreelikeNode(nodeIt,SNodeType::Tree,outChildIt); }
	inline bool parseAttributeNode(SNodeIt nodeIt,SNodeIt& outChildIt) { return parseTreelikeNode(nodeIt,SNodeType::Attribute,outChildIt); }
	
	// Parse a S-expression symbol node. Upon success, outSymbol is set to the parsed symbol.
	template<typename Symbol>
	bool parseSymbol(SNodeIt& nodeIt,Symbol& outSymbol)
	{
		if(nodeIt && nodeIt->type == SNodeType::Symbol) { outSymbol = (Symbol)nodeIt->symbol; ++nodeIt; return true; }
		else { return false; }
	}

	// Parse a S-expression tree node whose first child is a symbol. Sets outChildren to the first child after the symbol on success.
	template<typename Symbol>
	bool parseTaggedNode(SNodeIt nodeIt,Symbol tagSymbol,SNodeIt& outChildIt)
	{
		Symbol symbol;
		return parseTreeNode(nodeIt,outChildIt) && parseSymbol(outChildIt,symbol) && symbol == tagSymbol;
	}
	
	// Parse a S-expression attribute node whose first child is a symbol. Sets outValue to the attribute value.
	template<typename Symbol>
	bool parseSymbolAttribute(SNodeIt nodeIt,Symbol keySymbol,SNodeIt& outValueIt)
	{
		Symbol symbol;
		return parseAttributeNode(nodeIt,outValueIt) && parseSymbol(outValueIt,symbol) && symbol == keySymbol;
	}

	// Tries to parse a name from a SExp node (a string symbol starting with a $).
	// On success, returns true, sets outString to the name's string, and advances node to the sibling following the name.
	inline bool parseName(SNodeIt& nodeIt,const char*& outString)
	{
		if(nodeIt && nodeIt->type == SNodeType::Name)
		{
			outString = nodeIt->string;
			++nodeIt;
			return true;
		}
		else
		{
			return false;
		}
	}
}
