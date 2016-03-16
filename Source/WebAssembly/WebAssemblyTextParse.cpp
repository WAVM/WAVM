#include "Core/Core.h"
#include "Core/Floats.h"
#include "Core/MemoryArena.h"
#include "Core/SExpressions.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "AST/ASTDispatch.h"
#include "WebAssemblyTextSymbols.h"
#include "WebAssembly.h"

#include <map>

using namespace AST;

#ifdef _WIN32
	#pragma warning(disable:4702) // unreachable code
#endif

namespace WebAssemblyText
{
	typedef SExp::Node SNode;
	typedef SExp::NodeIt SNodeIt;
	
	typedef std::map<const char*,uintptr,Core::StringCompareFunctor> NameToIndexMap;
	typedef std::map<const char*,BranchTarget*,Core::StringCompareFunctor> NameToBranchTargetMap;

	// Describes a S-expression node briefly for parsing error messages.
	std::string describeSNode(SNode* node)
	{
		if(!node) { return "null"; }
		else
		{
			switch(node->type)
			{
			case SExp::NodeType::Tree: return "(" + describeSNode(node->children) + "...";
			case SExp::NodeType::Attribute: return describeSNode(node->children) + "=...";
			case SExp::NodeType::Symbol: return wastSymbols[node->symbol];
			case SExp::NodeType::SignedInt: return std::to_string(node->i64);
			case SExp::NodeType::UnsignedInt: return std::to_string(node->u64);
			case SExp::NodeType::Float: return std::to_string(node->f64);
			case SExp::NodeType::Error: return std::string("error") + node->startLocus.describe() + ": " + node->error;
			case SExp::NodeType::String: return node->string;
			case SExp::NodeType::UnindexedSymbol: return node->string;
			default: throw;
			}
		}
	}

	// Creates and records an error with the given message and location (taken from nodeIt).
	template<typename Error> Error* recordError(std::vector<ErrorRecord*>& outErrors,SNodeIt nodeIt,std::string&& message)
	{
		auto locus = nodeIt.node ? nodeIt.node->startLocus : nodeIt.previousLocus;
		auto error = new Error(locus.describe() + ": " + message + " (S-expression node is " + describeSNode(nodeIt) + ")");
		outErrors.push_back(error);
		return error;
	}

	template<typename Error> Error* recordExcessInputError(std::vector<ErrorRecord*>& outErrors,SNodeIt nodeIt,const char* errorContext)
	{
		auto message = std::string("unexpected input following ") + errorContext;
		return recordError<Error>(outErrors,nodeIt,std::move(message));
	}

	// Parse a type from a S-expression symbol.
	bool parseType(SNodeIt& nodeIt,TypeId& outType)
	{
		if(nodeIt
			&& nodeIt->type == SExp::NodeType::Symbol
			&& nodeIt->symbol > (uintptr)Symbol::_typeBase
			&& nodeIt->symbol <= ((uintptr)Symbol::_typeBase + (uintptr)TypeId::max))
		{
			outType = (TypeId)(nodeIt->symbol - (uintptr)Symbol::_typeBase);
			++nodeIt;
			return true;
		}
		else { return false; }
	}

	// Parse an integer from a S-expression node.
	bool parseInt(SNodeIt& nodeIt,int64& outInt)
	{
		if(nodeIt && nodeIt->type == SExp::NodeType::SignedInt)		{ outInt = nodeIt->i64; ++nodeIt; return true; }
		if(nodeIt && nodeIt->type == SExp::NodeType::UnsignedInt)	{ outInt = nodeIt->u64; ++nodeIt; return true; }
		else { return false; }
	}
	bool parseSignedInt(SNodeIt& nodeIt,int64& outInt)
	{
		if(nodeIt && nodeIt->type == SExp::NodeType::SignedInt)									{ outInt = nodeIt->i64; ++nodeIt; return true; }
		if(nodeIt && nodeIt->type == SExp::NodeType::UnsignedInt && nodeIt->u64 <= INT64_MAX)	{ outInt = nodeIt->u64; ++nodeIt; return true; }
		else { return false; }
	}
	bool parseUnsignedInt(SNodeIt& nodeIt,uint64& outInt)
	{
		if(nodeIt && nodeIt->type == SExp::NodeType::SignedInt && nodeIt->i64 >= 0)	{ outInt = nodeIt->i64; ++nodeIt; return true; }
		if(nodeIt && nodeIt->type == SExp::NodeType::UnsignedInt)					{ outInt = nodeIt->u64; ++nodeIt; return true; }
		else { return false; }
	}

	// Parse a float from a S-expression node.
	bool parseFloat(SNodeIt& nodeIt,float64& outF64,float32& outF32)
	{
		if(nodeIt && nodeIt->type == SExp::NodeType::Float) { outF64 = nodeIt->f64; outF32 = nodeIt->f32; ++nodeIt; return true; }
		else if(nodeIt && nodeIt->type == SExp::NodeType::SignedInt) { outF64 = (float64)nodeIt->i64; outF32 = (float32)nodeIt->i64; ++nodeIt; return true; }
		else if(nodeIt && nodeIt->type == SExp::NodeType::UnsignedInt) { outF64 = (float64)nodeIt->u64; outF32 = (float32)nodeIt->u64; ++nodeIt; return true; }
		else { return false; }
	}

	// Parse a string from a S-expression node. Upon success, the string is copied into the provided memory arena.
	bool parseString(SNodeIt& nodeIt,const char*& outString,size_t& outStringLength,Memory::Arena& arena)
	{
		if(nodeIt && nodeIt->type == SExp::NodeType::String)
		{
			outString = arena.copyToArena(nodeIt->string,nodeIt->stringLength + 1);
			outStringLength = nodeIt->stringLength;
			++nodeIt;
			return true;
		}
		else { return false; }
	}
	
	// Parse a S-expression tree or attribute node. Upon success, outChildIt is set to the node's first child.
	bool parseTreelikeNode(SNodeIt nodeIt,SExp::NodeType nodeType,SNodeIt& outChildIt)
	{
		if(nodeIt && nodeIt->type == nodeType) { outChildIt = nodeIt.getChildIt(); return true; }
		else { return false; }
	}
	bool parseTreeNode(SNodeIt nodeIt,SNodeIt& outChildIt) { return parseTreelikeNode(nodeIt,SExp::NodeType::Tree,outChildIt); }
	bool parseAttributeNode(SNodeIt nodeIt,SNodeIt& outChildIt) { return parseTreelikeNode(nodeIt,SExp::NodeType::Attribute,outChildIt); }

	// Parse a S-expression symbol node. Upon success, outSymbol is set to the parsed symbol.
	bool parseSymbol(SNodeIt& nodeIt,Symbol& outSymbol)
	{
		if(nodeIt && nodeIt->type == SExp::NodeType::Symbol) { outSymbol = (Symbol)nodeIt->symbol; ++nodeIt; return true; }
		else { return false; }
	}

	// Parse a S-expression tree node whose first child is a symbol. Sets outChildren to the first child after the symbol on success.
	bool parseTaggedNode(SNodeIt nodeIt,Symbol tagSymbol,SNodeIt& outChildIt)
	{
		Symbol symbol;
		return parseTreeNode(nodeIt,outChildIt) && parseSymbol(outChildIt,symbol) && symbol == tagSymbol;
	}

	// Parse a S-expression attribute node whose first child is a symbol. Sets outValue to the attribute value.
	bool parseSymbolAttribute(SNodeIt nodeIt,Symbol keySymbol,SNodeIt& outValueIt)
	{
		Symbol symbol;
		return parseAttributeNode(nodeIt,outValueIt) && parseSymbol(outValueIt,symbol) && symbol == keySymbol;
	}

	// Tries to parse a name from a SExp node (a string symbol starting with a $).
	// On success, returns true, sets outString to the name's string, and advances node to the sibling following the name.
	bool parseName(SNodeIt& nodeIt,const char*& outString)
	{
		if(nodeIt && nodeIt->type == SExp::NodeType::UnindexedSymbol && nodeIt->string[0] == '$')
		{
			outString = nodeIt->string + 1;
			++nodeIt;
			return true;
		}
		else
		{
			return false;
		}
	}
	
	// Parse a variable from the child nodes of a local, or param node. Names are copied into the provided memory arena.
	// Format is (name type) | type+
	size_t parseVariables(SNodeIt& childNodeIt,std::vector<Variable>& outVariables,std::vector<ErrorRecord*>& outErrors,Memory::Arena& arena)
	{
		const char* name;
		if(parseName(childNodeIt,name))
		{
			TypeId type;
			if(!parseType(childNodeIt,type)) { recordError<ErrorRecord>(outErrors,childNodeIt,"expected type"); return 0; }
			auto nameCopy = arena.copyToArena(name,strlen(name)+1);
			outVariables.emplace_back(std::move(Variable({type,nameCopy})));
			return 1;
		}
		else
		{
			size_t numVariables = 0;
			while(childNodeIt)
			{
				TypeId type;
				if(!parseType(childNodeIt,type)) { recordError<ErrorRecord>(outErrors,childNodeIt,"expected type"); return numVariables; }
 				outVariables.emplace_back(std::move(Variable({type,nullptr})));
				++numVariables;
			}
			return numVariables;
		}
	}
	
	// Parse a name or an index.
	// If a name is parsed that is contained in nameToIndex, the index of the name is assigned to outIndex and true is returned.
	// If an index is parsed that is between 0 and numValidIndices, the index is assigned to outIndex and true is returned.
	bool parseNameOrIndex(SNodeIt& nodeIt,const NameToIndexMap& nameToIndex,size_t numValidIndices,uintptr& outIndex)
	{
		const char* name;
		uint64 parsedInt;
		if(parseUnsignedInt(nodeIt,parsedInt) && (uintptr)parsedInt < numValidIndices) { outIndex = (uintptr)parsedInt; return true; }
		else if(parseName(nodeIt,name))
		{
			auto it = nameToIndex.find(name);
			if(it != nameToIndex.end()) { outIndex = it->second; return true; }
			else { return false; }
		}
		else { return false; }
	}

	// Builds a map from name to index from an array of variables.
	void buildVariableNameToIndexMapMap(const std::vector<Variable>& variables,NameToIndexMap& outNameToIndexMap,std::vector<ErrorRecord*>& outErrors)
	{
		for(uintptr variableIndex = 0;variableIndex < variables.size();++variableIndex)
		{
			const auto& variable = variables[variableIndex];
			if(variable.name != nullptr)
			{
				if(outNameToIndexMap.count(variable.name)) { recordError<ErrorRecord>(outErrors,SNodeIt(nullptr),"duplicate variable name"); }
				else { outNameToIndexMap[variable.name] = variableIndex; }
			}
		}
	}
	
	// Context that is shared between functions parsing a module.
	struct ModuleContext
	{
		Module* module;
		NameToIndexMap functionNameToIndexMap;
		NameToIndexMap functionTableNameToIndexMap;
		NameToIndexMap functionImportNameToIndexMap;
		NameToIndexMap intrinsicNameToImportIndexMap;
		NameToIndexMap signatureNameToIndexMap;
		std::vector<FunctionType> signatures;
		std::vector<ErrorRecord*>& outErrors;

		ModuleContext(Module* inModule,std::vector<ErrorRecord*>& inOutErrors): module(inModule), outErrors(inOutErrors) {}

		Module* parse(SNodeIt moduleNode);
	};

	// Context that is shared between functions parsing a function.
	struct FunctionContext
	{
		FunctionContext(ModuleContext& inModuleContext,Function* inFunction)
		:	arena(inModuleContext.module->arena)
		,	outErrors(inModuleContext.outErrors)
		,	moduleContext(inModuleContext)
		,	function(inFunction)
		{
			// Build a map from local/parameter names to indices.
			buildVariableNameToIndexMapMap(function->locals,localNameToIndexMap,outErrors);
		}
		
		// Parses an expression of a specific type that's not known at compile time. Returns an UntypedExpression because the type is known to the caller.
		UntypedExpression* parseTypedExpression(TypeId type,SNodeIt& nodeIt,const char* errorContext)
		{
			switch(type)
			{
			#define AST_TYPE(typeName,className,...) case TypeId::typeName: return parseTypedExpression<className##Class>(type,nodeIt,errorContext);
			ENUM_AST_TYPES(AST_TYPE,_)
			#undef AST_TYPE
			default: throw;
			};
		}
		
		// Parses a sequence of expressions, with the final node being of a specific type.
		UntypedExpression* parseExpressionSequence(TypeId type,SNodeIt nodeIt,const char* context)
		{
			switch(type)
			{
			#define AST_TYPE(typeName,className,...) case TypeId::typeName: return parseExpressionSequence<className##Class>(type,nodeIt,context);
			ENUM_AST_TYPES(AST_TYPE,_)
			#undef AST_TYPE
			default: throw;
			}
		}

	private:
		Memory::Arena& arena;
		std::vector<ErrorRecord*>& outErrors;
		ModuleContext& moduleContext;
		Function* function;
		NameToIndexMap localNameToIndexMap;
		NameToBranchTargetMap labelToBranchTargetMap;

		std::vector<BranchTarget*> scopedBranchTargets;
	
		struct ScopedBranchTarget
		{
			ScopedBranchTarget(FunctionContext& inContext,bool inHasName,const char* inName,BranchTarget* inBranchTarget,SNodeIt nodeIt)
			: context(inContext), hasName(inHasName), name(inName), branchTarget(inBranchTarget), outerBranchTarget(nullptr)
			{
				if(hasName)
				{
					if(context.labelToBranchTargetMap.count(name))
					{
						outerBranchTarget = context.labelToBranchTargetMap[name];
					}
					context.labelToBranchTargetMap[name] = branchTarget;
				}
				context.scopedBranchTargets.push_back(branchTarget);
			}
			~ScopedBranchTarget()
			{
				if(hasName)
				{
					if(outerBranchTarget) { context.labelToBranchTargetMap[name] = outerBranchTarget; }
					else { context.labelToBranchTargetMap.erase(name); }
				}
				assert(context.scopedBranchTargets.back() == branchTarget);
				context.scopedBranchTargets.pop_back();
			}

		private:
			FunctionContext& context;
			bool hasName;
			const char* name;
			BranchTarget* branchTarget;
			BranchTarget* outerBranchTarget;
		};

		// Parses a non-parametric expression. These are expressions whose result type is defined by the opcode.
		TypedExpression parseNonParametricExpression(SNodeIt parentNodeIt)
		{
			SNodeIt nodeIt;
			Symbol tag;
			if(parseTreeNode(parentNodeIt,nodeIt) && parseSymbol(nodeIt,tag))
			{
				TypeId opType;
				switch(tag)
				{
				default: return TypedExpression();
				#define DEFINE_UNTYPED_OP(symbol) \
					throw; case Symbol::_##symbol: opType = TypeId::None;
				#define DISPATCH_TYPED_OP(opTypeName,opClassName,symbol) \
					throw; case Symbol::_##symbol##_##opTypeName: opType = TypeId::opTypeName; goto symbol##opClassName##Label;
				#define DEFINE_TYPED_OP(opClass,symbol) \
					ENUM_AST_TYPES_##opClass(DISPATCH_TYPED_OP,symbol) \
					throw; symbol##opClass##Label:
				#define DEFINE_ADHOC_TYPED_OP(opTypeName,symbol) \
					throw; case Symbol::_##symbol##_##opTypeName: opType = TypeId::opTypeName;
				#define DEFINE_BITYPED_OP(leftTypeName,rightTypeName,symbol) \
					throw; case Symbol::_##symbol##_##leftTypeName##_##rightTypeName:
					
				#define DEFINE_UNARY_OP(class,symbol,opcode) DEFINE_TYPED_OP(class,symbol) { return parseUnaryExpression<class##Class>(opType,class##Op::opcode,nodeIt); }
				#define DEFINE_BINARY_OP(class,symbol,opcode) DEFINE_TYPED_OP(class,symbol) { return parseBinaryExpression<class##Class>(opType,class##Op::opcode,nodeIt); }
				#define DEFINE_COMPARE_OP(class,symbol,opcode) DEFINE_TYPED_OP(class,symbol) { return parseComparisonExpression(opType,BoolOp::opcode,nodeIt); }
				#define DEFINE_CAST_OP(destType,sourceType,symbol,opcode) DEFINE_BITYPED_OP(destType,sourceType,symbol) \
					{ return parseCastExpression<destType##Type::Class>(destType##Type::Op::opcode,TypeId::sourceType,TypeId::destType,nodeIt); }

				DEFINE_UNTYPED_OP(nop)			{ return TypedExpression(requireFullMatch(nodeIt,"nop",Nop::get()),TypeId::Void); }

				DEFINE_UNTYPED_OP(br_if)
				{
					// Parse the name or index of the target label.
					const char* name;
					uint64 parsedInt;
					BranchTarget* branchTarget = nullptr;
					if(parseUnsignedInt(nodeIt,parsedInt) && (uintptr)parsedInt < scopedBranchTargets.size())
					{
						branchTarget = scopedBranchTargets[scopedBranchTargets.size() - 1 - (uintptr)parsedInt];
					}
					else if(parseName(nodeIt,name))
					{
						auto it = labelToBranchTargetMap.find(name);
						if(it != labelToBranchTargetMap.end()) { branchTarget = it->second; }
					}
					else
					{
						// If no name or index, use the top of the target stack.
						branchTarget = scopedBranchTargets.back();
					}
					if(!branchTarget)
					{
						return TypedExpression(recordError<Error>(outErrors,nodeIt,"br_if: expected label name or index"),TypeId::Void);
					}
					
					// If the branch target's type isn't void, parse an expression for the branch's value.
					auto value = branchTarget->type == TypeId::Void ? nullptr
						: parseTypedExpression(branchTarget->type,nodeIt,"break value");

					// Parse the condition.
					auto condition = parseTypedExpression<BoolClass>(TypeId::Bool,nodeIt,"if condition");

					// Create BranchIf node.
					auto result = new(arena) BranchIf(branchTarget,condition,value);
					return TypedExpression(requireFullMatch(nodeIt,"br_if",result),TypeId::Void);
				}

				DEFINE_UNTYPED_OP(memory_size)		{ return parseIntrinsic<IntClass>("memory_size",FunctionType(TypeId::I32,{}),nodeIt); }
				DEFINE_UNTYPED_OP(page_size)		{ return parseIntrinsic<IntClass>("page_size",FunctionType(TypeId::I32,{}),nodeIt); }
				DEFINE_UNTYPED_OP(grow_memory)		{ return parseIntrinsic<IntClass>("grow_memory",FunctionType(TypeId::I32,{TypeId::I32}),nodeIt); }
				
				DEFINE_TYPED_OP(Int,eqz)
				{
					UntypedExpression* zero = nullptr;
					switch(opType)
					{
					case TypeId::I8: zero = new(arena) Literal<I8Type>(0); break;
					case TypeId::I16: zero = new(arena) Literal<I16Type>(0); break;
					case TypeId::I32: zero = new(arena) Literal<I32Type>(0); break;
					case TypeId::I64: zero = new(arena) Literal<I64Type>(0); break;
					default: throw;
					}
					auto operand = parseTypedExpression(opType,nodeIt,"eqz operand");
					auto result = new(arena) Comparison(BoolOp::eq,opType,operand,zero);
					return TypedExpression(requireFullMatch(nodeIt,getOpName(BoolOp::eq),result),TypeId::Bool);
				}

				DEFINE_TYPED_OP(Int,const)
				{
					int64 integer;
					if(!parseInt(nodeIt,integer)) { return TypedExpression(recordError<Error>(outErrors,nodeIt,"const: expected integer"),opType); }
					switch(opType)
					{
					case TypeId::I8: return TypedExpression(requireFullMatch(nodeIt,"const.i8",new(arena)Literal<I8Type>((uint8)integer)),TypeId::I8);
					case TypeId::I16: return TypedExpression(requireFullMatch(nodeIt,"const.i16",new(arena)Literal<I16Type>((uint16)integer)),TypeId::I16);
					case TypeId::I32: return TypedExpression(requireFullMatch(nodeIt,"const.i32",new(arena)Literal<I32Type>((uint32)integer)),TypeId::I32);
					case TypeId::I64: return TypedExpression(requireFullMatch(nodeIt,"const.i64",new(arena)Literal<I64Type>((uint64)integer)),TypeId::I64);
					default: throw;
					}
				}
				DEFINE_TYPED_OP(Float,const)
				{
					float64 f64;
					float32 f32;
					if(!parseFloat(nodeIt,f64,f32)) { return TypedExpression(recordError<Error>(outErrors,nodeIt,"const: expected floating point number"),opType); }
					switch(opType)
					{
					case TypeId::F32: return TypedExpression(requireFullMatch(nodeIt,"const.f32",new(arena)Literal<F32Type>(f32)),TypeId::F32);
					case TypeId::F64: return TypedExpression(requireFullMatch(nodeIt,"const.f64",new(arena)Literal<F64Type>(f64)),TypeId::F64);
					default: throw;
					}
				}
				
				#define DEFINE_LOAD_OP(class,valueType,memoryType,loadSymbol,loadOp) DEFINE_ADHOC_TYPED_OP(valueType,loadSymbol)	\
					{ return parseLoadExpression<class##Class>(TypeId::valueType,TypeId::memoryType,class##Op::loadOp,false,nodeIt); }
				#define DEFINE_STORE_OP(class,valueType,memoryType,storeSymbol) DEFINE_ADHOC_TYPED_OP(valueType,storeSymbol) \
					{ return parseStoreExpression<class##Class>(TypeId::valueType,TypeId::memoryType,false,nodeIt); }
				#define DEFINE_MEMORY_OP(class,valueType,memoryType,loadSymbol,storeSymbol,loadOp) \
					DEFINE_LOAD_OP(class,valueType,memoryType,loadSymbol,loadOp) \
					DEFINE_STORE_OP(class,valueType,memoryType,storeSymbol)

				DEFINE_LOAD_OP(Int,I32,I8,load8_s,loadSExt)
				DEFINE_LOAD_OP(Int,I32,I8,load8_u,loadZExt)
				DEFINE_LOAD_OP(Int,I32,I16,load16_s,loadSExt)
				DEFINE_LOAD_OP(Int,I32,I16,load16_u,loadZExt)
				DEFINE_LOAD_OP(Int,I64,I8,load8_s,loadSExt)
				DEFINE_LOAD_OP(Int,I64,I8,load8_u,loadZExt)
				DEFINE_LOAD_OP(Int,I64,I16,load16_s,loadSExt)
				DEFINE_LOAD_OP(Int,I64,I16,load16_u,loadZExt)
				DEFINE_LOAD_OP(Int,I64,I32,load32_s,loadSExt)
				DEFINE_LOAD_OP(Int,I64,I32,load32_u,loadZExt)
				DEFINE_STORE_OP(Int,I32,I8,store8)
				DEFINE_STORE_OP(Int,I32,I16,store16)
				DEFINE_STORE_OP(Int,I64,I8,store8)
				DEFINE_STORE_OP(Int,I64,I16,store16)
				DEFINE_STORE_OP(Int,I64,I32,store32)
				DEFINE_MEMORY_OP(Int,I32,I32,load,store,load)
				DEFINE_MEMORY_OP(Int,I64,I64,load,store,load)
				DEFINE_MEMORY_OP(Float,F32,F32,load,store,load)
				DEFINE_MEMORY_OP(Float,F64,F64,load,store,load)

				DEFINE_UNARY_OP(Int,neg,neg)
				DEFINE_UNARY_OP(Int,abs,abs)
				DEFINE_UNARY_OP(Int,not,bitwiseNot)
				DEFINE_UNARY_OP(Int,clz,clz)
				DEFINE_UNARY_OP(Int,ctz,ctz)
				DEFINE_UNARY_OP(Int,popcnt,popcnt)
				DEFINE_BINARY_OP(Int,add,add)
				DEFINE_BINARY_OP(Int,sub,sub)
				DEFINE_BINARY_OP(Int,mul,mul)
				DEFINE_BINARY_OP(Int,div_s,divs)
				DEFINE_BINARY_OP(Int,div_u,divu)
				DEFINE_BINARY_OP(Int,rem_s,rems)
				DEFINE_BINARY_OP(Int,rem_u,remu)
				DEFINE_BINARY_OP(Int,and,bitwiseAnd)
				DEFINE_BINARY_OP(Int,or,bitwiseOr)
				DEFINE_BINARY_OP(Int,xor,bitwiseXor)
				DEFINE_BINARY_OP(Int,shl,shl)
				DEFINE_BINARY_OP(Int,shr_s,shrSExt)
				DEFINE_BINARY_OP(Int,shr_u,shrZExt)
				DEFINE_BINARY_OP(Int,rotl,rotl)
				DEFINE_BINARY_OP(Int,rotr,rotr)

				DEFINE_UNARY_OP(Float,neg,neg)
				DEFINE_UNARY_OP(Float,abs,abs)
				DEFINE_UNARY_OP(Float,ceil,ceil)
				DEFINE_UNARY_OP(Float,floor,floor)
				DEFINE_UNARY_OP(Float,trunc,trunc)
				DEFINE_UNARY_OP(Float,nearest,nearestInt)
				DEFINE_BINARY_OP(Float,add,add)
				DEFINE_BINARY_OP(Float,sub,sub)
				DEFINE_BINARY_OP(Float,mul,mul)
				DEFINE_BINARY_OP(Float,div,div)
				DEFINE_BINARY_OP(Float,rem,rem)
				DEFINE_BINARY_OP(Float,copysign,copySign)
				DEFINE_BINARY_OP(Float,min,min)
				DEFINE_BINARY_OP(Float,max,max)
				DEFINE_UNARY_OP(Float,sqrt,sqrt)

				DEFINE_UNARY_OP(Bool,not,bitwiseNot)
				DEFINE_BINARY_OP(Bool,and,bitwiseAnd)
				DEFINE_BINARY_OP(Bool,or,bitwiseOr)

				DEFINE_COMPARE_OP(Int,eq,eq) DEFINE_COMPARE_OP(Int,ne,ne)
				DEFINE_COMPARE_OP(Float,eq,eq) DEFINE_COMPARE_OP(Float,ne,ne)
				DEFINE_COMPARE_OP(Bool,eq,eq) DEFINE_COMPARE_OP(Bool,ne,ne)
				DEFINE_COMPARE_OP(Int,lt_s,lts)
				DEFINE_COMPARE_OP(Int,lt_u,ltu)
				DEFINE_COMPARE_OP(Int,le_s,les)
				DEFINE_COMPARE_OP(Int,le_u,leu)
				DEFINE_COMPARE_OP(Int,gt_s,gts)
				DEFINE_COMPARE_OP(Int,gt_u,gtu)
				DEFINE_COMPARE_OP(Int,ge_s,ges)
				DEFINE_COMPARE_OP(Int,ge_u,geu)
				DEFINE_COMPARE_OP(Float,lt,lt)
				DEFINE_COMPARE_OP(Float,le,le)
				DEFINE_COMPARE_OP(Float,gt,gt)
				DEFINE_COMPARE_OP(Float,ge,ge)
				
				DEFINE_CAST_OP(I8,I16,wrap,wrap)
				DEFINE_CAST_OP(I8,I32,wrap,wrap)
				DEFINE_CAST_OP(I8,I64,wrap,wrap)
				DEFINE_CAST_OP(I16,I32,wrap,wrap)
				DEFINE_CAST_OP(I16,I64,wrap,wrap)
				DEFINE_CAST_OP(I32,I64,wrap,wrap)

				DEFINE_CAST_OP(I64,I8,extend_s,sext)
				DEFINE_CAST_OP(I64,I16,extend_s,sext)
				DEFINE_CAST_OP(I64,I32,extend_s,sext)
				DEFINE_CAST_OP(I32,I8,extend_s,sext)
				DEFINE_CAST_OP(I32,I16,extend_s,sext)
				DEFINE_CAST_OP(I16,I8,extend_s,sext)

				DEFINE_CAST_OP(I64,I8,extend_u,zext)
				DEFINE_CAST_OP(I64,I16,extend_u,zext)
				DEFINE_CAST_OP(I64,I32,extend_u,zext)
				DEFINE_CAST_OP(I32,I8,extend_u,zext)
				DEFINE_CAST_OP(I32,I16,extend_u,zext)
				DEFINE_CAST_OP(I16,I8,extend_u,zext)

				DEFINE_CAST_OP(I32,F64,trunc_s,truncSignedFloat)
				DEFINE_CAST_OP(I32,F64,trunc_u,truncUnsignedFloat)
				DEFINE_CAST_OP(I32,F32,trunc_s,truncSignedFloat)
				DEFINE_CAST_OP(I32,F32,trunc_u,truncUnsignedFloat)
				DEFINE_CAST_OP(I64,F64,trunc_s,truncSignedFloat)
				DEFINE_CAST_OP(I64,F64,trunc_u,truncUnsignedFloat)
				DEFINE_CAST_OP(I64,F32,trunc_s,truncSignedFloat)
				DEFINE_CAST_OP(I64,F32,trunc_u,truncUnsignedFloat)

				DEFINE_CAST_OP(F64,I8,convert_s,convertSignedInt)
				DEFINE_CAST_OP(F64,I16,convert_s,convertSignedInt)
				DEFINE_CAST_OP(F64,I32,convert_s,convertSignedInt)
				DEFINE_CAST_OP(F64,I64,convert_s,convertSignedInt)

				DEFINE_CAST_OP(F32,I8,convert_s,convertSignedInt)
				DEFINE_CAST_OP(F32,I16,convert_s,convertSignedInt)
				DEFINE_CAST_OP(F32,I32,convert_s,convertSignedInt)
				DEFINE_CAST_OP(F32,I64,convert_s,convertSignedInt)

				DEFINE_CAST_OP(F64,I8,convert_u,convertUnsignedInt)
				DEFINE_CAST_OP(F64,I16,convert_u,convertUnsignedInt)
				DEFINE_CAST_OP(F64,I32,convert_u,convertUnsignedInt)
				DEFINE_CAST_OP(F64,I64,convert_u,convertUnsignedInt)

				DEFINE_CAST_OP(F32,I8,convert_u,convertUnsignedInt)
				DEFINE_CAST_OP(F32,I16,convert_u,convertUnsignedInt)
				DEFINE_CAST_OP(F32,I32,convert_u,convertUnsignedInt)
				DEFINE_CAST_OP(F32,I64,convert_u,convertUnsignedInt)

				DEFINE_CAST_OP(F32,F64,demote,demote)
				DEFINE_CAST_OP(F64,F32,promote,promote)

				DEFINE_CAST_OP(F64,I64,reinterpret,reinterpretInt)
				DEFINE_CAST_OP(F32,I32,reinterpret,reinterpretInt)
				DEFINE_CAST_OP(I64,F64,reinterpret,reinterpretFloat)
				DEFINE_CAST_OP(I32,F32,reinterpret,reinterpretFloat)

				DEFINE_CAST_OP(I8,Bool,reinterpret,reinterpretBool)
				DEFINE_CAST_OP(I16,Bool,reinterpret,reinterpretBool)
				DEFINE_CAST_OP(I32,Bool,reinterpret,reinterpretBool)
				DEFINE_CAST_OP(I64,Bool,reinterpret,reinterpretBool)

				#undef DEFINE_UNTYPED_OP
				#undef DISPATCH_TYPED_OP
				#undef DEFINE_TYPED_OP
				#undef DISPATCH_BITYPED_OP
				#undef DEFINE_BITYPED_OP
				}
			}
			
			return TypedExpression();
		}

		// Parse a parametric expression of a specific type.
		// These are different from the non-parametric expressions because the opcodes are valid in any type context,
		// so it needs to know the context to construct the correct class of expression.
		template<typename Class> typename Class::ClassExpression* parseParametricExpression(TypeId resultType,SNodeIt parentNodeIt)
		{
			SNodeIt nodeIt;
			Symbol tag;
			if(parseTreeNode(parentNodeIt,nodeIt) && parseSymbol(nodeIt,tag))
			{
				switch(tag)
				{
				default: return nullptr;
				#define DEFINE_PARAMETRIC_UNTYPED_OP(symbol) \
					throw; case Symbol::_##symbol:

				DEFINE_PARAMETRIC_UNTYPED_OP(if)
				{
					// Parse the if condition, then-expression, and else-expression.
					auto condition = parseTypedExpression<BoolClass>(TypeId::Bool,nodeIt,"if condition");

					SNodeIt thenNodeIt;
					typename Class::ClassExpression* trueValue = nullptr;
					if(parseTaggedNode(nodeIt,Symbol::_then,thenNodeIt)) { ++nodeIt; trueValue = parseLabeledBlock<Class>(resultType,thenNodeIt,"if then"); }
					else { trueValue = parseAnonLabeledExpression<Class>(resultType,nodeIt,"if then"); }

					typename Class::ClassExpression* falseValue = nullptr;
					SNodeIt elseNodeIt;
					if(parseTaggedNode(nodeIt,Symbol::_else,elseNodeIt)) { ++nodeIt; falseValue = parseLabeledBlock<Class>(resultType,elseNodeIt,"if else"); }
					else if(nodeIt) { falseValue = parseAnonLabeledExpression<Class>(resultType,nodeIt,"if else"); }
					else if(resultType != TypeId::Void) { return as<Class>(recordError<Error>(outErrors,nodeIt,"if: expected else expression for if with non-void result")); }
					else { falseValue = as<Class>(Nop::get()); }

					// Construct the Conditional node.
					return requireFullMatch(nodeIt,"if",new(arena)Conditional<Class>(Class::Op::ifElse,condition,trueValue,falseValue));
				}
				DEFINE_PARAMETRIC_UNTYPED_OP(select)
				{
					// Parse the then-expression, and else-expression, and select condition.
					auto trueValue = parseTypedExpression<Class>(resultType,nodeIt,"select true value");
					auto falseValue = parseTypedExpression<Class>(resultType,nodeIt,"select false value");
					auto condition = parseTypedExpression<BoolClass>(TypeId::Bool,nodeIt,"select condition");

					// Construct the Conditional node.
					auto select = new(arena)Conditional<Class>(Class::Op::select,condition,trueValue,falseValue);
					return requireFullMatch(nodeIt,"select",select);
				}
				DEFINE_PARAMETRIC_UNTYPED_OP(loop)
				{
					auto continueTarget = new(arena) BranchTarget(TypeId::Void);
					auto breakTarget = new(arena) BranchTarget(resultType);

					// Parse a label names for the continue and break branch targets.
					const char* firstLabelName = nullptr;
					const char* secondLabelName = nullptr;
					bool hasFirstLabel = parseName(nodeIt,firstLabelName);
					bool hasSecondLabel = hasFirstLabel && parseName(nodeIt,secondLabelName);
					
					const char* breakLabelName = hasSecondLabel ? firstLabelName : nullptr;
					const char* continueLabelName = hasSecondLabel ? secondLabelName : (hasFirstLabel ? firstLabelName : nullptr);

					ScopedBranchTarget scopedBreakTarget(*this,breakLabelName != nullptr,breakLabelName,breakTarget,nodeIt);
					ScopedBranchTarget scopedContinueTarget(*this,continueLabelName != nullptr,continueLabelName,continueTarget,nodeIt);

					// Parse the loop body.
					auto expression = parseExpressionSequence<Class>(resultType,nodeIt,"loop body");

					// Create the Loop node.
					return new(arena)Loop<Class>(expression,breakTarget,continueTarget);
				}
				DEFINE_PARAMETRIC_UNTYPED_OP(br)
				{
					// Parse the branch target.
					BranchTarget* branchTarget = parseBranchTargetRef(nodeIt);
					if(!branchTarget) { return as<Class>(recordError<Error>(outErrors,nodeIt,"br: expected label name or index")); }

					// If the branch target's type isn't void, parse an expression for the branch's value.
					auto value = branchTarget->type == TypeId::Void ? nullptr
						: parseTypedExpression(branchTarget->type,nodeIt,"br value");

					// Create the Branch node.
					return requireFullMatch(nodeIt,"br",as<Class>(new(arena)Branch(branchTarget,value)));
				}
				DEFINE_PARAMETRIC_UNTYPED_OP(br_table)
				{
					// Count the number of branch targets provided.
					size_t numTableTargets = 0;
					for(SNodeIt countIt = nodeIt;parseBranchTargetRef(countIt);) { ++numTableTargets; }
					if(numTableTargets == 0)
					{ return as<Class>(recordError<Error>(outErrors,nodeIt,"br_table: must have at least a default branch target")); }

					// Parse the table targets.
					auto tableTargets = new(arena) BranchTarget*[numTableTargets-1];
					for(uintptr tableIndex = 0;tableIndex < numTableTargets - 1;++tableIndex)
					{
						SNodeIt errorNodeIt = nodeIt;
						tableTargets[tableIndex] = parseBranchTargetRef(nodeIt);
						if(!tableTargets[tableIndex])
						{ return as<Class>(recordError<Error>(outErrors,errorNodeIt,"br_table: expected case or branch target reference")); }
					}

					// Parse the default branch target.
					BranchTarget* defaultBranchTarget;
					{
						SNodeIt errorNodeIt = nodeIt;
						defaultBranchTarget = parseBranchTargetRef(nodeIt);
						if(!defaultBranchTarget)
						{ return as<Class>(recordError<Error>(outErrors,errorNodeIt,"br_table: expected case or branch target reference")); }
					}

					// Verify that all the branch targets have the same type (assume the type of the default target is what's expected).
					for(uintptr tableIndex = 0;tableIndex < numTableTargets - 1;++tableIndex)
					{
						if(tableTargets[tableIndex]->type != defaultBranchTarget->type)
						{ return as<Class>(recordError<Error>(outErrors,parentNodeIt,std::string("br_table: target ") + std::to_string(tableIndex) + " doesn't take an argument of the same type as the default target")); }
					}

					// If the branch target's type isn't void, parse an expression for the branch's value.
					auto value = defaultBranchTarget->type == TypeId::Void ? nullptr
						: parseTypedExpression(defaultBranchTarget->type,nodeIt,"br_table value");

					// Parse the table index.
					auto index = parseTypedExpression<IntClass>(TypeId::I32,nodeIt,"br_table index");

					// Create a BranchTable operation that branches to the appropriate target for the case.
					auto branchTable = new(arena) BranchTable(
						value,
						TypedExpression(index,TypeId::I32),
						defaultBranchTarget,
						tableTargets,
						numTableTargets-1
						);

					return requireFullMatch(nodeIt,"br_table",as<Class>(branchTable));
				}
				DEFINE_PARAMETRIC_UNTYPED_OP(unreachable)
				{
					return requireFullMatch(nodeIt,"unreachable",as<Class>(Unreachable::get()));
				}
				DEFINE_PARAMETRIC_UNTYPED_OP(return)
				{
					// If the function's return type isn't void, parse an expression for the return value.
					auto returnType = function->type.returnType;
					auto valueExpression = returnType == TypeId::Void ? nullptr
						: parseTypedExpression(returnType,nodeIt,"return value");
					
					// Create the Return node.
					return requireFullMatch(nodeIt,"return",as<Class>(new(arena)Return(valueExpression)));
				}
				DEFINE_PARAMETRIC_UNTYPED_OP(call)
				{
					// Parse the function name or index to call.
					uintptr functionIndex;
					if(!parseNameOrIndex(nodeIt,moduleContext.functionNameToIndexMap,moduleContext.module->functions.size(),functionIndex))
					{
						return as<Class>(recordError<Error>(outErrors,nodeIt,"call: expected function name or index"));
					}

					// Parse the call's parameters.
					auto callFunction = moduleContext.module->functions[functionIndex];
					auto parameters = parseParameters(callFunction->type.parameters,nodeIt,"call parameter");

					// Create the Call node.
					auto call = new(arena)Call(AnyOp::callDirect,getPrimaryTypeClass(callFunction->type.returnType),functionIndex,parameters);

					// Validate the function return type against the result type of this call.
					auto result = coerceExpression(Class(),resultType,TypedExpression(call,callFunction->type.returnType),parentNodeIt,"call return value");
					return requireFullMatch(nodeIt,"call",result);
				}
				DEFINE_PARAMETRIC_UNTYPED_OP(call_import)
				{
					// Parse the import name or index to call.
					uintptr importIndex;
					if(!parseNameOrIndex(nodeIt,moduleContext.functionImportNameToIndexMap,moduleContext.module->functionImports.size(),importIndex))
					{
						return as<Class>(recordError<Error>(outErrors,nodeIt,"call_import: expected function import name or index"));
					}

					// Parse the call's parameters.
					auto functionImport = moduleContext.module->functionImports[importIndex];
					auto parameters = parseParameters(functionImport.type.parameters,nodeIt,"call_import parameter");

					// Create the Call node.
					auto call = new(arena)Call(AnyOp::callImport,getPrimaryTypeClass(functionImport.type.returnType),importIndex,parameters);
					
					// Validate the function return type against the result type of this call.
					auto result = coerceExpression(Class(),resultType,TypedExpression(call,functionImport.type.returnType),parentNodeIt,"call_import return value");
					return requireFullMatch(nodeIt,"call",result);
				}
				DEFINE_PARAMETRIC_UNTYPED_OP(call_indirect)
				{
					// Parse the function type.
					uintptr signatureIndex;
					if(!parseNameOrIndex(nodeIt,moduleContext.signatureNameToIndexMap,moduleContext.signatures.size(),signatureIndex))
					{
						return as<Class>(recordError<Error>(outErrors,nodeIt,"call_indirect: expected type index"));
					}
					const auto& functionType = moduleContext.signatures[signatureIndex];

					// Parse the function index.
					auto functionIndex = parseTypedExpression<IntClass>(TypeId::I32,nodeIt,"call_indirect function");

					// Parse the call's parameters.
					auto parameters = parseParameters(functionType.parameters,nodeIt,"call_indirect parameter");

					// Create the CallIndirect node.
					auto call = new(arena)CallIndirect(getPrimaryTypeClass(functionType.returnType),functionType,functionIndex,parameters);
					
					// Validate the function return type against the result type of this call.
					auto result = coerceExpression(Class(),resultType,TypedExpression(call,functionType.returnType),parentNodeIt,"call_indirect return value");
					return requireFullMatch(nodeIt,"call_indirect",result);
				}

				DEFINE_PARAMETRIC_UNTYPED_OP(block) { return parseLabeledBlock<Class>(resultType,nodeIt,"block"); }

				DEFINE_PARAMETRIC_UNTYPED_OP(get_local)
				{ return parseGetLocal<Class>(resultType,localNameToIndexMap,function->locals,nodeIt); }
				DEFINE_PARAMETRIC_UNTYPED_OP(set_local)
				{ return parseSetLocal<Class>(resultType,localNameToIndexMap,function->locals,nodeIt); }

				#undef DEFINE_PARAMETRIC_UNTYPED_OP
				}
			}

			return nullptr;
		}
		
		// Record a type error.
		Error* typeError(TypeId type,TypedExpression typedExpression,SNodeIt nodeIt,const char* errorContext)
		{
			auto message =
				std::string("type error: expecting a ") + getTypeName(type)
				+ " " + errorContext
				+ " but found " + getTypeName(typedExpression.type);
			return recordError<Error>(outErrors,nodeIt,std::move(message));
		}

		// By default coerceExpression results in a type error, but is overloaded below for specific classes.
		template<typename Class>
		typename Class::ClassExpression* coerceExpression(Class,TypeId resultType,TypedExpression typedExpression,SNodeIt nodeIt,const char* errorContext)
		{
			if(resultType == typedExpression.type) { return as<Class>(typedExpression.expression); }
			else { return as<Class>(typeError(resultType,typedExpression,nodeIt,errorContext)); }
		}

		// coerceExpression for BoolClass will try to coerce integers.
		BoolClass::ClassExpression* coerceExpression(BoolClass,TypeId resultType,TypedExpression typedExpression,SNodeIt nodeIt,const char* errorContext)
		{
			assert(resultType == TypeId::Bool);
			if(resultType == typedExpression.type) { return as<BoolClass>(typedExpression); }
			else if(isTypeClass(typedExpression.type,TypeClassId::Int))
			{
				// Create a literal zero of the appropriate type.
				IntExpression* zero;
				switch(typedExpression.type)
				{
				case TypeId::I8: zero = new(arena) Literal<I8Type>(0); break;
				case TypeId::I16: zero = new(arena) Literal<I16Type>(0); break;
				case TypeId::I32: zero = new(arena) Literal<I32Type>(0); break;
				case TypeId::I64: zero = new(arena) Literal<I64Type>(0); break;
				default: throw;
				}
				// Coerce the integer to a boolean by testing if the integer != 0.
				return new(arena) Comparison(BoolOp::ne,typedExpression.type,typedExpression.expression,zero);
			}
			else { return as<BoolClass>(typeError(resultType,typedExpression,nodeIt,errorContext)); }
		}

		// coerceExpression for VoidClass will wrap any other typed expression in a DiscardResult node.
		VoidClass::ClassExpression* coerceExpression(VoidClass,TypeId resultType,TypedExpression typedExpression,SNodeIt nodeIt,const char* errorContext)
		{
			if(resultType == typedExpression.type) { return as<VoidClass>(typedExpression); }
			else
			{
				assert(typedExpression.type != TypeId::Void);
				return new(arena) DiscardResult(typedExpression);
			}
		}
		
		// coerceExpression for IntClass will try to coerce bools.
		IntClass::ClassExpression* coerceExpression(IntClass,TypeId resultType,TypedExpression typedExpression,SNodeIt nodeIt,const char* errorContext)
		{
			assert(isTypeClass(resultType,TypeClassId::Int));
			if(resultType == typedExpression.type) { return as<IntClass>(typedExpression); }
			else if(isTypeClass(typedExpression.type,TypeClassId::Bool))
			{
				// Reinterpret the bool as an integer.
				return new(arena) Cast<IntClass>(IntOp::reinterpretBool,typedExpression);
			}
			else { return as<IntClass>(typeError(resultType,typedExpression,nodeIt,errorContext)); }
		}

		// Parses expressions of a specific type.
		template<typename Class>
		typename Class::ClassExpression* parseTypedExpression(TypeId type,SNodeIt& nodeIt,const char* errorContext)
		{
			// If the node is a S-expression error node, translate it to an AST error node.
			if(nodeIt && nodeIt->type == SExp::NodeType::Error)
			{
				auto messageLength = strlen(nodeIt->error) + 1;
				auto messageCopy = new char[messageLength];
				memcpy(messageCopy,nodeIt->error,messageLength + 1);
				return as<Class>(recordError<Error>(outErrors,nodeIt,messageCopy));
			}
			else
			{
				// Try to parse a non-parametric expression.
				auto nonParametricExpression = parseNonParametricExpression(nodeIt);
				if(nonParametricExpression)
				{
					// If successful, then advance to the next node, and coerce the expression to the expected type.
					auto result = coerceExpression(Class(),type,nonParametricExpression,nodeIt,errorContext);
					++nodeIt;
					return result;
				}
				else 
				{
					// Try to parse a parametric expression.
					auto parametricExpression = parseParametricExpression<Class>(type,nodeIt);
					
					if(parametricExpression) { ++nodeIt; return parametricExpression; }
					else
					{
						// Failed to parse an expression.
						auto error = as<Class>(recordError<Error>(outErrors,nodeIt,std::move(std::string("expected ")
							+ getTypeName(type)
							+ " expression for "
							+ errorContext
							)));
						++nodeIt;
						return error;
					}
				}
			}
		}

		// Used to verify that after a SExpr was parsed into an AST node, that all children of the SExpr were consumed.
		// If node is null, then returns result. Otherwise produces an error that there was unexpected input.
		template<typename Class>
		Expression<Class>* requireFullMatch(SNodeIt nodeIt,const char* errorContext,Expression<Class>* result)
		{
			if(!nodeIt) { return result; }
			else { return as<Class>(recordExcessInputError<Error>(outErrors,nodeIt,errorContext)); }
		}
		
		// Parse an expression from a sequence of S-expression children. A specified number of siblings following nodeIt are used.
		// Multiple children are turned into a block node, with all but the last expression yielding a void type.
		template<typename Class>
		typename Class::ClassExpression* parseExpressionSequence(TypeId type,SNodeIt nodeIt,const char* errorContext,size_t numOps)
		{
			if(numOps == 0) { return as<Class>(Nop::get()); }
			else if(numOps == 1) { return parseTypedExpression<Class>(type,nodeIt,errorContext); }
			else
			{
				// Parse the void expressions.
				VoidExpression* result = nullptr;
				for(uintptr expressionIndex = 0;expressionIndex < numOps - 1;++expressionIndex)
				{
					auto expression = parseTypedExpression<VoidClass>(TypeId::Void,nodeIt,errorContext);
					if(result) { result = new(arena) Sequence<VoidClass>(result,expression); }
					else { result = expression; }
				}
				// Parse the result expression.
				return new(arena) Sequence<Class>(
					result,
					parseTypedExpression<Class>(type,nodeIt,errorContext)
					);
			}
		}

		// Parse an expression from a sequence of S-expression children. All siblings following nodeIt are used.
		// Multiple children are turned into a block node, with all but the last expression yielding a void type.
		template<typename Class>
		typename Class::ClassExpression* parseExpressionSequence(TypeId type,SNodeIt nodeIt,const char* context)
		{
			size_t numOps = 0;
			for(auto countNodeIt = nodeIt;countNodeIt;++countNodeIt) {++numOps;}
			if(!numOps && Class::id != TypeClassId::Void)
			{
				return as<Class>(recordError<Error>(outErrors,nodeIt,"missing expression"));
			}
			return parseExpressionSequence<Class>(type,nodeIt,context,numOps);
		}

		// Parses the parameters of a call.
		UntypedExpression** parseParameters(const std::vector<TypeId>& parameterTypes,SNodeIt& nodeIt,const char* context)
		{
			auto parameters = new(arena)UntypedExpression*[parameterTypes.size()];
			for(uintptr parameterIndex = 0;parameterIndex < parameterTypes.size();++parameterIndex)
			{
				auto parameterType = parameterTypes[parameterIndex];
				auto parameterValue = parseTypedExpression(parameterType,nodeIt,context);
				parameters[parameterIndex] = parameterValue;
			}
			return parameters;
		}

		// Parse an operation that is turned into an intrinsic call.
		template<typename Class>
		TypedExpression parseIntrinsic(const char* intrinsicName,const FunctionType& functionType,SNodeIt nodeIt)
		{
			// Create a unique import index for the intrinsic.
			auto intrinsicImportIt = moduleContext.intrinsicNameToImportIndexMap.find(intrinsicName);
			uintptr importIndex;
			if(intrinsicImportIt != moduleContext.intrinsicNameToImportIndexMap.end()) { importIndex = intrinsicImportIt->second; }
			else
			{
				importIndex = moduleContext.module->functionImports.size();
				moduleContext.module->functionImports.push_back({functionType,"wasm_intrinsics",intrinsicName});
				moduleContext.intrinsicNameToImportIndexMap[intrinsicName] = importIndex;
			}

			// Parse the intrinsic call's parameters.
			auto parameters = parseParameters(functionType.parameters,nodeIt,"intrinsic parameter");

			// Create the Call node.
			auto call = as<Class>(new(arena)Call(AnyOp::callImport,Class::id,importIndex,parameters));
			return TypedExpression(requireFullMatch(nodeIt,intrinsicName,call),functionType.returnType);
		}

		// Parse a comparison operation.
		TypedExpression parseComparisonExpression(TypeId opType,BoolOp op,SNodeIt childNodeIt)
		{
			auto leftOperand = parseTypedExpression(opType,childNodeIt,"comparison left operand");
			auto rightOperand = parseTypedExpression(opType,childNodeIt,"comparison right operand");
			auto result = new(arena) Comparison(op,opType,leftOperand,rightOperand);
			return TypedExpression(requireFullMatch(childNodeIt,getOpName(op),result),TypeId::Bool);
		}

		// Parse a binary operation.
		template<typename Class>
		TypedExpression parseBinaryExpression(TypeId opType,typename Class::Op op,SNodeIt operandNodeIt)
		{
			auto left = parseTypedExpression<Class>(opType,operandNodeIt,"binary left operand");
			auto right = parseTypedExpression<Class>(opType,operandNodeIt,"binary right operand");
			auto result = new(arena) Binary<Class>(op,left,right);
			return TypedExpression(requireFullMatch(operandNodeIt,getOpName(op),result),opType);
		}
		
		// Parse an unary operation.
		template<typename Class>
		TypedExpression parseUnaryExpression(TypeId opType,typename Class::Op op,SNodeIt operandNodeIt)
		{
			auto operand = parseTypedExpression<Class>(opType,operandNodeIt,"unary operand");
			auto result = new(arena) Unary<Class>(op,operand);
			return TypedExpression(requireFullMatch(operandNodeIt,getOpName(op),result),opType);
		}

		// Parses an offset attribute.
		uint64 parseOffsetAttribute(SNodeIt& nodeIt)
		{
			SNodeIt valueIt;
			uint64 offset = 0;
			if(parseSymbolAttribute(nodeIt,Symbol::_offset,valueIt))
			{
				++nodeIt;
				if(!parseUnsignedInt(valueIt,offset))
				{
					recordError<ErrorRecord>(outErrors,nodeIt,"offset: expected unsigned integer");
				}
			}
			return offset;
		}

		// Parses an alignment attribute.
		uint8 parseAlignmentAttribute(SNodeIt& nodeIt)
		{
			SNodeIt valueIt;
			uint64 alignment = 1;
			if(parseSymbolAttribute(nodeIt,Symbol::_align,valueIt))
			{
				++nodeIt;
				SNodeIt mutableValueIt = valueIt;
				if(!parseUnsignedInt(mutableValueIt,alignment) || !alignment || (alignment & (alignment-1)))
				{
					recordError<ErrorRecord>(outErrors,valueIt,"align: expected positive non-zero power-of-two integer");
					alignment = 1;
				}
			}
			return (uint8)Platform::floorLogTwo(alignment);
		}

		// Parse a memory load operation.
		template<typename Class>
		TypedExpression parseLoadExpression(TypeId resultType,TypeId memoryType,typename Class::Op loadOp,bool isFarAddress,SNodeIt nodeIt)
		{
			if(!isTypeClass(memoryType,Class::id))
				{ return TypedExpression(recordError<Error>(outErrors,nodeIt,"load: memory type must be same type class as result"),resultType); }
			
			auto offset = parseOffsetAttribute(nodeIt);
			auto alignmentLog2 = parseAlignmentAttribute(nodeIt);

			auto address = parseTypedExpression<IntClass>(isFarAddress ? TypeId::I64 : TypeId::I32,nodeIt,"load address");

			auto result = new(arena) Load<Class>(loadOp,isFarAddress,alignmentLog2,offset,address,memoryType);
			return TypedExpression(requireFullMatch(nodeIt,"load",result),resultType);
		}

		// Parse a memory store operation.
		template<typename OperandClass>
		TypedExpression parseStoreExpression(TypeId valueType,TypeId memoryType,bool isFarAddress,SNodeIt nodeIt)
		{
			if(!isTypeClass(memoryType,OperandClass::id))
				{ return TypedExpression(recordError<Error>(outErrors,nodeIt,"store: memory type must be same type class as result"),TypeId::Void); }
			
			auto offset = parseOffsetAttribute(nodeIt);
			auto alignmentLog2 = parseAlignmentAttribute(nodeIt);

			auto address = parseTypedExpression<IntClass>(isFarAddress ? TypeId::I64 : TypeId::I32,nodeIt,"store address");
			auto value = parseTypedExpression<OperandClass>(valueType,nodeIt,"store value");
			auto result = new(arena) Store<OperandClass>(isFarAddress,alignmentLog2,offset,address,TypedExpression(value,valueType),memoryType);
			return TypedExpression(requireFullMatch(nodeIt,"store",result),valueType);
		}
		
		// Parse a cast operation.
		template<typename Class>
		TypedExpression parseCastExpression(typename Class::Op op,TypeId sourceType,TypeId destType,SNodeIt nodeIt)
		{
			auto source = parseTypedExpression(sourceType,nodeIt,"cast source");
			auto result = new(arena) Cast<Class>(op,TypedExpression(source,sourceType));
			return TypedExpression(requireFullMatch(nodeIt,getOpName(op),result),destType);
		}
		
		// Parses a load from a local variable.
		template<typename Class>
		typename Class::ClassExpression* parseGetLocal(TypeId resultType,const NameToIndexMap& nameToIndexMap,const std::vector<Variable>& variables,SNodeIt nodeIt)
		{
			uintptr variableIndex;
			if(!parseNameOrIndex(nodeIt,nameToIndexMap,variables.size(),variableIndex))
			{
				auto message = "get_local: expected local name or index";
				return as<Class>(recordError<Error>(outErrors,nodeIt,std::move(message)));
			}
			auto variableType = variables[variableIndex].type;
			auto load = new(arena) GetLocal(getPrimaryTypeClass(variableType),variableIndex);
			auto result = coerceExpression(Class(),resultType,TypedExpression(load,variableType),nodeIt,"variable");
			return requireFullMatch(nodeIt,"get_local",result);
		}

		// Parses a store to a local variable.
		template<typename Class>
		typename Class::ClassExpression* parseSetLocal(TypeId resultType,const NameToIndexMap& nameToIndexMap,const std::vector<Variable>& variables,SNodeIt nodeIt)
		{
			uintptr variableIndex;
			if(!parseNameOrIndex(nodeIt,nameToIndexMap,variables.size(),variableIndex))
			{
				auto message = "set_local: expected local name or index";
				return as<Class>(recordError<Error>(outErrors,nodeIt,message));
			}
			auto variableType = variables[variableIndex].type;
			auto valueExpression = parseTypedExpression(variableType,nodeIt,"store value");
			auto store = new(arena) SetLocal(getPrimaryTypeClass(variableType),valueExpression,variableIndex);
			auto result = coerceExpression(Class(),resultType,TypedExpression(store,variableType),nodeIt,"variable");
			return requireFullMatch(nodeIt,"set_local",result);
		}

		// Parses a label or a block.
		template<typename Class>
		typename Class::ClassExpression* parseLabeledBlock(TypeId resultType,SNodeIt nodeIt,const char* errorContext)
		{
			// Create a branch target for the block.
			auto branchTarget = new(arena)BranchTarget(resultType);
			
			// Parse an optional name for the label.
			const char* labelName = nullptr;
			bool hasName = parseName(nodeIt,labelName);
			ScopedBranchTarget scopedBranchTarget(*this,hasName,labelName,branchTarget,nodeIt);

			// Parse the block body.
			auto expression = parseExpressionSequence<Class>(resultType,nodeIt,errorContext);

			// Create the Label node.
			return new(arena)Label<Class>(branchTarget,expression);
		}

		// Parse an anonymously labeled expression.
		template<typename Class>
		typename Class::ClassExpression* parseAnonLabeledExpression(TypeId resultType,SNodeIt& nodeIt,const char* errorContext)
		{
			// Create a branch target for the block.
			auto branchTarget = new(arena)BranchTarget(resultType);
			ScopedBranchTarget scopedBranchTarget(*this,false,nullptr,branchTarget,nodeIt);

			// Parse the block body.
			auto expression = parseTypedExpression<Class>(resultType,nodeIt,errorContext);

			// Create the Label node.
			return new(arena)Label<Class>(branchTarget,expression);
		}

		BranchTarget* parseBranchTargetRef(SNodeIt& nodeIt)
		{
			// Parse the name or index of the target label.
			const char* name;
			uint64 parsedInt;
			if(parseUnsignedInt(nodeIt,parsedInt) && (uintptr)parsedInt < scopedBranchTargets.size())
			{
				return scopedBranchTargets[scopedBranchTargets.size() - 1 - (uintptr)parsedInt];
			}
			else if(parseName(nodeIt,name))
			{
				auto it = labelToBranchTargetMap.find(name);
				if(it != labelToBranchTargetMap.end()) { return it->second; }
			}
			return nullptr;
		}
	};

	void parseFunctionType(SNodeIt& nodeIt,std::vector<Variable>& outParameters,TypeId& outReturnType,Memory::Arena& arena,std::vector<ErrorRecord*>& outErrors)
	{
		bool hasResult = false;
		for(;nodeIt;++nodeIt)
		{
			SNodeIt childNodeIt;
			if(parseTaggedNode(nodeIt,Symbol::_result,childNodeIt))
			{
				// Parse a result declaration.
				if(hasResult) { recordError<ErrorRecord>(outErrors,nodeIt,"duplicate result declaration"); continue; }
				if(!parseType(childNodeIt,outReturnType)) { recordError<ErrorRecord>(outErrors,childNodeIt,"expected type"); continue; }
				hasResult = true;
				if(childNodeIt) { recordError<ErrorRecord>(outErrors,childNodeIt,"unexpected input following result declaration"); continue; }
			}
			else if(parseTaggedNode(nodeIt,Symbol::_param,childNodeIt))
			{
				// Parse a parameter declaration.
				parseVariables(childNodeIt,outParameters,outErrors,arena);
				if(childNodeIt) { recordError<ErrorRecord>(outErrors,childNodeIt,"unexpected input following parameter declaration"); continue; }
			}
			else { break; }
		}
	}

	Module* ModuleContext::parse(SNodeIt firstModuleChildNode)
	{
		// Do a first pass that only parses declarations before parsing definitions.
		bool hasMemoryNode = false;
		for(auto nodeIt = firstModuleChildNode;nodeIt;++nodeIt)
		{
			SNodeIt childNodeIt;
			Symbol tag;
			if(parseTreeNode(nodeIt,childNodeIt) && parseSymbol(childNodeIt,tag))
			{
				switch(tag)
				{
					case Symbol::_func:
					{
						auto function = new(module->arena) Function();
						auto functionIndex = module->functions.size();
						module->functions.push_back(function);

						// Parse an optional function name.
						const char* functionName;
						if(parseName(childNodeIt,functionName))
						{
							function->name = module->arena.copyToArena(functionName,strlen(functionName)+1);
							if(functionNameToIndexMap.count(functionName)) { recordError<ErrorRecord>(outErrors,childNodeIt,"duplicate function name"); }
							else { functionNameToIndexMap[functionName] = functionIndex; }
						}

						// Parse a module type reference.
						FunctionType referencedFunctionType;
						bool hasReferencedFunctionType = false;
						SNodeIt typeChildNodeIt;
						if(parseTaggedNode(childNodeIt,Symbol::_type,typeChildNodeIt))
						{
							// Parse a name or index into the module's type declarations.
							++childNodeIt;
							uintptr signatureIndex = 0;
							if(!parseNameOrIndex(typeChildNodeIt,signatureNameToIndexMap,signatures.size(),signatureIndex))
							{ recordError<ErrorRecord>(outErrors,typeChildNodeIt,"expected function type name or index"); continue; }
							referencedFunctionType = signatures[signatureIndex];
							hasReferencedFunctionType = true;
						}

						// Parse the function parameter and result types.
						parseFunctionType(childNodeIt,function->locals,function->type.returnType,module->arena,outErrors);
						for(uintptr parameterIndex = 0;parameterIndex < function->locals.size();++parameterIndex)
						{
							function->parameterLocalIndices.push_back(parameterIndex);
							function->type.parameters.push_back(function->locals[parameterIndex].type);
						}

						// If there's both a type reference and explicit function signature, then make sure they match.
						if(function->type.parameters.size() || function->type.returnType != TypeId::Void)
						{
							if(hasReferencedFunctionType && function->type != referencedFunctionType)
							{
								recordError<ErrorRecord>(outErrors,nodeIt,std::string("type reference doesn't match function signature"));
								continue;
							}
						}
						else if(hasReferencedFunctionType)
						{
							// If there's only a referenced type, use it.
							function->type = std::move(referencedFunctionType);
							for(auto parameterType : function->type.parameters)
							{
								function->parameterLocalIndices.push_back(function->locals.size());
								function->locals.push_back({parameterType,nullptr});
							}
						}

						for(;childNodeIt;++childNodeIt)
						{
							SNodeIt innerChildNodeIt;
							if(parseTaggedNode(childNodeIt,Symbol::_local,innerChildNodeIt))
							{
								// Parse a local declaration.
								parseVariables(innerChildNodeIt,function->locals,outErrors,module->arena);
								if(innerChildNodeIt) { recordError<ErrorRecord>(outErrors,innerChildNodeIt,"unexpected input following local declaration"); continue; }
							}
							else { break; } // Stop parsing when we reach the first func child that isn't a param, result, or local.
						}

						break;
					}
					case Symbol::_import:
					{
						// Parse an optional import name used within the module.
						const char* importInternalName;
						bool hasName = parseName(childNodeIt,importInternalName);

						// Parse a mandatory import module name.
						const char* importModuleName;
						size_t importModuleNameLength;
						if(!parseString(childNodeIt,importModuleName,importModuleNameLength,module->arena))
						{ recordError<ErrorRecord>(outErrors,childNodeIt,"expected import module name string"); continue; }

						// Parse a mandary import function name.
						const char* importFunctionName;
						size_t importFunctionNameLength;
						if(!parseString(childNodeIt,importFunctionName,importFunctionNameLength,module->arena))
						{ recordError<ErrorRecord>(outErrors,childNodeIt,"expected import function name string"); continue; }

						FunctionType importType;
						SNodeIt typeChildNodeIt;
						if(parseTaggedNode(childNodeIt,Symbol::_type,typeChildNodeIt))
						{
							// Parse a name or index into the module's type declarations.
							++childNodeIt;
							uintptr signatureIndex = 0;
							if(!parseNameOrIndex(typeChildNodeIt,signatureNameToIndexMap,signatures.size(),signatureIndex))
							{ recordError<ErrorRecord>(outErrors,typeChildNodeIt,"expected function type name or index"); continue; }
							importType = signatures[signatureIndex];
						}
						else
						{
							// Parse the import's parameter and result declarations.
							std::vector<Variable> parameters;
							TypeId returnType = TypeId::Void;
							parseFunctionType(childNodeIt,parameters,returnType,module->arena,outErrors);
							std::vector<TypeId> parameterTypes;
							for(auto parameter : parameters) { parameterTypes.push_back(parameter.type); }
							importType = FunctionType(returnType,parameterTypes);
						}
				
						// Create the import.
						auto importIndex = module->functionImports.size();
						module->functionImports.push_back({importType,importModuleName,importFunctionName});
						if(hasName)
						{
							importInternalName = module->arena.copyToArena(importInternalName,strlen(importInternalName) + 1);
							if(functionImportNameToIndexMap.count(importInternalName)) { recordError<ErrorRecord>(outErrors,SNodeIt(nullptr),"duplicate variable name"); }
							else { functionImportNameToIndexMap[importInternalName] = importIndex; }
						}

						if(childNodeIt) { recordError<ErrorRecord>(outErrors,childNodeIt,"unexpected input following import declaration"); continue; }
						
						break;
					}
					case Symbol::_memory:
					{
						// Parse a memory declaration.
						if(hasMemoryNode) { recordError<ErrorRecord>(outErrors,nodeIt,"duplicate memory declaration"); continue; }
						hasMemoryNode = true;

						// Parse the initial and maximum number of bytes.
						// If one number is found, it is taken to be both the initial and max.
						int64 initialNumPages;
						int64 maxNumPages;
						if(!parseInt(childNodeIt,initialNumPages))
							{ recordError<ErrorRecord>(outErrors,childNodeIt,"expected initial memory size integer"); continue; }
						if(!parseInt(childNodeIt,maxNumPages))
							{ maxNumPages = initialNumPages; }
						if(maxNumPages > (1ll<<32))
							{ recordError<ErrorRecord>(outErrors,childNodeIt,"maximum memory size must be <=2^32 bytes"); continue; }
						if(initialNumPages > maxNumPages)
							{ recordError<ErrorRecord>(outErrors,childNodeIt,"initial memory size must be <= maximum memory size"); continue; }
						module->initialNumPagesMemory = (uint64) initialNumPages;
						module->maxNumPagesMemory = (uint64) maxNumPages;
				
						// Parse the memory segments.
						for(;childNodeIt;++childNodeIt)
						{
							SNodeIt segmentChildNodeIt;
							int64 baseAddress;
							const char* dataString;
							size_t dataLength;
							if(!parseTaggedNode(childNodeIt,Symbol::_segment,segmentChildNodeIt))
								{ recordError<ErrorRecord>(outErrors,segmentChildNodeIt,"expected segment declaration"); continue; }
							if(!parseInt(segmentChildNodeIt,baseAddress))
								{ recordError<ErrorRecord>(outErrors,segmentChildNodeIt,"expected segment base address integer"); continue; }
							if(!parseString(segmentChildNodeIt,dataString,dataLength,module->arena))
								{ recordError<ErrorRecord>(outErrors,segmentChildNodeIt,"expected segment data string"); continue; }
							if(	(uint64)baseAddress + dataLength < (uint64)baseAddress // Check for integer overflow in baseAddress+dataLength.
							||	(uint64)baseAddress + dataLength > module->initialNumPagesMemory * AST::numBytesPerPage )
								{ recordError<ErrorRecord>(outErrors,segmentChildNodeIt,"data segment bounds aren't contained by initial memory size"); continue; }
							module->dataSegments.push_back({(uint64)baseAddress,dataLength,(uint8*)dataString});
						}

						if(childNodeIt) { recordError<ErrorRecord>(outErrors,childNodeIt,"unexpected input following memory declaration"); continue; }
						
						break;
					}
					case Symbol::_type:
					{
						// Parse a function signature definition.
						const char* signatureName;
						bool hasName = parseName(childNodeIt,signatureName);

						SNodeIt funcChildNodeIt;
						if(!parseTaggedNode(childNodeIt++,Symbol::_func,funcChildNodeIt))
						{ recordError<ErrorRecord>(outErrors,childNodeIt,"expected func declaration"); continue; }

						FunctionType functionType;
						std::vector<Variable> parameters;
						parseFunctionType(funcChildNodeIt,parameters,functionType.returnType,module->arena,outErrors);
						functionType.parameters.resize(parameters.size());
						for(uintptr parameterIndex = 0;parameterIndex < parameters.size();++parameterIndex)
						{
							functionType.parameters[parameterIndex] = parameters[parameterIndex].type;
						}
						if(funcChildNodeIt) { recordError<ErrorRecord>(outErrors,childNodeIt,"unexpected input following function type declaration"); continue; }

						auto signatureIndex = signatures.size();
						signatures.push_back(functionType);

						if(hasName) { signatureNameToIndexMap[signatureName] = signatureIndex; }

						if(childNodeIt) { recordError<ErrorRecord>(outErrors,childNodeIt,"unexpected input following type declaration"); continue; }
						
						break;
					}
					case Symbol::_export:
					case Symbol::_start:
					case Symbol::_table:
						break;
					default:
						recordError<ErrorRecord>(outErrors,nodeIt,"unrecognized declaration");
						break;
				}
			}
		}

		// Parse function tables and the start function reference in a second pass, so it has information about all functions.
		bool hasFunctionTable = false;
		for(auto nodeIt = firstModuleChildNode;nodeIt;++nodeIt)
		{
			SNodeIt childNodeIt;
			if(parseTaggedNode(nodeIt,Symbol::_table,childNodeIt))
			{
				if(hasFunctionTable) { recordError<ErrorRecord>(outErrors,nodeIt,"duplicate function table"); continue; }

				// Count the number of functions in the table.
				size_t numFunctions = 0;
				for(auto countNodeIt = childNodeIt;countNodeIt;++countNodeIt)
				{ ++numFunctions; }

				FunctionType functionType;
				auto functionIndices = new(module->arena) uintptr[numFunctions];
				if(!numFunctions) { recordError<ErrorRecord>(outErrors,nodeIt,"function table must contain atleast 1 function"); }
				else
				{
					// Parse the function indices or names.
					for(uintptr index = 0;index < numFunctions;++index)
					{
						uintptr functionIndex;
						if(!parseNameOrIndex(childNodeIt,functionNameToIndexMap,module->functions.size(),functionIndex))
							{ functionIndices[index] = 0; recordError<ErrorRecord>(outErrors,childNodeIt,"expected function name or index"); }
						else { functionIndices[index] = functionIndex; }
					}
				}
				module->functionTable.functionIndices = functionIndices;
				module->functionTable.numFunctions = numFunctions;
				hasFunctionTable = true;
			}
			else if(parseTaggedNode(nodeIt,Symbol::_start,childNodeIt))
			{
				uintptr functionIndex;
				if(!parseNameOrIndex(childNodeIt,functionNameToIndexMap,module->functions.size(),functionIndex))
					{ recordError<ErrorRecord>(outErrors,childNodeIt,"expected function name or index"); }
				else if(module->functions[functionIndex]->type != FunctionType())
					{ recordError<ErrorRecord>(outErrors,childNodeIt,"start function must be of type ()->Void"); }
				else { module->startFunctionIndex = functionIndex; }
			}
		}

		// Parse function definitions last, so it has information about all other function types and the function table.
		intptr currentFunctionIndex = 0;
		for(auto nodeIt = firstModuleChildNode;nodeIt;++nodeIt)
		{
			SNodeIt childNodeIt;
			if(parseTaggedNode(nodeIt,Symbol::_func,childNodeIt))
			{
				// Process nodes until the first node that isn't the function name, a param, local, or result.
				const char* functionName;
				parseName(childNodeIt,functionName);

				for(;childNodeIt;++childNodeIt)
				{
					SNodeIt childChildNodeIt;
					if(	!parseTaggedNode(childNodeIt,Symbol::_local,childChildNodeIt)
					&&	!parseTaggedNode(childNodeIt,Symbol::_type,childChildNodeIt)
					&&	!parseTaggedNode(childNodeIt,Symbol::_param,childChildNodeIt)
					&&	!parseTaggedNode(childNodeIt,Symbol::_result,childChildNodeIt))
					{ break; }
				};

				// Parse the function's body.
				auto function = module->functions[currentFunctionIndex++];
				FunctionContext functionContext(*this,function);
				function->expression = functionContext.parseExpressionSequence(function->type.returnType,childNodeIt,"function body");
			}
			else if(parseTaggedNode(nodeIt,Symbol::_export,childNodeIt))
			{
				// Parse an export definition.
				const char* exportName;
				size_t nameLength;
				if(!parseString(childNodeIt,exportName,nameLength,module->arena))
					{ recordError<ErrorRecord>(outErrors,childNodeIt,"expected export name string"); continue; }
				uintptr functionIndex;
				if(parseNameOrIndex(childNodeIt,functionNameToIndexMap,module->functions.size(),functionIndex))
				{
					module->exportNameToFunctionIndexMap[exportName] = functionIndex;
				}
				else
				{
					Symbol symbol;
					if(parseSymbol(childNodeIt,symbol) && symbol == Symbol::_memory)
					{
						// Ignore the memory export declaration for now.
					}
					else
					{
						recordError<ErrorRecord>(outErrors,childNodeIt,"expected function name or index or 'memory'");
						continue;
					}
				}
				if(childNodeIt) { recordError<ErrorRecord>(outErrors,childNodeIt,"unexpected input following export declaration"); continue; }
			}
		}
		
		return module;
	}

	Runtime::Value parseRuntimeValue(SNodeIt nodeIt,std::vector<ErrorRecord*>& outErrors)
	{
		SNodeIt childNodeIt;
		Symbol symbol;
		int64 integerValue;
		float64 f64Value;
		float32 f32Value;
		if(parseTreeNode(nodeIt,childNodeIt) && parseSymbol(childNodeIt,symbol))
		{
			switch(symbol)
			{
			case Symbol::_const_I8:
				if(!parseInt(childNodeIt,integerValue)) { recordError<ErrorRecord>(outErrors,childNodeIt,"const: expected integer"); return Runtime::Value(); }
				else { return Runtime::Value((uint8)integerValue); }
			case Symbol::_const_I16:
				if(!parseInt(childNodeIt,integerValue)) { recordError<ErrorRecord>(outErrors,childNodeIt,"const: expected integer"); return Runtime::Value(); }
				else { return Runtime::Value((uint16)integerValue); }
			case Symbol::_const_I32:
				if(!parseInt(childNodeIt,integerValue)) { recordError<ErrorRecord>(outErrors,childNodeIt,"const: expected integer"); return Runtime::Value(); }
				else { return Runtime::Value((uint32)integerValue); }
			case Symbol::_const_I64:
				if(!parseInt(childNodeIt,integerValue)) { recordError<ErrorRecord>(outErrors,childNodeIt,"const: expected integer"); return Runtime::Value(); }
				else { return Runtime::Value((uint64)integerValue); }
			case Symbol::_const_F32:
				if(!parseFloat(childNodeIt,f64Value,f32Value)) { recordError<ErrorRecord>(outErrors,childNodeIt,"const: expected floating point number"); return Runtime::Value(); }
				else { return Runtime::Value(f32Value); }
			case Symbol::_const_F64:
				if(!parseFloat(childNodeIt,f64Value,f32Value)) { recordError<ErrorRecord>(outErrors,childNodeIt,"const: expected floating point number"); return Runtime::Value(); }
				else { return Runtime::Value(f64Value); }
			default:;
			};
		}

		recordError<ErrorRecord>(outErrors,nodeIt,"expected const expression");
		return Runtime::Value();
	}

	Invoke* parseInvoke(SNodeIt nodeIt,uintptr moduleIndex,File& outFile)
	{
		SNodeIt originalNodeIt = nodeIt;

		SNodeIt invokeChildIt;
		if(!parseTaggedNode(nodeIt++,Symbol::_invoke,invokeChildIt))
			{ recordError<ErrorRecord>(outFile.errors,nodeIt,"expected invoke expression"); return nullptr; }

		// Parse the export name to invoke.
		Memory::ScopedArena scopedArena;
		const char* invokeExportName;
		size_t invokeExportNameLength;
		SNodeIt savedExportNameIt = invokeChildIt;
		if(!parseString(invokeChildIt,invokeExportName,invokeExportNameLength,scopedArena))
			{ recordError<ErrorRecord>(outFile.errors,invokeChildIt,"expected export name string"); return nullptr; }

		// Find the named export in one of the modules.
		AST::Module* exportModule = outFile.modules[moduleIndex];
		auto exportIt = exportModule->exportNameToFunctionIndexMap.find(invokeExportName);
		if(exportIt == exportModule->exportNameToFunctionIndexMap.end())
		{ recordError<ErrorRecord>(outFile.errors,savedExportNameIt,"couldn't find export with this name"); return nullptr; }
		auto exportedFunctionIndex = exportIt->second;

		// Parse the invoke's parameters.
		auto function = exportModule->functions[exportedFunctionIndex];
		std::vector<Runtime::Value> parameters(function->type.parameters.size());
		for(uintptr parameterIndex = 0;parameterIndex < function->type.parameters.size();++parameterIndex)
			{ parameters[parameterIndex] = parseRuntimeValue(invokeChildIt++,outFile.errors); }

		// Verify that all of the invoke's parameters were matched.
		if(invokeChildIt) { recordExcessInputError<ErrorRecord>(outFile.errors,invokeChildIt,"invoke parameters"); return nullptr; }

		auto result = new(exportModule->arena) Invoke;
		result->locus = originalNodeIt->startLocus;
		result->functionIndex = exportedFunctionIndex;
		result->parameters = std::move(parameters);
		return result;
	}

	Assert* parseAssertReturn(Core::TextFileLocus locus,SNodeIt nodeIt,uintptr moduleIndex,File& outFile)
	{
		// Parse the assert_return's invoke.
		auto invoke = parseInvoke(nodeIt++,moduleIndex,outFile);
		if(!invoke) { return nullptr; }

		// Parse the expected value of the invoke.
		auto value = nodeIt ? parseRuntimeValue(nodeIt++,outFile.errors) : Runtime::Value(Runtime::Void());

		// Verify that all of the assert_return's parameters were matched.
		if(nodeIt) { recordExcessInputError<ErrorRecord>(outFile.errors,nodeIt,"assert_return expected value"); return nullptr; }

		auto result = new(outFile.modules[moduleIndex]->arena) Assert;
		result->invoke = invoke;
		result->locus = locus;
		result->value = value;
		return result;
	}
	
	AssertNaN* parseAssertReturnNaN(Core::TextFileLocus locus,SNodeIt nodeIt,uintptr moduleIndex,File& outFile)
	{
		// Parse the assert_return's invoke.
		auto invoke = parseInvoke(nodeIt++,moduleIndex,outFile);
		if(!invoke) { return nullptr; }

		// Verify that all of the assert_return_nan's parameters were matched.
		if(nodeIt) { recordExcessInputError<ErrorRecord>(outFile.errors,nodeIt,"assert_return expected value"); return nullptr; }

		auto result = new(outFile.modules[moduleIndex]->arena) AssertNaN;
		result->invoke = invoke;
		result->locus = locus;
		return result;
	}
	
	Assert* parseAssertTrap(Core::TextFileLocus locus,SNodeIt nodeIt,uintptr moduleIndex,File& outFile)
	{
		// Parse the assert_trap's invoke.
		auto invoke = parseInvoke(nodeIt++,moduleIndex,outFile);
		if(!invoke) { return nullptr; }
		
		// Set up a dummy module, function, and parsing context to parse the assertion value in.
		Function dummyFunction;
		ModuleContext dummyModuleContext(outFile.modules[moduleIndex],outFile.errors);
		FunctionContext dummyFunctionContext(dummyModuleContext,&dummyFunction);

		// Parse the expected value of the invoke.
		const char* message;
		size_t numMessageChars;
		if(!parseString(nodeIt,message,numMessageChars,outFile.modules[moduleIndex]->arena))
			{ recordError<ErrorRecord>(outFile.errors,nodeIt,"expected trap message"); return nullptr; }

		// Verify that all of the assert_trap's parameters were matched.
		if(nodeIt) { recordExcessInputError<ErrorRecord>(outFile.errors,nodeIt,"assert_return expected value"); return nullptr; }
		
		// Try to map the trap message to an exception cause.
		Runtime::Exception::Cause cause = Runtime::Exception::Cause::Unknown;
		if(!strcmp(message,"out of bounds memory access")) { cause = Runtime::Exception::Cause::AccessViolation; }
		else if(!strcmp(message,"callstack exhausted")) { cause = Runtime::Exception::Cause::StackOverflow; }
		else if(!strcmp(message,"integer overflow")) { cause = Runtime::Exception::Cause::IntegerDivideByZeroOrIntegerOverflow; }
		else if(!strcmp(message,"integer divide by zero")) { cause = Runtime::Exception::Cause::IntegerDivideByZeroOrIntegerOverflow; }
		else if(!strcmp(message,"invalid conversion to integer")) { cause = Runtime::Exception::Cause::InvalidFloatOperation; }
		else if(!strcmp(message,"call stack exhausted")) { cause = Runtime::Exception::Cause::StackOverflow; }
		else if(!strcmp(message,"memory size exceeds implementation limit")) { cause = Runtime::Exception::Cause::OutOfMemory; }
		else if(!strcmp(message,"growing memory by non-multiple of page size")) { cause = Runtime::Exception::Cause::GrowMemoryNotPageAligned; }
		else if(!strcmp(message,"unreachable executed")) { cause = Runtime::Exception::Cause::ReachedUnreachable; }
		else if(!strcmp(message,"indirect call signature mismatch")) { cause = Runtime::Exception::Cause::IndirectCallSignatureMismatch; }
		else if(!strncmp(message,"undefined table index",21)) { cause = Runtime::Exception::Cause::OutOfBoundsFunctionTableIndex; }

		auto result = new(outFile.modules[moduleIndex]->arena) Assert;
		result->invoke = invoke;
		result->locus = locus;
		result->value = Runtime::Value(new Runtime::Exception {cause});
		return result;
	}

	// Parses a module from a WAST string.
	bool parse(const char* string,File& outFile)
	{
		const SExp::SymbolIndexMap& symbolIndexMap = getWASTSymbolIndexMap();
		
		// Parse S-expressions from the string.
		Memory::ScopedArena scopedArena;
		auto rootNode = SExp::parse(string,scopedArena,symbolIndexMap);
		
		// Parse modules from S-expressions.
		for(auto rootNodeIt = SNodeIt(rootNode);rootNodeIt;++rootNodeIt)
		{
			SNodeIt childNodeIt;
			if(parseTaggedNode(rootNodeIt,Symbol::_module,childNodeIt))
			{
				// Parse a module definition.
				outFile.modules.push_back(ModuleContext(new Module(),outFile.errors).parse(childNodeIt));
			}
		}
		
		// Parse the test statements.
		outFile.moduleTests.resize(outFile.modules.size());
		uintptr currentModuleIndex = UINTPTR_MAX;
		for(auto rootNodeIt = SNodeIt(rootNode);rootNodeIt;++rootNodeIt)
		{
			SNodeIt childNodeIt;
			if(currentModuleIndex < outFile.modules.size() && parseTaggedNode(rootNodeIt,Symbol::_invoke,childNodeIt))
			{
				auto invoke = parseInvoke(rootNodeIt,currentModuleIndex,outFile);
				if(invoke) { outFile.moduleTests[currentModuleIndex].push_back(invoke); }
			}
			else if(currentModuleIndex < outFile.modules.size() && parseTaggedNode(rootNodeIt,Symbol::_assert_return,childNodeIt))
			{
				assert(currentModuleIndex < outFile.modules.size());
				auto assert = parseAssertReturn(rootNodeIt->startLocus,childNodeIt,currentModuleIndex,outFile);
				if(assert) { outFile.moduleTests[currentModuleIndex].push_back(assert); }
			}
			else if(currentModuleIndex < outFile.modules.size() && parseTaggedNode(rootNodeIt,Symbol::_assert_return_nan,childNodeIt))
			{
				assert(currentModuleIndex < outFile.modules.size());
				auto assert = parseAssertReturnNaN(rootNodeIt->startLocus,childNodeIt,currentModuleIndex,outFile);
				if(assert) { outFile.moduleTests[currentModuleIndex].push_back(assert); }
			}
			else if(currentModuleIndex < outFile.modules.size() && parseTaggedNode(rootNodeIt,Symbol::_assert_trap,childNodeIt))
			{
				assert(currentModuleIndex < outFile.modules.size());
				auto assertTrap = parseAssertTrap(rootNodeIt->startLocus,childNodeIt,currentModuleIndex,outFile);
				if(assertTrap) { outFile.moduleTests[currentModuleIndex].push_back(assertTrap); }
			}
			else if(parseTaggedNode(rootNodeIt,Symbol::_module,childNodeIt))
			{
				++currentModuleIndex;
			}
		}

		return !outFile.errors.size();
	}
}
