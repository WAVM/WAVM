#include "Core/Core.h"
#include "Core/Floats.h"
#include "Core/SExpressions.h"
#include "WAST.h"
#include "WASTSymbols.h"
#include "WebAssembly/Module.h"
#include "WebAssembly/Types.h"
#include "WebAssembly/Operations.h"

#include <map>

#ifdef _WIN32
	#pragma warning(disable:4702) // unreachable code
#endif

using namespace WebAssembly;

namespace WAST
{
	using namespace SExp;
	
	typedef std::map<std::string,uintptr> NameToIndexMap;
	typedef std::map<std::string,uintptr> NameToBranchTargetMap;
	
	enum class ExpressionType : uint8
	{
		none = (uint8)ResultType::none,
		i32 = (uint8)ResultType::i32,
		i64 = (uint8)ResultType::i64,
		f32 = (uint8)ResultType::f32,
		f64 = (uint8)ResultType::f64,
		unreachable
	};
	
	static ExpressionType asExpressionType(ValueType type)
	{
		assert(type != ValueType::invalid && type <= ValueType::max);
		return (ExpressionType)type;
	}
	
	static ExpressionType asExpressionType(ResultType type)
	{
		assert(type <= ResultType::max);
		return (ExpressionType)type;
	}

	static ResultType asResultType(ExpressionType type)
	{
		assert(type <= (ExpressionType)ResultType::max);
		return (ResultType)type;
	}
	
	static ExpressionType intersectTypes(ExpressionType a,ExpressionType b)
	{
		if(a == b) { return a; }
		else { return ExpressionType::unreachable; }
	}
	
	const char* getTypeName(ExpressionType type)
	{
		switch(type)
		{
		case ExpressionType::none: return "()";
		case ExpressionType::i32: return "i32";
		case ExpressionType::i64: return "i64";
		case ExpressionType::f32: return "f32";
		case ExpressionType::f64: return "f64";
		case ExpressionType::unreachable: return "unreachable";
		default: Core::unreachable();
		};
	}

	// Parse a type from a S-expression node.
	bool parseType(SNodeIt& nodeIt,ValueType& outType)
	{
		if(nodeIt && nodeIt->type == SNodeType::Symbol)
		{
			switch((Symbol)nodeIt->symbol)
			{
			case Symbol::_i32: outType = ValueType::i32; break;
			case Symbol::_i64: outType = ValueType::i64; break;
			case Symbol::_f32: outType = ValueType::f32; break;
			case Symbol::_f64: outType = ValueType::f64; break;
			default: return false;
			};
			++nodeIt;
			return true;
		}
		else { return false; }
	}

	// Parse a global type from a S-expression node.
	bool parseGlobalType(SNodeIt& nodeIt,GlobalType& outType)
	{
		SNodeIt mutChildIt;
		if(nodeIt && nodeIt->type == SNodeType::Symbol)
		{
			if(!parseType(nodeIt,outType.valueType)) { return false; }
			outType.isMutable = false;
			return true;
		}
		else if(parseTaggedNode(nodeIt,Symbol::_mut,mutChildIt))
		{
			if(!parseType(mutChildIt,outType.valueType)) { return false; }
			outType.isMutable = true;
			++nodeIt;
			return true;
		}
		else { return false; }
	}

	// Context that is shared between functions parsing a module.
	struct ModuleContext
	{
		Module& module;
		std::vector<Error>& errors;

		NameToIndexMap functionNameToIndexMap;
		NameToIndexMap memoryNameToIndexMap;
		NameToIndexMap tableNameToIndexMap;
		NameToIndexMap exportNameToIndexMap;
		NameToIndexMap globalNameToIndexMap;
		std::map<const FunctionType*,uintptr> functionTypeToTypeIndexMap;
		std::vector<GlobalType> globalTypes;
		std::vector<MemoryType> memoryTypes;
		std::vector<TableType> tableTypes;
		std::vector<uintptr> functionTypes;
		
		NameToIndexMap signatureNameToIndexMap;
		std::vector<const FunctionType*> signatures;

		ModuleContext(Module& inModule,std::vector<Error>& inErrors): module(inModule), errors(inErrors) {}

		uintptr getFunctionTypeIndex(const FunctionType* functionType);
		bool parseInitializerExpression(SNodeIt& nodeIt,ValueType expectedType,InitializerExpression& outExpression);

		void parse(SNodeIt moduleNode);
	};

	void recordError(std::vector<Error>& outErrors,const Core::TextFileLocus& locus,std::string&& message)
	{
		outErrors.push_back({locus,std::move(message)});
	}

	// Creates and records an error with the given message and location.
	void recordError(ModuleContext& moduleContext,const Core::TextFileLocus& locus,std::string&& message)
	{
		recordError(moduleContext.errors,locus,std::move(message));
	}
	
	// Creates and record a S-expression error node.
	void recordSExpError(ModuleContext& moduleContext,SNode* node)
	{
		recordError(moduleContext,node->startLocus,node->error);
	}

	// Creates and records an error with the given message and location (taken from nodeIt).
	void recordError(ModuleContext& moduleContext,SNodeIt nodeIt,std::string&& message)
	{
		// If the referenced node is a S-expression error, pass it through as well.
		if(nodeIt && nodeIt->type == SNodeType::Error) { recordSExpError(moduleContext,nodeIt); }

		const Core::TextFileLocus& locus = nodeIt.node ? nodeIt.node->startLocus : nodeIt.previousLocus;
		recordError(moduleContext,locus,std::move(message));
	}
	
	void recordExcessInputError(ModuleContext& moduleContext,SNodeIt nodeIt,const char* errorContext)
	{
		auto message = std::string("unexpected input following ") + errorContext;
		recordError(moduleContext,nodeIt,std::move(message));
	}
	
	// Parse a name or an index.
	// If a name is parsed that is contained in nameToIndex, the index of the name is assigned to outIndex and true is returned.
	// If an index is parsed that is between 0 and numValidIndices, the index is assigned to outIndex and true is returned.
	bool parseNameOrIndex(ModuleContext& moduleContext,SNodeIt& nodeIt,const NameToIndexMap& nameToIndexMap,size_t numValidIndices,bool isOptional,const char* errorContext,uintptr& outIndex)
	{
		SNodeIt savedNodeIt = nodeIt;
		const char* name = nullptr;
		if(parseName(nodeIt,name))
		{
			auto mapIt = nameToIndexMap.find(name);
			if(mapIt == nameToIndexMap.end()) { recordError(moduleContext,savedNodeIt,std::string(errorContext) + ": not a valid name"); return false; }
			else { outIndex = mapIt->second; assert(outIndex < numValidIndices); return true; }
		}
		else if(parseUnsignedInt(nodeIt,outIndex))
		{
			if(outIndex >= numValidIndices) { recordError(moduleContext,savedNodeIt,std::string(errorContext) + ": not a valid index"); return false; }
			else { return true; }
		}
		else
		{
			if(!isOptional) { recordError(moduleContext,savedNodeIt,std::string(errorContext) + ": not a valid name or index"); }
			return false;
		}
	}

	// Parse a variable from the child nodes of a local, or param node. Format is (name type) | type+
	template<typename Allocator>
	size_t parseVariables(ModuleContext& moduleContext,SNodeIt& childNodeIt,std::vector<ValueType,Allocator>& outTypes,std::vector<std::string>& outNames)
	{
		const char* name;
		if(parseName(childNodeIt,name))
		{
			ValueType type;
			if(!parseType(childNodeIt,type)) { recordError(moduleContext,childNodeIt,std::string("expected type for: '") + name + "'"); return 0; }
			outTypes.push_back(type);
			outNames.push_back(name);
			return 1;
		}
		else
		{
			size_t numVariables = 0;
			while(childNodeIt)
			{
				ValueType type;
				if(!parseType(childNodeIt,type)) { recordError(moduleContext,childNodeIt,"expected type"); return numVariables; }
				outTypes.push_back(type);
				outNames.push_back("");
				++numVariables;
			}
			return numVariables;
		}
	}
	
	// Builds a map from name to index from an array of variables.
	void buildVariableNameToIndexMapMap(ModuleContext& moduleContext,const std::vector<std::string>& variableNames,NameToIndexMap& outNameToIndexMap)
	{
		for(uintptr variableIndex = 0;variableIndex < variableNames.size();++variableIndex)
		{
			const auto& variableName = variableNames[variableIndex];
			if(variableName.size())
			{
				if(outNameToIndexMap.count(variableName)) { recordError(moduleContext,SNodeIt(nullptr),std::string("duplicate variable name: ") + variableName); }
				else { outNameToIndexMap[variableName] = variableIndex; }
			}
		}
	}
	
	void parseFunctionType(ModuleContext& moduleContext,SNodeIt& nodeIt,const FunctionType*& outFunctionType,std::vector<std::string>& outParameterNames)
	{
		ResultType returnType = ResultType::none;
		std::vector<ValueType> parameterTypes;
		for(;nodeIt;++nodeIt)
		{
			SNodeIt childNodeIt;
			if(parseTaggedNode(nodeIt,Symbol::_result,childNodeIt))
			{
				// Parse a result declaration.
				if(returnType != ResultType::none) { recordError(moduleContext,nodeIt,"duplicate result declaration"); continue; }
				ValueType valueType;
				if(!parseType(childNodeIt,valueType)) { recordError(moduleContext,childNodeIt,"expected type"); continue; }
				returnType = asResultType(valueType);
				if(childNodeIt) { recordError(moduleContext,childNodeIt,"unexpected input following result declaration"); continue; }
			}
			else if(parseTaggedNode(nodeIt,Symbol::_param,childNodeIt))
			{
				if(returnType != ResultType::none) { recordError(moduleContext,nodeIt,"unexpected param following result declaration"); continue; }
				// Parse a parameter declaration.
				parseVariables(moduleContext,childNodeIt,parameterTypes,outParameterNames);
				if(childNodeIt) { recordError(moduleContext,childNodeIt,"unexpected input following parameter declaration"); continue; }
			}
			else { break; }
		}
		outFunctionType = FunctionType::get(returnType,parameterTypes);
	}

	bool parseSizeConstraints(ModuleContext& moduleContext,SNodeIt& nodeIt,SizeConstraints& outSizeConstraints,size_t maxMax)
	{
		// Parse the minimum and an optional maximum.
		SNodeIt minNodeIt = nodeIt;
		if(!parseUnsignedInt(nodeIt,outSizeConstraints.min)) { return false; }
		SNodeIt maxNodeIt = nodeIt;
		if(!parseUnsignedInt(nodeIt,outSizeConstraints.max)) { outSizeConstraints.max = UINT64_MAX; }

		if(outSizeConstraints.max != UINT64_MAX && outSizeConstraints.max > maxMax)
		{
			recordError(moduleContext,maxNodeIt,std::string("maximum size: '") + std::to_string(outSizeConstraints.max) + "' must be <= " + std::to_string(maxMax));
			return false;
		}
		else if(outSizeConstraints.min > outSizeConstraints.max || outSizeConstraints.min > maxMax)
		{
			recordError(moduleContext,minNodeIt,std::string("minimum size is greater than maximum: ") + std::to_string(outSizeConstraints.min) + " >= " + std::to_string(outSizeConstraints.max));
			return false;
		}

		return true;
	}
	
	// Parse a table type.
	bool parseTableType(ModuleContext& moduleContext,SNodeIt& nodeIt,TableType& outType)
	{
		// Parse the table size.
		if(!parseSizeConstraints(moduleContext,nodeIt,outType.size,UINT64_MAX))
		{
			outType.size.min = UINT64_MAX;
			outType.size.max = UINT64_MAX;
		}

		// Parse the table element type: it must be anyfunc for now.
		SNodeIt typeNodeIt = nodeIt;
		Symbol elementTypeSymbol;
		if(!parseSymbol(nodeIt,elementTypeSymbol) || elementTypeSymbol != Symbol::_anyfunc)
		{
			recordError(moduleContext,typeNodeIt,"expected table element type (must be anyfunc)");
		}
		outType.elementType = TableElementType::anyfunc;

		return true;
	}

	// Parse a memory type.
	bool parseMemoryType(ModuleContext& moduleContext,SNodeIt& nodeIt,MemoryType& outType)
	{
		// Parse the memory size.
		if(!parseSizeConstraints(moduleContext,nodeIt,outType.size,WebAssembly::maxMemoryPages))
		{
			outType.size.min = UINT64_MAX;
			outType.size.max = UINT64_MAX;
		}
		return true;
	}

	struct InlineImportExport
	{
		enum class Type
		{
			none,
			imported,
			exported
		};
		Type type;
		std::string moduleName;
		std::string exportName;
		InlineImportExport(): type(Type::none) {}
	};

	bool parseInlineImportOrExportDeclaration(ModuleContext& moduleContext,SNodeIt& nodeIt,InlineImportExport& outInlineImportExport)
	{
		SNodeIt childNodeIt;
		if(parseTaggedNode(nodeIt,Symbol::_export,childNodeIt))
		{
			if(!parseString(childNodeIt,outInlineImportExport.exportName)) { recordError(moduleContext,childNodeIt,"expected export name string"); return false; }
			if(childNodeIt) { recordError(moduleContext,childNodeIt,"unexpected input"); }
			outInlineImportExport.type = InlineImportExport::Type::exported;
			++nodeIt;
			return true;
		}
		else if(parseTaggedNode(nodeIt,Symbol::_import,childNodeIt))
		{
			if(!parseString(childNodeIt,outInlineImportExport.moduleName)) { recordError(moduleContext,childNodeIt,"expected import module string"); return false; }
			if(!parseString(childNodeIt,outInlineImportExport.exportName)) { recordError(moduleContext,childNodeIt,"expected import export name string"); return false; }
			if(childNodeIt) { recordError(moduleContext,childNodeIt,"unexpected input"); }
			outInlineImportExport.type = InlineImportExport::Type::imported;
			++nodeIt;
			return true;
		}
		else { return false; }
	}

	uintptr ModuleContext::getFunctionTypeIndex(const FunctionType* functionType)
	{
		auto existingIt = functionTypeToTypeIndexMap.find(functionType);
		if(existingIt != functionTypeToTypeIndexMap.end()) { return existingIt->second; }
		else
		{
			const uintptr typeIndex = module.types.size();
			module.types.push_back(functionType);
			functionTypeToTypeIndexMap[functionType] = typeIndex;
			return typeIndex;
		}
	}
	
	bool ModuleContext::parseInitializerExpression(SNodeIt& nodeIt,ValueType expectedType,InitializerExpression& outExpression)
	{
		SNodeIt childNodeIt;
		Symbol symbol;
		if(parseTreeNode(nodeIt,childNodeIt) && parseSymbol(childNodeIt,symbol))
		{
			ValueType actualType = ValueType::invalid;
			switch(symbol)
			{
			case Symbol::_i32_const:
			{
				int32 i32Value;
				if(!parseInt(childNodeIt,i32Value)) { recordError(*this,childNodeIt,"const: expected i32 literal"); return false; }
				outExpression = InitializerExpression(i32Value);
				actualType = ValueType::i32;
				break;
			}
			case Symbol::_i64_const:
			{
				int64 i64Value;
				if(!parseInt(childNodeIt,i64Value)) { recordError(*this,childNodeIt,"const: expected i64 literal"); return false; }
				outExpression = InitializerExpression(i64Value);
				actualType = ValueType::i64;
				break;
			}
			case Symbol::_f32_const:
			{
				float32 f32Value;
				if(!parseFloat(childNodeIt,f32Value)) { recordError(*this,childNodeIt,"const: expected f32 literal"); return false; }
				outExpression = InitializerExpression(f32Value);
				actualType = ValueType::f32;
				break;
			}
			case Symbol::_f64_const:
			{
				float64 f64Value;
				if(!parseFloat(childNodeIt,f64Value)) { recordError(*this,childNodeIt,"const: expected f64 literal"); return false; }
				outExpression = InitializerExpression(f64Value);
				actualType = ValueType::f64;
				break;
			}
			case Symbol::_get_global:
			{
				uintptr globalIndex = 0;
				if(!parseNameOrIndex(*this,childNodeIt,globalNameToIndexMap,globalTypes.size(),false,"get_global",globalIndex)) { return false; }
				outExpression = InitializerExpression(InitializerExpression::Type::get_global,globalIndex);
				actualType = globalTypes[globalIndex].valueType;
				break;
			}
			default:
			{
				recordError(*this,nodeIt,"expected initializer expression");
				return false;
			}
			};
			++nodeIt;
			
			if(actualType == expectedType) { return true; }
			else
			{
				recordError(*this,nodeIt,std::string("expected ") + getTypeName(expectedType) + " initializer expression, but got " + getTypeName(actualType));
				outExpression = InitializerExpression();
				return true;
			}
		}
		else { return false; }
	}
	
	// Context that is shared between functions parsing a function.
	struct FunctionContext
	{
		FunctionContext(ModuleContext& inModuleContext,Function& inFunction,const std::vector<std::string>& localNames,const FunctionType* inFunctionType)
		:	moduleContext(inModuleContext)
		,	function(inFunction)
		,	functionType(inFunctionType)
		,	encoder(codeStream)
		{
			// Build a map from local/parameter names to indices, and indices to types.
			buildVariableNameToIndexMapMap(moduleContext,localNames,localNameToIndexMap);
			localTypes.insert(localTypes.begin(),functionType->parameters.begin(),functionType->parameters.end());
			localTypes.insert(localTypes.end(),function.nonParameterLocalTypes.begin(),function.nonParameterLocalTypes.end());

			branchTargets.push_back(BranchTarget(asExpressionType(functionType->ret)));
		}

		void parseTypedExpressionSequence(SNodeIt& nodeIt,const char* errorContext,ExpressionType expectedType)
		{
			if(!nodeIt && expectedType == ExpressionType::none) { return; }

			while(true)
			{
				SNodeIt expressionNodeIt = nodeIt++;
				if(!nodeIt) { parseTypedExpression(expressionNodeIt,errorContext,expectedType); break; }
				else { parseTypedExpression(expressionNodeIt,errorContext,ExpressionType::none); }
			};
		}

		void parseOptionalTypedExpression(SNodeIt& nodeIt,const char* errorContext,ExpressionType expectedType)
		{
			if(expectedType != ExpressionType::none) { parseTypedExpression(nodeIt++,errorContext,expectedType); }
		}

		void parseTypedExpression(SNodeIt nodeIt,const char* errorContext,ExpressionType expectedType)
		{
			const ExpressionType resultType = parseExpression(nodeIt,errorContext);
			coerceResult(expectedType,resultType,nodeIt,errorContext);
		}

		std::vector<uint8>&& getCode()
		{
			// Implicitly insert the end of the function.
			endControlStructure();
			return codeStream.getBytes();
		}

	private:
		ModuleContext& moduleContext;
		Function& function;
		const FunctionType* functionType;
		NameToIndexMap localNameToIndexMap;
		std::vector<ValueType> localTypes;
		NameToBranchTargetMap labelToBranchTargetMap;

		struct BranchTarget
		{
			ExpressionType expectedArgumentType;
			BranchTarget(ExpressionType inExpectedArgumentType): expectedArgumentType(inExpectedArgumentType) {}
		};
		std::vector<BranchTarget> branchTargets;

		ArrayOutputStream codeStream;
		OperationEncoder encoder;

		struct ScopedBranchTarget
		{
			ScopedBranchTarget(FunctionContext& inContext,ExpressionType inExpectedArgumentType,bool inHasName,const char* inName)
			: context(inContext), hasName(inHasName), name(inName), outerNamedBranchTargetIndex(UINTPTR_MAX)
			{
				branchTargetIndex = context.branchTargets.size();
				context.branchTargets.push_back(BranchTarget(inExpectedArgumentType));
				
				if(hasName)
				{
					if(context.labelToBranchTargetMap.count(name)) { outerNamedBranchTargetIndex = context.labelToBranchTargetMap[name]; }
					context.labelToBranchTargetMap[name] = context.branchTargets.size() - 1;
				}
			}
			~ScopedBranchTarget()
			{
				assert(branchTargetIndex == context.branchTargets.size() - 1);
				context.branchTargets.pop_back();

				if(hasName)
				{
					if(outerNamedBranchTargetIndex != UINTPTR_MAX) { context.labelToBranchTargetMap[name] = outerNamedBranchTargetIndex; }
					else { context.labelToBranchTargetMap.erase(name); }
				}
			}
			BranchTarget& getBranchTarget() const { return context.branchTargets[branchTargetIndex]; }
		private:
			FunctionContext& context;
			uintptr branchTargetIndex;
			bool hasName;
			const char* name;
			uintptr outerNamedBranchTargetIndex;
		};

		// Parses an expression of any type.
		ExpressionType parseExpression(SNodeIt parentNodeIt,const char* errorContext);

		void enterUnreachable()
		{
			if(!WebAssembly::unconditionalBranchImpliesEnd) { encoder.end(); }
			if(!encoder.unreachableDepth) { ++encoder.unreachableDepth; }
		}

		void enterControlStructure()
		{
			if(encoder.unreachableDepth) { ++encoder.unreachableDepth; }
		}

		void endControlStructure()
		{
			if(!encoder.unreachableDepth) { encoder.end(); }
			else { --encoder.unreachableDepth; }
		}

		void emitError(SNodeIt nodeIt,const std::string& message)
		{
			recordError(moduleContext,nodeIt,std::string(message));
			encoder.error({message});
			enterUnreachable();
		}

		void prohibitExtraOperands(SNodeIt nodeIt)
		{
			if(nodeIt) { emitError(nodeIt,"unexpected operands"); }
		}
		
		void parseOperands(SNodeIt& nodeIt,const char* errorContext) { }

		template<typename... OperandTypes>
		void parseOperands(SNodeIt& nodeIt,const char* errorContext,ExpressionType operandType,OperandTypes... operandTypes)
		{
			parseTypedExpression(nodeIt++,errorContext,operandType);
			parseOperands(nodeIt,errorContext,operandTypes...);
		}

		// Record a type error.
		void typeError(ExpressionType type,ExpressionType expectedType,SNodeIt nodeIt,const char* errorContext)
		{
			// Ignore type errors in unreachable code.
			if(!encoder.unreachableDepth)
			{
				std::string message =
					std::string("type error: expecting ") + getTypeName(expectedType)
					+ " in " + errorContext
					+ " but found " + getTypeName(type);
				emitError(nodeIt,std::move(message));
			}
		}

		void coerceResult(ExpressionType expectedType,ExpressionType type,SNodeIt nodeIt,const char* errorContext)
		{
			if(type != expectedType && type != ExpressionType::unreachable) { typeError(type,expectedType,nodeIt,errorContext); }
		}

		// Parses an offset attribute.
		uint32 parseOffsetAttribute(SNodeIt& nodeIt)
		{
			SNodeIt valueIt;
			uint64 offset = 0;
			if(parseSymbolAttribute(nodeIt,Symbol::_offset,valueIt))
			{
				if(!parseUnsignedInt(valueIt,offset))
				{
					recordError(moduleContext,nodeIt,"offset: expected unsigned integer");
				}
				if(offset > UINT32_MAX)
				{
					recordError(moduleContext,nodeIt,"offset: too large");
					offset = UINT32_MAX;
				}
				++nodeIt;
			}
			return (uint32)offset;
		}

		// Parses an alignment attribute.
		uint8 parseAlignmentAttribute(SNodeIt& nodeIt,size_t naturalAlignment)
		{
			SNodeIt valueIt;
			uint64 alignment = naturalAlignment;
			if(parseSymbolAttribute(nodeIt,Symbol::_align,valueIt))
			{
				++nodeIt;
				SNodeIt mutableValueIt = valueIt;
				if(!parseUnsignedInt(mutableValueIt,alignment) || !alignment)
				{
					recordError(moduleContext,valueIt,"align: expected positive integer");
					alignment = 1;
				}
				else if(alignment & (alignment-1))
				{
					recordError(moduleContext,valueIt,"align: expected power-of-two");
					alignment = 1;
				}
				else if(alignment > naturalAlignment)
				{
					recordError(moduleContext,valueIt,std::string("align: must not be larger than natural (") + std::to_string(naturalAlignment) + " bytes)");
					alignment = naturalAlignment;
				}
			}
			return (uint8)Platform::floorLogTwo(alignment);
		}

		BranchTarget& getBranchTargetByDepth(uintptr depth)
		{
			return branchTargets[branchTargets.size() - depth - 1];
		}

		bool parseBranchTargetRef(SNodeIt& nodeIt,uintptr& outDepth)
		{
			// Parse the name or index of the target label.
			const char* name;
			if(parseUnsignedInt(nodeIt,outDepth) && (uintptr)outDepth < branchTargets.size()) { return true; }
			else if(parseName(nodeIt,name))
			{
				auto it = labelToBranchTargetMap.find(name);
				if(it != labelToBranchTargetMap.end()) { outDepth = branchTargets.size() - it->second - 1; return true; }
			}
			return false;
		}
	};

	ExpressionType parseControlSignature(SNodeIt& nodeIt)
	{
		ValueType valueType;
		if(!parseType(nodeIt,valueType)) { return ExpressionType::none; }
		else { return asExpressionType(valueType); }
	}

	ExpressionType FunctionContext::parseExpression(SNodeIt parentNodeIt,const char* errorContext)
	{
		ExpressionType resultType = ExpressionType::unreachable;

		SNodeIt nodeIt;
		Symbol tag;
		if(nodeIt && nodeIt->type == SNodeType::Error)
		{
			// If the node is a S-expression error node, translate it to an AST error node.
			recordSExpError(moduleContext,nodeIt);
			encoder.error({nodeIt->string});
		}
		else if(parseTreeNode(parentNodeIt,nodeIt) && parseSymbol(nodeIt,tag))
		{
			switch(tag)
			{
			default:
				emitError(parentNodeIt,"invalid opcode");
				break;

			#define DEFINE_OP(opcode) \
				prohibitExtraOperands(nodeIt); \
				break; \
				case Symbol::_##opcode:

			DEFINE_OP(current_memory)	{ encoder.current_memory(); resultType = ExpressionType::i32; }
			DEFINE_OP(grow_memory)		{ parseOperands(nodeIt,"grow_memory operand",ExpressionType::i32); encoder.grow_memory(); resultType = ExpressionType::i32; }

			#define DEFINE_CONST_OP(type,parseLiteralFunc) \
				DEFINE_OP(type##_const) \
				{ \
					typename ValueTypeInfo<ValueType::type>::Value value; \
					if(!parseLiteralFunc(nodeIt,value)) { emitError(nodeIt,#type ".const: expected " #type " literal"); break; } \
					encoder.type##_const({value}); \
					resultType = ExpressionType::type; \
				}
			DEFINE_CONST_OP(i32,parseInt)
			DEFINE_CONST_OP(i64,parseInt)
			DEFINE_CONST_OP(f32,parseFloat)
			DEFINE_CONST_OP(f64,parseFloat)

			#define DEFINE_LOAD_OP(type,numBytes,opcode) DEFINE_OP(type##_##opcode) \
				{ \
					if(!moduleContext.memoryTypes.size()) { emitError(parentNodeIt,#type "." #opcode ": module does not have default memory"); break; } \
					auto offset = parseOffsetAttribute(nodeIt); \
					auto alignmentLog2 = parseAlignmentAttribute(nodeIt,numBytes); \
					parseOperands(nodeIt,"load address operand",ExpressionType::i32); \
					encoder.type##_##opcode({alignmentLog2,offset}); \
					resultType = ExpressionType::type; \
				}
			#define DEFINE_STORE_OP(type,numBytes,opcode) DEFINE_OP(type##_##opcode) \
				{ \
					if(!moduleContext.memoryTypes.size()) { emitError(parentNodeIt,#type "." #opcode ": module does not have default memory"); break; } \
					auto offset = parseOffsetAttribute(nodeIt); \
					auto alignmentLog2 = parseAlignmentAttribute(nodeIt,numBytes); \
					parseOperands(nodeIt,"store operands",ExpressionType::i32,ExpressionType::type); \
					encoder.type##_##opcode({alignmentLog2,offset}); \
					resultType = ExpressionType::none; \
				}
			#define DEFINE_MEMORY_OP(type,numBytes,loadOpcode,storeOpcode) \
				DEFINE_LOAD_OP(type,numBytes,loadOpcode) DEFINE_STORE_OP(type,numBytes,storeOpcode)
					
			DEFINE_LOAD_OP(i32,1,load8_s)
			DEFINE_LOAD_OP(i32,1,load8_u)
			DEFINE_LOAD_OP(i32,2,load16_s)
			DEFINE_LOAD_OP(i32,2,load16_u)
			DEFINE_LOAD_OP(i64,1,load8_s)
			DEFINE_LOAD_OP(i64,1,load8_u)
			DEFINE_LOAD_OP(i64,2,load16_s)
			DEFINE_LOAD_OP(i64,2,load16_u)
			DEFINE_LOAD_OP(i64,4,load32_s)
			DEFINE_LOAD_OP(i64,4,load32_u)
			DEFINE_STORE_OP(i32,1,store8)
			DEFINE_STORE_OP(i32,2,store16)
			DEFINE_STORE_OP(i64,1,store8)
			DEFINE_STORE_OP(i64,2,store16)
			DEFINE_STORE_OP(i64,4,store32)
			DEFINE_MEMORY_OP(i32,4,load,store)
			DEFINE_MEMORY_OP(i64,8,load,store)
			DEFINE_MEMORY_OP(f32,4,load,store)
			DEFINE_MEMORY_OP(f64,8,load,store)

			#define DEFINE_TYPED_UNARY_OP(type,opcode) DEFINE_OP(type##_##opcode) \
				{ parseOperands(nodeIt,"unary operand"	,ExpressionType::type);					encoder.type##_##opcode(); resultType = ExpressionType::type; }
			#define DEFINE_TYPED_BINARY_OP(type,opcode) DEFINE_OP(type##_##opcode) \
				{ parseOperands(nodeIt,"binary operands"	,ExpressionType::type,ExpressionType::type);	encoder.type##_##opcode(); resultType = ExpressionType::type; }
			#define DEFINE_TYPED_COMPARE_OP(type,opcode) DEFINE_OP(type##_##opcode) \
				{ parseOperands(nodeIt,"compare operands"	,ExpressionType::type,ExpressionType::type);	encoder.type##_##opcode(); resultType = ExpressionType::i32; }

			#define DEFINE_INT_UNARY_OP(opcode) DEFINE_TYPED_UNARY_OP(i32,opcode) DEFINE_TYPED_UNARY_OP(i64,opcode)
			#define DEFINE_FLOAT_UNARY_OP(opcode) DEFINE_TYPED_UNARY_OP(f32,opcode) DEFINE_TYPED_UNARY_OP(f64,opcode)

			#define DEFINE_INT_BINARY_OP(opcode) DEFINE_TYPED_BINARY_OP(i32,opcode) DEFINE_TYPED_BINARY_OP(i64,opcode)
			#define DEFINE_FLOAT_BINARY_OP(opcode) DEFINE_TYPED_BINARY_OP(f32,opcode) DEFINE_TYPED_BINARY_OP(f64,opcode)
				
			#define DEFINE_INT_COMPARE_OP(opcode) DEFINE_TYPED_COMPARE_OP(i32,opcode) DEFINE_TYPED_COMPARE_OP(i64,opcode)
			#define DEFINE_FLOAT_COMPARE_OP(opcode) DEFINE_TYPED_COMPARE_OP(f32,opcode) DEFINE_TYPED_COMPARE_OP(f64,opcode)
				
			DEFINE_INT_UNARY_OP(clz)
			DEFINE_INT_UNARY_OP(ctz)
			DEFINE_INT_UNARY_OP(popcnt)
			DEFINE_INT_BINARY_OP(add)
			DEFINE_INT_BINARY_OP(sub)
			DEFINE_INT_BINARY_OP(mul)
			DEFINE_INT_BINARY_OP(div_s)
			DEFINE_INT_BINARY_OP(div_u)
			DEFINE_INT_BINARY_OP(rem_s)
			DEFINE_INT_BINARY_OP(rem_u)
			DEFINE_INT_BINARY_OP(and)
			DEFINE_INT_BINARY_OP(or)
			DEFINE_INT_BINARY_OP(xor)
			DEFINE_INT_BINARY_OP(shl)
			DEFINE_INT_BINARY_OP(shr_s)
			DEFINE_INT_BINARY_OP(shr_u)
			DEFINE_INT_BINARY_OP(rotl)
			DEFINE_INT_BINARY_OP(rotr)

			DEFINE_FLOAT_UNARY_OP(neg)
			DEFINE_FLOAT_UNARY_OP(abs)
			DEFINE_FLOAT_UNARY_OP(ceil)
			DEFINE_FLOAT_UNARY_OP(floor)
			DEFINE_FLOAT_UNARY_OP(trunc)
			DEFINE_FLOAT_UNARY_OP(nearest)
			DEFINE_FLOAT_BINARY_OP(add)
			DEFINE_FLOAT_BINARY_OP(sub)
			DEFINE_FLOAT_BINARY_OP(mul)
			DEFINE_FLOAT_BINARY_OP(div)
			DEFINE_FLOAT_BINARY_OP(copysign)
			DEFINE_FLOAT_BINARY_OP(min)
			DEFINE_FLOAT_BINARY_OP(max)
			DEFINE_FLOAT_UNARY_OP(sqrt)

			DEFINE_INT_COMPARE_OP(eq) DEFINE_INT_COMPARE_OP(ne)
			DEFINE_INT_COMPARE_OP(lt_s) DEFINE_INT_COMPARE_OP(lt_u)
			DEFINE_INT_COMPARE_OP(le_s) DEFINE_INT_COMPARE_OP(le_u)
			DEFINE_INT_COMPARE_OP(gt_s) DEFINE_INT_COMPARE_OP(gt_u)
			DEFINE_INT_COMPARE_OP(ge_s) DEFINE_INT_COMPARE_OP(ge_u)

			DEFINE_FLOAT_COMPARE_OP(eq) DEFINE_FLOAT_COMPARE_OP(ne)
			DEFINE_FLOAT_COMPARE_OP(lt) DEFINE_FLOAT_COMPARE_OP(le)
			DEFINE_FLOAT_COMPARE_OP(gt) DEFINE_FLOAT_COMPARE_OP(ge)
	
			#define DEFINE_CAST_OP(destType,sourceType,opcode) DEFINE_OP(destType##_##opcode##_##sourceType) \
				{ parseOperands(nodeIt,"cast operand",ExpressionType::sourceType); encoder.destType##_##opcode##_##sourceType(); resultType = ExpressionType::destType; }

			DEFINE_CAST_OP(i32,i64,wrap)
			DEFINE_CAST_OP(i64,i32,extend_s)
			DEFINE_CAST_OP(i64,i32,extend_u)

			DEFINE_CAST_OP(i32,f64,trunc_s) DEFINE_CAST_OP(i32,f32,trunc_s)
			DEFINE_CAST_OP(i64,f64,trunc_s) DEFINE_CAST_OP(i64,f32,trunc_s)

			DEFINE_CAST_OP(i32,f64,trunc_u) DEFINE_CAST_OP(i32,f32,trunc_u)
			DEFINE_CAST_OP(i64,f64,trunc_u) DEFINE_CAST_OP(i64,f32,trunc_u)

			DEFINE_CAST_OP(f32,i32,convert_s) DEFINE_CAST_OP(f32,i64,convert_s)
			DEFINE_CAST_OP(f64,i32,convert_s) DEFINE_CAST_OP(f64,i64,convert_s)

			DEFINE_CAST_OP(f32,i32,convert_u) DEFINE_CAST_OP(f32,i64,convert_u)
			DEFINE_CAST_OP(f64,i32,convert_u) DEFINE_CAST_OP(f64,i64,convert_u)

			DEFINE_CAST_OP(f32,f64,demote)
			DEFINE_CAST_OP(f64,f32,promote)

			DEFINE_CAST_OP(f64,i64,reinterpret)
			DEFINE_CAST_OP(f32,i32,reinterpret)
			DEFINE_CAST_OP(i64,f64,reinterpret)
			DEFINE_CAST_OP(i32,f32,reinterpret)
					
			DEFINE_OP(i32_eqz) { parseOperands(nodeIt,"i32.eqz operand",ExpressionType::i32); encoder.i32_eqz(); resultType = ExpressionType::i32; }
			DEFINE_OP(i64_eqz) { parseOperands(nodeIt,"i64.eqz operand",ExpressionType::i64); encoder.i64_eqz(); resultType = ExpressionType::i32; }
			
			DEFINE_OP(block)
			{
				// Parse an optional label.
				const char* labelName = nullptr;
				bool hasLabel = parseName(nodeIt,labelName);

				// Parse an optional result type for the block.
				resultType = parseControlSignature(nodeIt);

				// Write the block operator.
				encoder.beginBlock({asResultType(resultType)});

				// Parse the block's body.
				ScopedBranchTarget scopedBranchTarget(*this,resultType,hasLabel,labelName);
				enterControlStructure();
				parseTypedExpressionSequence(nodeIt,"block",resultType);
				endControlStructure();
			}
				
			DEFINE_OP(if)
			{
				// Parse a label for the if.
				const char* labelName = nullptr;
				bool hasLabel = parseName(nodeIt,labelName);
				
				// Parse an optional result type for the if.
				resultType = parseControlSignature(nodeIt);
				
				// Parse the condition.
				parseOperands(nodeIt,"if condition",ExpressionType::i32);

				// Wrap the whole if in a branch target.
				ScopedBranchTarget scopedBranchTarget(*this,resultType,hasLabel,labelName);
				
				// Look ahead to see whether there's an else node.
				SNodeIt testElseNodeIt = nodeIt;
				const bool hasElseNode = (++testElseNodeIt);

				// Emit an if or an if_else depending on whether there's an else node.
				if(hasElseNode) { encoder.beginIfElse({asResultType(resultType)}); }
				else
				{
					if(resultType != ExpressionType::none) { emitError(nodeIt,"if without else cannot yield a result"); }
					encoder.beginIf();
				}
				enterControlStructure();

				// Parse the then clause.
				SNodeIt thenNodeIt;
				if(parseTaggedNode(nodeIt,Symbol::_then,thenNodeIt))
					{ parseTypedExpressionSequence(thenNodeIt,"then",resultType); }
				else	{ parseTypedExpression(nodeIt,"then",resultType); }
				++nodeIt;

				// Parse the else clause.
				if(hasElseNode)
				{
					endControlStructure();
					enterControlStructure();

					SNodeIt elseNodeIt;
					if(parseTaggedNode(nodeIt,Symbol::_else,elseNodeIt))
						{ parseTypedExpressionSequence(elseNodeIt,"else",resultType); }
					else	{ parseTypedExpression(nodeIt++,"else",resultType); }
					++nodeIt;
				}

				endControlStructure();
			}
				
			DEFINE_OP(loop)
			{
				// Parse a label names for the continue and break branch targets.
				const char* labelName = nullptr;
				bool hasLabel = parseName(nodeIt,labelName);
				
				// Parse an optional result type for the loop.
				resultType = parseControlSignature(nodeIt);

				ScopedBranchTarget scopedContinueTarget(*this,ExpressionType::none,hasLabel,labelName);

				encoder.beginLoop({asResultType(resultType)});
				enterControlStructure();
				parseTypedExpressionSequence(nodeIt,"loop body",resultType);
				endControlStructure();
			}
				
			DEFINE_OP(br)
			{
				// Parse the branch target.
				uintptr depth;
				if(!parseBranchTargetRef(nodeIt,depth)) { emitError(nodeIt,"br: expected label name or index"); break; }

				// Parse the branch argument.
				parseOptionalTypedExpression(nodeIt,"br target argument",getBranchTargetByDepth(depth).expectedArgumentType);

				encoder.br({depth});
				enterUnreachable();
				resultType = ExpressionType::unreachable;
			}
			DEFINE_OP(br_table)
			{
				// Count the number of branch targets provided.
				size_t numTableTargets = 0;
				uintptr countTargetDepth = 0;
				for(SNodeIt countIt = nodeIt;parseBranchTargetRef(countIt,countTargetDepth);) { ++numTableTargets; }
				if(numTableTargets == 0) { emitError(nodeIt,"br_table: must have at least a default branch target"); break; }

				// Parse the table targets.
				std::vector<uintptr> targetDepths;
				for(uintptr tableIndex = 0;tableIndex < numTableTargets - 1;++tableIndex)
				{
					uintptr targetDepth = 0;
					parseBranchTargetRef(nodeIt,targetDepth);
					targetDepths.push_back(targetDepth);
				}

				// Parse the default branch target.
				uintptr defaultTargetDepth = 0;
				parseBranchTargetRef(nodeIt,defaultTargetDepth);

				// Find the most specific argument type expected between the default targets and cases.
				ExpressionType expectedArgumentType = getBranchTargetByDepth(defaultTargetDepth).expectedArgumentType;
				for(uintptr depth : targetDepths) { expectedArgumentType = intersectTypes(expectedArgumentType,getBranchTargetByDepth(depth).expectedArgumentType); }
				if(expectedArgumentType == ExpressionType::unreachable)
				{
					emitError(parentNodeIt,"br_table: targets must have compatible signatures.");
					break;
				}

				// Parse the branch argument.
				parseOptionalTypedExpression(nodeIt,"br_table target argument",expectedArgumentType);

				// Parse the branch index.
				parseOperands(nodeIt,"br_table index",ExpressionType::i32);

				encoder.br_table({defaultTargetDepth,std::move(targetDepths)});
				enterUnreachable();
				resultType = ExpressionType::unreachable;
			}

			DEFINE_OP(br_if)
			{
				// Parse the branch target.
				uintptr depth;
				if(!parseBranchTargetRef(nodeIt,depth)) { emitError(nodeIt,"br_if: expected label name or index"); break; }
					
				// Parse the branch argument.
				resultType = getBranchTargetByDepth(depth).expectedArgumentType;
				parseOptionalTypedExpression(nodeIt,"br_if target argument",resultType);

				// Parse the branch condition.
				parseOperands(nodeIt,"br_if condition",ExpressionType::i32);

				encoder.br_if({depth});
			}
	
			DEFINE_OP(unreachable)
			{
				encoder.unreachable();
				enterUnreachable();
				resultType = ExpressionType::unreachable;
			}

			DEFINE_OP(return)
			{
				parseOptionalTypedExpression(nodeIt,"return operands",asExpressionType(functionType->ret));
				encoder.ret();
				enterUnreachable();
				resultType = ExpressionType::unreachable;
			}

			DEFINE_OP(call)
			{
				// Parse the function name or index to call.
				uintptr functionIndex = 0;
				if(!parseNameOrIndex(moduleContext,nodeIt,moduleContext.functionNameToIndexMap,moduleContext.functionTypes.size(),false,"call",functionIndex)) { break; }
				const FunctionType* calleeType = moduleContext.module.types[moduleContext.functionTypes[functionIndex]];

				// Parse the call's arguments.
				for(auto parameterType : calleeType->parameters)
				{ parseTypedExpression(nodeIt++,"call arguments",asExpressionType(parameterType)); }

				encoder.call({functionIndex});
				resultType = asExpressionType(calleeType->ret);
			}
			DEFINE_OP(call_indirect)
			{
				// Don't allow call_indirect unless the module has a default table.
				if(!moduleContext.tableTypes.size()) { emitError(parentNodeIt,"call_indirect: module does not have default table"); break; }

				// Parse the function type.
				uintptr signatureIndex = 0;
				if(!parseNameOrIndex(moduleContext,nodeIt,moduleContext.signatureNameToIndexMap,moduleContext.signatures.size(),false,"call_indirect",signatureIndex)) { break; }
				const FunctionType* calleeType = moduleContext.signatures[signatureIndex];
					
				// Parse the call's arguments.
				for(auto parameterType : calleeType->parameters)
				{ parseTypedExpression(nodeIt++,"call_indirect arguments",asExpressionType(parameterType)); }
					
				// Parse the function index.
				parseOperands(nodeIt,"call_indirect index operand",ExpressionType::i32);
					
				encoder.call_indirect({moduleContext.getFunctionTypeIndex(calleeType)});
				resultType = asExpressionType(calleeType->ret);
			}

			DEFINE_OP(nop) { encoder.nop(); resultType = ExpressionType::none; }
				
			DEFINE_OP(select)
			{
				const ExpressionType trueType = parseExpression(nodeIt++,"select true operand");
				const ExpressionType falseType = parseExpression(nodeIt++,"select false operand");

				if(trueType == falseType) { resultType = trueType; }
				else if(trueType == ExpressionType::unreachable) { resultType = falseType; }
				else if(falseType == ExpressionType::unreachable) { resultType = trueType; }
				else { emitError(parentNodeIt,"select non-condition operands must be the same type"); break; }

				parseOperands(nodeIt,"select condition operand",ExpressionType::i32);
				encoder.select();
			}

			DEFINE_OP(drop)
			{
				const ExpressionType dropType = parseExpression(nodeIt++,"drop operand");
				if(dropType == ExpressionType::none) { emitError(parentNodeIt,"drop operand must yield a value"); }
				encoder.drop();
				resultType = ExpressionType::none;
			}
			DEFINE_OP(get_local)
			{
				uintptr localIndex = 0;
				if(!parseNameOrIndex(moduleContext,nodeIt,localNameToIndexMap,localTypes.size(),false,"get_local",localIndex)) { break; }
				auto localType = localTypes[localIndex];

				encoder.get_local({localIndex});
				resultType = asExpressionType(localType);
			}
			DEFINE_OP(set_local)
			{
				uintptr localIndex = 0;
				if(!parseNameOrIndex(moduleContext,nodeIt,localNameToIndexMap,localTypes.size(),false,"set_local",localIndex)) { break; }
				auto localType = localTypes[localIndex];

				parseOperands(nodeIt,"set_local value operand",asExpressionType(localType));

				encoder.set_local({localIndex});
				resultType = ExpressionType::none;
			}
			DEFINE_OP(tee_local)
			{
				uintptr localIndex = 0;
				if(!parseNameOrIndex(moduleContext,nodeIt,localNameToIndexMap,localTypes.size(),false,"tee_local",localIndex)) { break; }
				auto localType = localTypes[localIndex];
					
				parseOperands(nodeIt,"tee_local value operand",asExpressionType(localType));

				encoder.tee_local({localIndex});
				resultType = asExpressionType(localType);
			}

			DEFINE_OP(get_global)
			{
				uintptr globalIndex = 0;
				if(!parseNameOrIndex(moduleContext,nodeIt,moduleContext.globalNameToIndexMap,moduleContext.globalTypes.size(),false,"get_global",globalIndex)) { break; }
				auto globalType = moduleContext.globalTypes[globalIndex];

				encoder.get_global({globalIndex});
				resultType = asExpressionType(globalType.valueType);
			}
			DEFINE_OP(set_global)
			{
				uintptr globalIndex = 0;
				if(!parseNameOrIndex(moduleContext,nodeIt,moduleContext.globalNameToIndexMap,moduleContext.globalTypes.size(),false,"set_global",globalIndex)) { break; }
				auto globalType = moduleContext.globalTypes[globalIndex];
					
				if(!globalType.isMutable) { emitError(parentNodeIt,"set_global: cannot write to immutable global"); break; }
					
				parseOperands(nodeIt,"set_global value operand",asExpressionType(globalType.valueType));

				encoder.set_global({globalIndex});
				resultType = ExpressionType::none;
			}
				
			break;
			}
		}
		else { emitError(parentNodeIt,"expected expression"); }

		return resultType;
	}

	void ModuleContext::parse(SNodeIt firstModuleChildNodeIt)
	{
		// Do a first pass that only parses declarations before parsing definitions.
		std::vector<SNodeIt> exportNodes;
		std::vector<SNodeIt> startNodes;
		std::vector<SNodeIt> funcNodes;
		std::vector<std::tuple<bool,uintptr,SNodeIt>> dataNodes;
		std::vector<std::tuple<bool,uintptr,SNodeIt>> elemNodes;
		for(auto nodeIt = firstModuleChildNodeIt;nodeIt;++nodeIt)
		{
			SNodeIt childNodeIt;
			Symbol tag;
			if(parseTreeNode(nodeIt,childNodeIt) && parseSymbol(childNodeIt,tag))
			{
				switch(tag)
				{
					case Symbol::_func:
					{
						auto functionIndex = functionTypes.size();

						// Parse an optional function name.
						const char* functionName = "";
						const SNodeIt nameIt = childNodeIt;
						if(parseName(childNodeIt,functionName))
						{
							if(functionNameToIndexMap.count(functionName)) { recordError(*this,nameIt,std::string("duplicate function name: ") + functionName); }
							else { functionNameToIndexMap[functionName] = functionIndex; }
						}

						// Parse an inline import or export tag.
						SNodeIt savedImportExportIt = childNodeIt;
						InlineImportExport inlineImportExport;
						if(parseInlineImportOrExportDeclaration(*this,childNodeIt,inlineImportExport)
							&& inlineImportExport.type == InlineImportExport::Type::exported)
						{
							auto exportIt = exportNameToIndexMap.find(inlineImportExport.exportName);
							if(exportIt != exportNameToIndexMap.end()) { recordError(*this,savedImportExportIt,std::string("duplicate export name: ") + inlineImportExport.exportName); continue; }
							exportNameToIndexMap[inlineImportExport.exportName] = module.exports.size();
							module.exports.push_back({inlineImportExport.exportName,ObjectKind::function,functionIndex});
						}

						// Parse a module type reference.
						const FunctionType* referencedFunctionType = nullptr;
						bool hasReferencedFunctionType = false;
						SNodeIt typeChildNodeIt;
						if(parseTaggedNode(childNodeIt,Symbol::_type,typeChildNodeIt))
						{
							// Parse a name or index into the module's type declarations.
							++childNodeIt;
							uintptr signatureIndex = 0;
							if(!parseNameOrIndex(*this,typeChildNodeIt,signatureNameToIndexMap,signatures.size(),false,"func type",signatureIndex)) { continue; }
							referencedFunctionType = signatures[signatureIndex];
							hasReferencedFunctionType = true;
						}

						// Parse the function parameter and result types.
						std::vector<std::string> parameterNames;
						const FunctionType* inlineFunctionType = nullptr;
						parseFunctionType(*this,childNodeIt,inlineFunctionType,parameterNames);

						// Use either the referenced function type or the inline function type.
						uintptr functionTypeIndex;
						if(!hasReferencedFunctionType) { functionTypeIndex = getFunctionTypeIndex(inlineFunctionType); }
						else
						{
							functionTypeIndex = getFunctionTypeIndex(referencedFunctionType);

							if(inlineFunctionType->parameters.size() && getArity(inlineFunctionType->ret) && inlineFunctionType != referencedFunctionType)
							{
								// If there's both a type reference and explicit function signature, then make sure they match.
								recordError(*this,nodeIt,std::string("type reference doesn't match function signature"));
								continue;
							}
						}

						functionTypes.push_back(functionTypeIndex);

						if(inlineImportExport.type == InlineImportExport::Type::imported)
						{
							// Create the function import.
							module.imports.push_back({inlineImportExport.moduleName,inlineImportExport.exportName,functionTypeIndex});

							if(childNodeIt) { recordError(*this,childNodeIt,"unexpected input following imported function declaration"); continue; }
						}
						else
						{
							// Create the function declaration.
							module.functionDefs.push_back(Function());
							funcNodes.push_back(nodeIt);
							Function& function = module.functionDefs.back();
							function.typeIndex = functionTypeIndex;

							module.disassemblyInfo.functions.push_back(FunctionDisassemblyInfo());
							FunctionDisassemblyInfo& functionDisassemblyInfo = module.disassemblyInfo.functions.back();
							functionDisassemblyInfo.localNames = std::move(parameterNames);
							functionDisassemblyInfo.name = functionName;

							std::vector<ValueType> localTypes;
							for(;childNodeIt;++childNodeIt)
							{
								SNodeIt innerChildNodeIt;
								if(parseTaggedNode(childNodeIt,Symbol::_local,innerChildNodeIt))
								{
									// Parse a local declaration.
									parseVariables(*this,innerChildNodeIt,localTypes,functionDisassemblyInfo.localNames);
									if(innerChildNodeIt) { recordError(*this,innerChildNodeIt,"unexpected input following local declaration"); continue; }
								}
								else
								{
									if(parseTaggedNode(childNodeIt,Symbol::_param,innerChildNodeIt))
										{ recordError(*this,childNodeIt,"unexpected param declaration"); }
									if(parseTaggedNode(childNodeIt,Symbol::_result,innerChildNodeIt))
										{ recordError(*this,childNodeIt,"unexpected result declaration"); }
									break;
								} // Stop parsing when we reach the first func child that isn't a param, result, or local.
							}
							function.nonParameterLocalTypes = std::move(localTypes);
						}
						break;
					}
					case Symbol::_import:
					{
						if(module.functionDefs.size() || module.globalDefs.size() || module.memoryDefs.size() || module.tableDefs.size())
						{ recordError(*this,nodeIt,"import declarations must occur before all global, function, memory, and table declarations"); }

						// Parse a mandatory import module name.
						std::string importModuleName;
						if(!parseString(childNodeIt,importModuleName))
						{ recordError(*this,childNodeIt,"expected import module name string"); continue; }

						// Parse a mandatory import function name.
						std::string importExportName;
						if(!parseString(childNodeIt,importExportName))
						{ recordError(*this,childNodeIt,"expected import function name string"); continue; }
						
						// Parse a mandatory import type.
						Symbol kindSymbol;
						SNodeIt importTypeChildNodeIt;
						if(!parseTreeNode(childNodeIt,importTypeChildNodeIt) || !parseSymbol(importTypeChildNodeIt,kindSymbol))
						{ recordError(*this,childNodeIt,"expected import type declaration"); continue; }
						++childNodeIt;

						switch(kindSymbol)
						{
						case Symbol::_func:
						{
							// Parse an optional import name used within the module.
							const char* importInternalName = nullptr;
							bool hasName = parseName(importTypeChildNodeIt,importInternalName);

							const FunctionType* importType = nullptr;
							SNodeIt functionTypeChildNodeIt;
							if(parseTaggedNode(importTypeChildNodeIt,Symbol::_type,functionTypeChildNodeIt))
							{
								// Parse a name or index into the module's type declarations.
								++importTypeChildNodeIt;
								uintptr signatureIndex = 0;
								if(!parseNameOrIndex(*this,functionTypeChildNodeIt,signatureNameToIndexMap,signatures.size(),false,"func type",signatureIndex)) { continue; }
								importType = signatures[signatureIndex];
							}
							else
							{
								// Parse the import's parameter and result declarations.
								std::vector<std::string> parameterNames;
								parseFunctionType(*this,importTypeChildNodeIt,importType,parameterNames);
							}

							// Create the import.
							auto functionTypeIndex = getFunctionTypeIndex(importType);
							module.imports.push_back({std::move(importModuleName),std::move(importExportName),ImportType(functionTypeIndex)});
							if(hasName)
							{
								if(functionNameToIndexMap.count(importInternalName)) { recordError(*this,nodeIt,std::string("duplicate function name: ") + importInternalName); }
								else { functionNameToIndexMap[importInternalName] = functionTypes.size(); }
							}
							functionTypes.push_back(functionTypeIndex);
							break;
						}
						case Symbol::_table:
						{
							const uintptr tableIndex = tableTypes.size();

							// Parse an optional import name used within the module.
							const char* importInternalName = nullptr;
							bool hasName = parseName(importTypeChildNodeIt,importInternalName);
							
							// Parse the table type.
							TableType tableType;
							if(!parseTableType(*this,importTypeChildNodeIt,tableType)) { recordError(*this,importTypeChildNodeIt,"expected table type"); continue; }
							tableTypes.push_back(tableType);
							if(tableTypes.size() > 1) { recordError(*this,nodeIt,"may not have more than one table declaration or import"); }

							module.imports.push_back({std::move(importModuleName),std::move(importExportName),tableType});
							if(hasName)
							{
								if(tableNameToIndexMap.count(importInternalName)) { recordError(*this,nodeIt,std::string("duplicate table name: ") + importInternalName); }
								else { tableNameToIndexMap[importInternalName] = tableIndex; }
							}
							break;
						}
						case Symbol::_memory:
						{
							const uintptr memoryIndex = memoryTypes.size();

							// Parse an optional import name used within the module.
							const char* importInternalName = nullptr;
							bool hasName = parseName(importTypeChildNodeIt,importInternalName);

							// Parse the memory type.
							MemoryType memoryType;
							if(!parseMemoryType(*this,importTypeChildNodeIt,memoryType)) { recordError(*this,importTypeChildNodeIt,"expected memory type"); continue; }
							memoryTypes.push_back(memoryType);
							if(memoryTypes.size() > 1) { recordError(*this,nodeIt,"may not have more than one memory declaration or import"); }

							module.imports.push_back({std::move(importModuleName),std::move(importExportName),memoryType});
							if(hasName)
							{
								if(memoryNameToIndexMap.count(importInternalName)) { recordError(*this,nodeIt,std::string("duplicate memory name: ") + importInternalName); }
								else { memoryNameToIndexMap[importInternalName] = memoryIndex; }
							}
							break;
						}
						case Symbol::_global:
						{
							const uintptr globalIndex = globalTypes.size();

							// Parse an optional import name used within the module.
							const char* importInternalName = nullptr;
							bool hasName = parseName(importTypeChildNodeIt,importInternalName);

							// Parse the type of the global.
							GlobalType globalType;
							if(!parseGlobalType(importTypeChildNodeIt,globalType))
							{ recordError(*this,importTypeChildNodeIt,"expected global type"); continue; }
							globalTypes.push_back(globalType);

							module.imports.push_back({std::move(importModuleName),std::move(importExportName),globalType});
							if(hasName)
							{
								if(globalNameToIndexMap.count(importInternalName)) { recordError(*this,nodeIt,std::string("duplicate global name: ") + importInternalName); }
								else { globalNameToIndexMap[importInternalName] = globalIndex; }
							}
							break;
						}
						default: recordError(*this,childNodeIt,"expected 'function', 'memory', 'table', 'global' or string"); continue;
						};

						if(importTypeChildNodeIt) { recordError(*this,childNodeIt,"unexpected input following import type declaration"); continue; }
						if(childNodeIt) { recordError(*this,childNodeIt,"unexpected input following import declaration"); continue; }
						
						break;
					}
					case Symbol::_memory:
					{
						const uintptr memoryIndex = memoryTypes.size();

						// Parse an optional memory name used within the module.
						const char* memoryInternalName = nullptr;
						if(parseName(childNodeIt,memoryInternalName))
						{
							if(memoryNameToIndexMap.count(memoryInternalName)) { recordError(*this,nodeIt,"duplicate memory name"); continue; }
							else { memoryNameToIndexMap[memoryInternalName] = memoryIndex; }
						}
						
						// Parse an optional import or export declaration.
						SNodeIt savedImportExportIt = childNodeIt;
						InlineImportExport inlineImportExport;
						if(parseInlineImportOrExportDeclaration(*this,childNodeIt,inlineImportExport)
							&& inlineImportExport.type == InlineImportExport::Type::exported)
						{
							auto exportIt = exportNameToIndexMap.find(inlineImportExport.exportName);
							if(exportIt != exportNameToIndexMap.end()) { recordError(*this,savedImportExportIt,std::string("duplicate export name: ") + inlineImportExport.exportName); continue; }
							exportNameToIndexMap[inlineImportExport.exportName] = module.exports.size();
							module.exports.push_back({inlineImportExport.exportName,ObjectKind::memory,memoryIndex});
						}

						// Parse the memory type.
						MemoryType memoryType;
						if(!parseMemoryType(*this,childNodeIt,memoryType)) { recordError(*this,childNodeIt,"expected memory type"); continue; }
						memoryTypes.push_back(memoryType);
						if(memoryTypes.size() > 1) { recordError(*this,nodeIt,"may not have more than one memory declaration or import"); }
						
						// Create a memory import or definition.
						if(inlineImportExport.type == InlineImportExport::Type::imported)
						{ module.imports.push_back({inlineImportExport.moduleName,inlineImportExport.exportName,memoryType}); }
						else
						{
							module.memoryDefs.push_back(memoryType);
							
							// For memory definitions without an explicit size, allow an inline data segment definition.
							if(memoryType.size.min == UINT64_MAX && memoryType.size.max == UINT64_MAX)
							{
								SNodeIt dataChildNodeIt;
								if(parseTaggedNode(childNodeIt,Symbol::_data,dataChildNodeIt))
								{ dataNodes.push_back(std::make_tuple(true,memoryIndex,childNodeIt++)); }
							}
						}

						if(childNodeIt) { recordError(*this,childNodeIt,"unexpected input following memory declaration"); continue; }
						
						break;
					}
					case Symbol::_table:
					{
						const uintptr tableIndex = tableTypes.size();

						// Parse an optional table name used within the module.
						const char* tableInternalName = nullptr;
						if(parseName(childNodeIt,tableInternalName))
						{
							if(tableNameToIndexMap.count(tableInternalName)) { recordError(*this,nodeIt,"duplicate table name"); continue; }
							else { tableNameToIndexMap[tableInternalName] = tableTypes.size(); }
						}
						
						// Parse an optional import or export declaration.
						SNodeIt savedImportExportIt = childNodeIt;
						InlineImportExport inlineImportExport;
						if(parseInlineImportOrExportDeclaration(*this,childNodeIt,inlineImportExport)
							&& inlineImportExport.type == InlineImportExport::Type::exported)
						{
							auto exportIt = exportNameToIndexMap.find(inlineImportExport.exportName);
							if(exportIt != exportNameToIndexMap.end()) { recordError(*this,savedImportExportIt,std::string("duplicate export name: ") + inlineImportExport.exportName); continue; }
							exportNameToIndexMap[inlineImportExport.exportName] = module.exports.size();
							module.exports.push_back({inlineImportExport.exportName,ObjectKind::table,tableIndex});
						}

						// Parse the table type.
						TableType tableType;
						if(!parseTableType(*this,childNodeIt,tableType)) { recordError(*this,childNodeIt,"expected table type"); continue; }
						tableTypes.push_back(tableType);
						if(tableTypes.size() > 1) { recordError(*this,nodeIt,"may not have more than one table declaration or import"); }

						// Create a table import or definition.
						if(inlineImportExport.type == InlineImportExport::Type::imported)
						{ module.imports.push_back({inlineImportExport.moduleName,inlineImportExport.exportName,tableType}); }
						else
						{
							module.tableDefs.push_back(tableType);
						
							// For table definitions without an explicit size, allow an inline elem segment definition.
							if(tableType.size.min == UINT64_MAX && tableType.size.max == UINT64_MAX)
							{
								SNodeIt elemChildNodeIt;
								if(parseTaggedNode(childNodeIt,Symbol::_elem,elemChildNodeIt))
								{ elemNodes.push_back(std::make_tuple(true,tableIndex,childNodeIt++)); }
							}
						}

						if(childNodeIt) { recordError(*this,childNodeIt,"unexpected input following table declaration"); continue; }
						
						break;
					}
					case Symbol::_type:
					{
						// Parse a function type definition.
						const char* typeName = nullptr;
						bool hasName = parseName(childNodeIt,typeName);

						SNodeIt funcChildNodeIt;
						if(!parseTaggedNode(childNodeIt++,Symbol::_func,funcChildNodeIt))
						{ recordError(*this,childNodeIt,"expected func declaration"); continue; }

						const FunctionType* functionType;
						std::vector<std::string> parameterNames;
						parseFunctionType(*this,funcChildNodeIt,functionType,parameterNames);
						if(funcChildNodeIt) { recordError(*this,childNodeIt,"unexpected input following function type declaration"); continue; }

						auto signatureIndex = signatures.size();
						signatures.push_back(functionType);

						if(hasName) { signatureNameToIndexMap[typeName] = signatureIndex; }

						if(childNodeIt) { recordError(*this,childNodeIt,"unexpected input following type declaration"); continue; }
						
						break;
					}
					case Symbol::_global:
					{
						const uintptr globalIndex = globalTypes.size();

						// Parse an optional name for the global.
						const char* globalName = nullptr;
						if(parseName(childNodeIt,globalName))
						{
							if(globalNameToIndexMap.count(globalName)) { recordError(*this,nodeIt,std::string("duplicate global name: ") + globalName); }
							else { globalNameToIndexMap[globalName] = globalIndex; }
						}
						
						// Parse an inline import or export tag.
						SNodeIt savedImportExportIt = childNodeIt;
						InlineImportExport inlineImportExport;
						if(parseInlineImportOrExportDeclaration(*this,childNodeIt,inlineImportExport)
							&& inlineImportExport.type == InlineImportExport::Type::exported)
						{
							auto exportIt = exportNameToIndexMap.find(inlineImportExport.exportName);
							if(exportIt != exportNameToIndexMap.end()) { recordError(*this,savedImportExportIt,std::string("duplicate export name: ") + inlineImportExport.exportName); continue; }
							exportNameToIndexMap[inlineImportExport.exportName] = module.exports.size();
							module.exports.push_back({inlineImportExport.exportName,ObjectKind::global,globalIndex});
						}

						// Parse the global type.
						GlobalType globalType;
						if(!parseGlobalType(childNodeIt,globalType))
						{
							recordError(*this,childNodeIt,"expected global type");
							continue;
						}

						if(inlineImportExport.type == InlineImportExport::Type::imported)
						{
							module.imports.push_back({std::move(inlineImportExport.moduleName),std::move(inlineImportExport.exportName),globalType});
							globalTypes.push_back(globalType);
						}
						else
						{
							// Parse an initializer expression for the global.
							InitializerExpression initializer;
							if(!parseInitializerExpression(childNodeIt,globalType.valueType,initializer))
							{
								recordError(*this,childNodeIt,"expected initializer expression");
								continue;
							}

							// Create the global.
							module.globalDefs.push_back({globalType,initializer});
							globalTypes.push_back(globalType);
						}

						if(childNodeIt) { recordError(*this,childNodeIt,"unexpected input following global declaration"); continue; }

						break;
					}
					case Symbol::_data: dataNodes.push_back(std::make_tuple(false,0,nodeIt)); break;
					case Symbol::_export: exportNodes.push_back(nodeIt); break;
					case Symbol::_start: startNodes.push_back(nodeIt); break;
					case Symbol::_elem: elemNodes.push_back(std::make_tuple(false,0,nodeIt)); break;
					default:
						recordError(*this,nodeIt,"unrecognized declaration");
						break;
				}
			}
			else { recordError(*this,nodeIt,"unrecognized input"); }
		}
		
		// Parse the export declarations after all other declarations are available for use.
		for(auto nodeIt : exportNodes)
		{
			SNodeIt childNodeIt(nodeIt->children->nextSibling);
			
			// Parse the export name.
			std::string exportName;
			SNodeIt savedExportNameIt = childNodeIt;
			if(!parseString(childNodeIt,exportName))
				{ recordError(*this,childNodeIt,"expected export name string"); continue; }
			auto exportIt = exportNameToIndexMap.find(exportName);
			if(exportIt != exportNameToIndexMap.end()) { recordError(*this,savedExportNameIt,std::string("duplicate export name: ") + exportName); continue; }
			exportNameToIndexMap[exportName] = module.exports.size();

			// Parse the kind of the export.
			ObjectKind kind = ObjectKind::invalid;
			const WAST::NameToIndexMap* kindNameToIndexMap;
			size_t maxKindIndex;
			Symbol symbol;
			SNodeIt kindChildNodeIt;
			if(!parseTreeNode(childNodeIt,kindChildNodeIt)) { kindChildNodeIt = childNodeIt; }
			else
			{
				++childNodeIt;
				if(childNodeIt) { recordError(*this,childNodeIt,"unexpected input following export declaration"); }
			}
			
			if(!parseSymbol(kindChildNodeIt,symbol))
			{
				recordError(*this,childNodeIt,"expected export kind");
				continue;
			}

			switch(symbol)
			{
			case Symbol::_func: kind = ObjectKind::function; kindNameToIndexMap = &functionNameToIndexMap; maxKindIndex = functionTypes.size(); break;
			case Symbol::_table: kind = ObjectKind::table; kindNameToIndexMap = &tableNameToIndexMap; maxKindIndex = tableTypes.size(); break;
			case Symbol::_memory: kind = ObjectKind::memory; kindNameToIndexMap = &memoryNameToIndexMap; maxKindIndex = memoryTypes.size(); break;
			case Symbol::_global: kind = ObjectKind::global; kindNameToIndexMap = &globalNameToIndexMap; maxKindIndex = globalTypes.size(); break;
			default:
				recordError(*this,childNodeIt,"invalid export kind");
				continue;
			};

			// Parse a name or index of the appropriate kind for the export.
			uintptr exportedIndex = 0;
			if(!parseNameOrIndex(*this,kindChildNodeIt,*kindNameToIndexMap,maxKindIndex,false,"exported object",exportedIndex)) { continue; }
			module.exports.push_back({exportName,kind,exportedIndex});

			if(kindChildNodeIt) { recordError(*this,kindChildNodeIt,"unexpected input following export declaration"); }
		}

		// Parse the start function declaration after all function declarations are available for use.
		if(startNodes.size() > 1) { recordError(*this,startNodes[1],"only one start declaration is allowed in a module"); }
		else if(startNodes.size() == 1)
		{
			SNodeIt childNodeIt(startNodes[0]->children->nextSibling);
			uintptr functionIndex = 0;
			SNodeIt functionNodeIt = childNodeIt;
			if(parseNameOrIndex(*this,childNodeIt,functionNameToIndexMap,functionTypes.size(),false,"start function",functionIndex))
			{
				if(module.types[functionTypes[functionIndex]] == FunctionType::get()) { module.startFunctionIndex = functionIndex; }
				else { recordError(*this,functionNodeIt,"start function must be of type ()->Void"); }
			}
		}

		// Parse the function bodies after all other declarations are available for use.
		for(uintptr functionDefinitionIndex = 0;functionDefinitionIndex < funcNodes.size();++functionDefinitionIndex)
		{
			SNodeIt childNodeIt(funcNodes[functionDefinitionIndex]->children->nextSibling);
			
			const char* functionName;
			parseName(childNodeIt,functionName);

			// Process nodes until the first node that isn't the function name, a param, local, or result.
			for(;childNodeIt;++childNodeIt)
			{
				SNodeIt childChildNodeIt;
				if(	!parseTaggedNode(childNodeIt,Symbol::_import,childChildNodeIt)
				&&	!parseTaggedNode(childNodeIt,Symbol::_export,childChildNodeIt)
				&&	!parseTaggedNode(childNodeIt,Symbol::_local,childChildNodeIt)
				&&	!parseTaggedNode(childNodeIt,Symbol::_type,childChildNodeIt)
				&&	!parseTaggedNode(childNodeIt,Symbol::_param,childChildNodeIt)
				&&	!parseTaggedNode(childNodeIt,Symbol::_result,childChildNodeIt))
				{ break; }
			};

			// Parse the function's body.
			const FunctionDisassemblyInfo& functionDisassemblyInfo = module.disassemblyInfo.functions[functionDefinitionIndex];
			Function& function = module.functionDefs[functionDefinitionIndex];
			FunctionContext functionContext(*this,function,functionDisassemblyInfo.localNames,module.types[function.typeIndex]);
			functionContext.parseTypedExpressionSequence(childNodeIt,"function body",asExpressionType(module.types[function.typeIndex]->ret));

			// Append the code to the module's code array and reference it from the function.
			std::vector<uint8> functionCode = functionContext.getCode();
			function.code = CodeRef {module.code.size(),functionCode.size()};
			module.code.insert(module.code.end(),functionCode.begin(),functionCode.end());
		}

		// Parse data segments after all imports are available for use in their base address initializer expression.
		for(auto dataNodeTuple : dataNodes)
		{
			const bool isInline = std::get<0>(dataNodeTuple);
			SNodeIt nodeIt = std::get<2>(dataNodeTuple);
			SNodeIt childNodeIt(nodeIt->children->nextSibling);

			uintptr memoryIndex = std::get<1>(dataNodeTuple);
			InitializerExpression baseOffset = InitializerExpression((int32)0);
			if(!isInline)
			{
				// Parse an optional name or index of the memory to put this data in.
				if(!parseNameOrIndex(*this,childNodeIt,memoryNameToIndexMap,memoryTypes.size(),true,"data segment memory object",memoryIndex)) { memoryIndex = 0; }

				// Parse an initializer expression for the base offset of the segment in the memory.
				if(!parseInitializerExpression(childNodeIt,ValueType::i32,baseOffset))
				{ recordError(*this,childNodeIt,"expected initializer expression"); }
			}

			// Parse the segment data from a string.
			std::vector<uint8> dataVector;
			std::string dataString;
			while(parseString(childNodeIt,dataString))
			{
				dataVector.insert(dataVector.end(),(const uint8*)dataString.data(),(const uint8*)dataString.data() + dataString.length());
				dataString.clear();
			};
			
			if(isInline)
			{
				assert(memoryIndex == 0 && module.memoryDefs.size() == 1);
				assert(module.memoryDefs[0].size.min == UINT64_MAX && module.memoryDefs[0].size.max == UINT64_MAX);
				MemoryType& memory = module.memoryDefs[0];
				memory.size.min = memory.size.max = (dataVector.size() + WebAssembly::numBytesPerPage - 1) >> WebAssembly::numBytesPerPageLog2;
			}
			else
			{
				// Validate that there is a memory to place this segment in, and that it's minimum size includes the segment's address range.
				if(!memoryTypes.size()) { recordError(*this,nodeIt,"module does not have a memory to allocate data segment in"); }
				else if(baseOffset.type == InitializerExpression::Type::i32_const
					&& (uint64)baseOffset.i32 + dataVector.size() > (memoryTypes[0].size.min << WebAssembly::numBytesPerPageLog2))
				{ recordError(*this,nodeIt,"data segment address range is outside of allocated memory pages"); }
			
				// Validate that this segment follows previous segment declarations.
				if(module.dataSegments.size()
					&& module.dataSegments.back().baseOffset.type == InitializerExpression::Type::i32_const
					&& baseOffset.type == InitializerExpression::Type::i32_const
					&& (uint32)module.dataSegments.back().baseOffset.i32 + module.dataSegments.back().data.size() > (uint32)baseOffset.i32)
				{ recordError(*this,nodeIt,"data segment must use address range that follows previous segments"); }
			}

			// Create the data segment.
			module.dataSegments.push_back({memoryIndex,baseOffset,std::move(dataVector)});
			
			if(childNodeIt) { recordError(*this,childNodeIt,"unexpected input following data segment declaration"); continue; }
		}

		// Parse elem segments after all imports are available for use in their base address initializer expression, and all function declarations have been parsed.
		for(auto elemNodeTuple : elemNodes)
		{
			const bool isInline = std::get<0>(elemNodeTuple);
			SNodeIt nodeIt = std::get<2>(elemNodeTuple);
			SNodeIt childNodeIt(nodeIt->children->nextSibling);
			
			uintptr tableIndex = std::get<1>(elemNodeTuple);
			InitializerExpression baseOffset = InitializerExpression((int32)0);
			if(!isInline)
			{
				// Parse an optional name or index of the table to put these elements in.
				if(!parseNameOrIndex(*this,childNodeIt,tableNameToIndexMap,tableTypes.size(),true,"elem segment table object",tableIndex)) { tableIndex = 0; }

				// Parse an initializer expression for the base offset of the segment in the table.
				if(!parseInitializerExpression(childNodeIt,ValueType::i32,baseOffset))
				{ recordError(*this,childNodeIt,"expected initializer expression"); }
			}

			// Parse the function indices or names.
			std::vector<uintptr> functionIndices;
			while(childNodeIt)
			{
				uintptr functionIndex = 0;
				SNodeIt elementIt = childNodeIt++;
				if(!parseNameOrIndex(*this,elementIt,functionNameToIndexMap,functionTypes.size(),true,"table element",functionIndex))
				{ functionIndex = 0; }
				functionIndices.push_back(functionIndex);
			};
			
			if(isInline)
			{
				assert(tableIndex == 0 && module.tableDefs.size() == 1);
				assert(module.tableDefs[0].size.min == UINT64_MAX && module.tableDefs[0].size.max == UINT64_MAX);
				TableType& table = module.tableDefs[0];
				table.size.min = table.size.max = functionIndices.size();
			}
			else
			{
				// Validate that there is a table to place this segment in, and that its minimum size includes the segment's index range.
				if(!tableTypes.size()) { recordError(*this,nodeIt,"module does not have a table to allocate this elem segment in"); }
				else if(baseOffset.type == InitializerExpression::Type::i32_const
					&& (uint64)baseOffset.i32 + functionIndices.size() > tableTypes[0].size.min)
				{
					recordError(*this,nodeIt,"elem segment index range is outside of allocated table indices");
				}

				// Validate that this segment follows previous segment declarations.
				if(module.tableSegments.size()
					&& module.tableSegments.back().baseOffset.type == InitializerExpression::Type::i32_const
					&& baseOffset.type == InitializerExpression::Type::i32_const
					&& (uint32)module.tableSegments.back().baseOffset.i32 + module.tableSegments.back().indices.size() > (uint32)baseOffset.i32)
				{ recordError(*this,nodeIt,"elem segment must use index range that follows previous segments"); }
			}

			// Create the table segment.
			module.tableSegments.push_back({tableIndex,baseOffset,std::move(functionIndices)});
		}
		
		// Fix up memory and table definitions with uninitialized sizes to fit any static data or elem segments.
		if(module.memoryDefs.size() == 1 && module.dataSegments.size() == 1)
		{
			auto& memory = module.memoryDefs[0];
			const auto& dataSegment = module.dataSegments[0];
			if(memory.size.min == UINT64_MAX
			&& memory.size.max == UINT64_MAX
			&& dataSegment.baseOffset.type == InitializerExpression::Type::i32_const)
			{
				memory.size.min = memory.size.max = ((uint64)dataSegment.baseOffset.i32 + dataSegment.data.size() + WebAssembly::numBytesPerPage - 1) >> WebAssembly::numBytesPerPageLog2;
			}
		}
		if(module.tableDefs.size() == 1 && module.tableSegments.size() == 1)
		{
			auto& table = module.tableDefs[0];
			const auto& tableSegment = module.tableSegments[0];
			if(table.size.min == UINT64_MAX
			&& table.size.max == UINT64_MAX
			&& tableSegment.baseOffset.type == InitializerExpression::Type::i32_const)
			{
				table.size.min = table.size.max = (uint64)tableSegment.baseOffset.i32 + tableSegment.indices.size();
			}
		}

	}

	// Parses a module from a S-expression.
	bool parseModule(SNodeIt firstNonNameChildNodeIt,WebAssembly::Module& outModule,std::vector<Error>& outErrors)
	{
		try
		{
			Core::Timer parseTimer;
			ModuleContext moduleContext(outModule,outErrors);
			moduleContext.parse(firstNonNameChildNodeIt);
			Log::logTimer("Parsed WAST",parseTimer);
		
			// If there weren't any other errors, validate the module to try to catch any validation errors the parser didn't catch sooner.
			// In general, the parser should try to catch validation errors first though, so it can give more than one error at a time, and
			// with a nice location within the file.
			if(!outErrors.size()) { WebAssembly::validate(outModule); }
			return outErrors.size() == 0;
		}
		catch(WebAssembly::ValidationException exception)
		{
			recordError(outErrors,firstNonNameChildNodeIt->startLocus,"validation error: " + exception.message);
			return false;
		}
		catch(FatalSerializationException exception)
		{
			recordError(outErrors,firstNonNameChildNodeIt->startLocus,"serialization error: " + exception.message);
			return false;
		}
	}

	// Parses a module from a WAST string.
	bool parseModule(const char* string,WebAssembly::Module& outModule,std::vector<Error>& outErrors)
	{
		const SExp::SymbolIndexMap& symbolIndexMap = getWASTSymbolIndexMap();
		
		// Parse S-expressions from the string.
		MemoryArena::ScopedArena scopedArena;
		auto rootNode = SExp::parse(string,scopedArena,symbolIndexMap);

		// Parse the module from the S-expressions.
		SNodeIt rootNodeIt(rootNode);
		SNodeIt moduleChildNodeIt;
		if(rootNodeIt && rootNodeIt->type == SNodeType::Error) { recordError(outErrors,rootNodeIt->startLocus,rootNodeIt->error); }
		else if(parseTaggedNode(rootNodeIt,Symbol::_module,moduleChildNodeIt)) { parseModule(moduleChildNodeIt,outModule,outErrors); }
		else { recordError(outErrors,rootNodeIt->startLocus,"expected module"); }
		++rootNodeIt;
		if(rootNodeIt) { recordError(outErrors,rootNodeIt->startLocus,"unexpected input following module"); }

		return outErrors.size() == 0;
	}
}
