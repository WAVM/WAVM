#include "Core/Core.h"
#include "WAST.h"
#include "Lexer.h"
#include "Parse.h"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Validate.h"

#include <memory>

using namespace WAST;
using namespace IR;

namespace
{
	struct Local {};
	
	// State associated with parsing a function.
	struct FunctionParseState : ParseState
	{
		const ModuleParseState& moduleState;

		std::unique_ptr<NameToIndexMap> localNameToIndexMap;
		NameToIndexMap branchTargetNameToIndexMap;

		uintp branchTargetDepth;

		ArrayOutputStream codeByteStream;
		OperationEncoder operationEncoder;
		CodeValidationProxyStream<OperationEncoder> codeStream;

		FunctionParseState(const ModuleParseState& inModuleState,NameToIndexMap* inLocalNameToIndexMap,const Token* firstBodyToken,const FunctionDef& functionDef)
		: ParseState(inModuleState.string,inModuleState.lineInfo,inModuleState.errors,firstBodyToken)
		, moduleState(inModuleState)
		, localNameToIndexMap(inLocalNameToIndexMap)
		, branchTargetDepth(0)
		, operationEncoder(codeByteStream)
		, codeStream(inModuleState.module,functionDef,operationEncoder)
		{}
	};

	// While in scope, pushes a branch target onto the branch target stack.
	// Also maintains the branchTargetNameToIndexMap 
	struct ScopedBranchTarget
	{
		ScopedBranchTarget(FunctionParseState& inState,Name inName)
		: state(inState), name(inName), previousBranchTargetIndex(UINTPTR_MAX)
		{
			branchTargetIndex = ++state.branchTargetDepth;
			if(name)
			{
				auto previousIt = state.branchTargetNameToIndexMap.find(name);
				if(previousIt != state.branchTargetNameToIndexMap.end())
				{
					// If the name was already bound to a branch target, remember the
					// previously bound branch target.
					previousBranchTargetIndex = previousIt->second;
					previousIt->second = branchTargetIndex;
				}
				else
				{
					state.branchTargetNameToIndexMap.emplace(name,branchTargetIndex);
				}
			}
		}

		~ScopedBranchTarget()
		{
			assert(branchTargetIndex == state.branchTargetDepth);
			--state.branchTargetDepth;
			if(name)
			{
				assert(state.branchTargetNameToIndexMap.count(name) == 1);
				assert(state.branchTargetNameToIndexMap.at(name) == branchTargetIndex);
				if(previousBranchTargetIndex == UINTPTR_MAX)
				{
					state.branchTargetNameToIndexMap.erase(name);
				}
				else
				{
					// If hte name was previously bound to an outer branch target, restore it.
					state.branchTargetNameToIndexMap[name] = previousBranchTargetIndex;
				}
			}
		}

	private:

		FunctionParseState& state;
		Name name;
		uintp branchTargetIndex;
		uintp previousBranchTargetIndex;
	};
}

static bool tryParseAndResolveBranchTargetRef(FunctionParseState& state,uintp& outTargetDepth)
{
	Reference branchTargetRef;
	if(tryParseNameOrIndexRef(state,branchTargetRef))
	{
		switch(branchTargetRef.type)
		{
		case Reference::Type::index: outTargetDepth = branchTargetRef.index; break;
		case Reference::Type::name:
		{
			auto nameToIndexMapIt = state.branchTargetNameToIndexMap.find(branchTargetRef.name);
			if(nameToIndexMapIt == state.branchTargetNameToIndexMap.end())
			{
				parseErrorf(state,branchTargetRef.token,"unknown name");
				outTargetDepth = UINTPTR_MAX;
			}
			else
			{
				outTargetDepth = state.branchTargetDepth - nameToIndexMapIt->second;
			}
			break;
		}
		default: Core::unreachable();
		};
		return true;
	}
	return false;
}

static void parseAndValidateRedundantBranchTargetName(ParseState& state,Name branchTargetName,const char* context,const char* redundantContext)
{
	Name redundantName;
	if(tryParseName(state,redundantName) && branchTargetName != redundantName)
	{
		parseErrorf(state,state.nextToken-1,"%s label doesn't match %s label",redundantContext,context);
	}
}

static void parseImm(FunctionParseState& state,NoImm&,Opcode) {}
static void parseImm(FunctionParseState& state,MemoryImm& outImm,Opcode) {}

static void parseImm(FunctionParseState& state,LiteralImm<int32>& outImm,Opcode) { outImm.value = (int32)parseI32(state); }
static void parseImm(FunctionParseState& state,LiteralImm<int64>& outImm,Opcode) { outImm.value = (int64)parseI64(state); }
static void parseImm(FunctionParseState& state,LiteralImm<float32>& outImm,Opcode) { outImm.value = parseF32(state); }
static void parseImm(FunctionParseState& state,LiteralImm<float64>& outImm,Opcode) { outImm.value = parseF64(state); }

static void parseImm(FunctionParseState& state,BranchImm& outImm,Opcode)
{
	if(!tryParseAndResolveBranchTargetRef(state,outImm.targetDepth))
	{
		parseErrorf(state,state.nextToken,"expected branch target name or index");
		throw RecoverParseException();
	}
}

static void parseImm(FunctionParseState& state,BranchTableImm& outImm,Opcode)
{
	uintp targetDepth = 0;
	while(tryParseAndResolveBranchTargetRef(state,targetDepth))
	{
		outImm.targetDepths.push_back(targetDepth);
	};

	if(!outImm.targetDepths.size())
	{
		parseErrorf(state,state.nextToken,"expected branch target name or index");
		throw RecoverParseException();
	}
	else
	{
		outImm.defaultTargetDepth = outImm.targetDepths.back();
		outImm.targetDepths.pop_back();
	}
}

static void parseImm(FunctionParseState& state,GetOrSetVariableImm& outImm,Opcode opcode)
{
	const bool isGlobal = opcode == Opcode::get_global || opcode == Opcode::set_global;
	outImm.variableIndex = parseAndResolveNameOrIndexRef(
		state,
		isGlobal ? state.moduleState.globalNameToIndexMap : *state.localNameToIndexMap,
		isGlobal ? "global" : "local");
}

static void parseImm(FunctionParseState& state,CallImm& outImm,Opcode)
{
	outImm.functionIndex = parseAndResolveNameOrIndexRef(state,state.moduleState.functionNameToIndexMap,"function");
}

static void parseImm(FunctionParseState& state,CallIndirectImm& outImm,Opcode)
{
	outImm.type.index = parseAndResolveNameOrIndexRef(state,state.moduleState.typeNameToIndexMap,"type");
}

template<size_t naturalAlignmentLog2>
static void parseImm(FunctionParseState& state,LoadOrStoreImm<naturalAlignmentLog2>& outImm,Opcode opcode)
{
	outImm.offset = 0;
	if(state.nextToken->type == t_offset)
	{
		++state.nextToken;
		require(state,t_equals);
		outImm.offset = parseI32(state);
	}

	const uint32 naturalAlignment = 1 << naturalAlignmentLog2;
	uint32 alignment = naturalAlignment;
	if(state.nextToken->type == t_align)
	{
		++state.nextToken;
		require(state,t_equals);
		alignment = parseI32(state);
		if(alignment > naturalAlignment)
		{
			parseErrorf(state,state.nextToken,"alignment must be <= natural alignment");
			alignment = naturalAlignment;
		}
	}

	outImm.alignmentLog2 = Platform::floorLogTwo(alignment);
	if(!alignment || alignment & (alignment - 1))
	{
		parseErrorf(state,state.nextToken,"alignment must be power of 2");
	}
}

static void parseInstrSequence(FunctionParseState& state);
static void parseExpr(FunctionParseState& state);

static void parseBlock(FunctionParseState& state,bool isExpr)
{
	Name branchTargetName;
	ControlStructureImm imm;
	tryParseName(state,branchTargetName);
	tryParseResultType(state,imm.resultType);
				
	ScopedBranchTarget branchTarget(state,branchTargetName);
	state.codeStream.block(imm);
	parseInstrSequence(state);
	state.codeStream.end();

	if(!isExpr)
	{
		require(state,t_end);
		parseAndValidateRedundantBranchTargetName(state,branchTargetName,"block","end");
	}
}

static void parseLoop(FunctionParseState& state,bool isExpr)
{
	Name branchTargetName;
	ControlStructureImm imm;
	tryParseName(state,branchTargetName);
	tryParseResultType(state,imm.resultType);
			
	ScopedBranchTarget branchTarget(state,branchTargetName);
	state.codeStream.loop(imm);
	parseInstrSequence(state);
	state.codeStream.end();
	
	if(!isExpr)
	{
		require(state,t_end);
		parseAndValidateRedundantBranchTargetName(state,branchTargetName,"loop","end");
	}
}

static void parseExprSequence(FunctionParseState& state)
{
	while(state.nextToken->type != t_rightParenthesis)
	{
		parseExpr(state);
	};
}

static void parseExpr(FunctionParseState& state)
{
	parseParenthesized(state,[&]
	{
		const Token* opcodeToken = state.nextToken;
		try
		{
			switch(state.nextToken->type)
			{
			case t_block:
			{
				++state.nextToken;
				parseBlock(state,true);
				break;
			}
			case t_loop:
			{
				++state.nextToken;
				parseLoop(state,true);
				break;
			}
			case t_if_:
			{
				++state.nextToken;

				Name branchTargetName;
				ControlStructureImm imm;
				tryParseName(state,branchTargetName);
				tryParseResultType(state,imm.resultType);

				// Parse an optional condition expression.
				if(state.nextToken[0].type != t_leftParenthesis || state.nextToken[1].type != t_then)
				{
					parseExpr(state);
				}

				ScopedBranchTarget branchTarget(state,branchTargetName);
				state.codeStream.if_(imm);

				// Parse the if clauses.
				if(state.nextToken[0].type == t_leftParenthesis && state.nextToken[1].type == t_then)
				{
					// First syntax: (then <instr>)* (else <instr>*)?
					parseParenthesized(state,[&]
					{
						require(state,t_then);
						parseInstrSequence(state);
					});
					if(state.nextToken->type == t_leftParenthesis)
					{
						parseParenthesized(state,[&]
						{
							require(state,t_else_);
							state.codeStream.else_();
							parseInstrSequence(state);
						});
					}
				}
				else
				{
					// Second syntax option: <expr> <expr>?
					parseExpr(state);
					if(state.nextToken->type != t_rightParenthesis)
					{
						state.codeStream.else_();
						parseExpr(state);
					}
				}
				state.codeStream.end();
				break;
			}
			#define VISIT_OPCODE_TOKEN(name,Imm) \
				case t_##name: \
				{ \
					++state.nextToken; \
					Imm imm; \
					parseImm(state,imm,Opcode::name); \
					parseExprSequence(state); \
					state.codeStream.name(imm); \
					break; \
				}
			#define VISIT_OP(opcode,name,nameString,Imm,...) VISIT_OPCODE_TOKEN(name,Imm)
			ENUM_NONCONTROL_OPERATORS(VISIT_OP)
			#undef VISIT_OP
			#undef VISIT_OPCODE_TOKEN
			default:
				parseErrorf(state,state.nextToken,"expected opcode");
				throw RecoverParseException();
			}
		}
		catch(ValidationException exception)
		{
			parseErrorf(state,opcodeToken,"%s",exception.message.c_str());
			throw RecoverParseException();
		}
	});
}

static void parseInstrSequence(FunctionParseState& state)
{
	while(true)
	{
		const Token* opcodeToken = state.nextToken;
		try
		{
			switch(state.nextToken->type)
			{
			case t_leftParenthesis: parseExpr(state); break;
			case t_rightParenthesis: return;
			case t_else_: return;
			case t_end: return;
			case t_block:
			{
				++state.nextToken;
				parseBlock(state,false);
				break;
			}
			case t_loop:
			{
				++state.nextToken;
				parseLoop(state,false);
				break;
			}
			case t_if_:
			{
				++state.nextToken;

				Name branchTargetName;
				ControlStructureImm imm;
				tryParseName(state,branchTargetName);
				tryParseResultType(state,imm.resultType);
				
				ScopedBranchTarget branchTarget(state,branchTargetName);
				state.codeStream.if_(imm);

				// Parse the then clause.
				parseInstrSequence(state);

				// Parse the else clause.
				if(state.nextToken->type == t_else_)
				{
					++state.nextToken;
					parseAndValidateRedundantBranchTargetName(state,branchTargetName,"if","else");

					state.codeStream.else_();
					parseInstrSequence(state);
				}
				state.codeStream.end();
		
				require(state,t_end);
				parseAndValidateRedundantBranchTargetName(state,branchTargetName,"if","end");

				break;
			}
			#define VISIT_OPCODE_TOKEN(name,Imm) \
				case t_##name: \
				{ \
					++state.nextToken; \
					Imm imm; \
					parseImm(state,imm,Opcode::name); \
					state.codeStream.name(imm); \
					break; \
				}
			#define VISIT_OP(opcode,name,nameString,Imm,...) VISIT_OPCODE_TOKEN(name,Imm)
			ENUM_NONCONTROL_OPERATORS(VISIT_OP)
			#undef VISIT_OP
			#undef VISIT_OPCODE_TOKEN
			default:
				parseErrorf(state,state.nextToken,"expected opcode");
				throw RecoverParseException();
			}
		}
		catch(ValidationException exception)
		{
			parseErrorf(state,opcodeToken,"%s",exception.message.c_str());
			throw RecoverParseException();
		}
	};
}

namespace WAST
{
	FunctionDef parseFunctionDef(ModuleParseState& state)
	{
		std::vector<std::string>* localDisassemblyNames = new std::vector<std::string>;
		NameToIndexMap* localNameToIndexMap = new NameToIndexMap();

		// Parse an optional function type reference.
		const Token* typeReferenceToken = state.nextToken;
		IndexedFunctionType referencedFunctionType = {UINTPTR_MAX};
		if(state.nextToken[0].type == t_leftParenthesis
		&& state.nextToken[1].type == t_type)
		{
			referencedFunctionType = parseFunctionTypeRef(state);
		}

		// Parse the explicit function parameters and result type.
		const FunctionType* directFunctionType = parseFunctionType(state,*localNameToIndexMap,*localDisassemblyNames);
		const bool hasNoDirectType = directFunctionType == FunctionType::get();

		// Validate that if the function definition has both a type reference, and explicit parameter/result type declarations, that they match.
		IndexedFunctionType functionType;
		if(referencedFunctionType.index != UINTPTR_MAX && hasNoDirectType)
		{
			functionType = referencedFunctionType;
		}
		else
		{
			functionType = getUniqueFunctionTypeIndex(state,directFunctionType);
			if(referencedFunctionType.index != UINTPTR_MAX && state.module.types[referencedFunctionType.index] != directFunctionType)
			{
				parseErrorf(state,typeReferenceToken,"referenced function type (%s) does not match declared parameters and results (%s)",
					asString(state.module.types[referencedFunctionType.index]).c_str(),
					asString(directFunctionType).c_str()
					);
			}
		}

		// Parse the function's local variables.
		std::vector<ValueType> nonParameterLocalTypes;
		while(tryParseParenthesizedTagged(state,t_local,[&]
		{
			Name localName;
			if(tryParseName(state,localName))
			{
				bindName(state,*localNameToIndexMap,localName,directFunctionType->parameters.size() + nonParameterLocalTypes.size());
				localDisassemblyNames->push_back(localName.getString());
				nonParameterLocalTypes.push_back(parseValueType(state));
			}
			else
			{
				while(state.nextToken->type != t_rightParenthesis)
				{
					localDisassemblyNames->push_back(std::string());
					nonParameterLocalTypes.push_back(parseValueType(state));
				};
			}
		}));

		// Defer parsing the body of the function until after all declarations have been parsed.
		const uintp functionIndex = state.module.functions.size();
		const uintp functionDefIndex = state.module.functions.defs.size();
		const Token* firstBodyToken = state.nextToken;
		state.postDeclarationCallbacks.push_back([functionIndex,functionDefIndex,firstBodyToken,localNameToIndexMap,localDisassemblyNames](ModuleParseState& state)
		{
			FunctionParseState functionState(state,localNameToIndexMap,firstBodyToken,state.module.functions.defs[functionDefIndex]);
			try
			{
				parseInstrSequence(functionState);
				if(!functionState.errors.size())
				{
					functionState.codeStream.end();
					functionState.codeStream.finishValidation();
				}
			}
			catch(ValidationException exception)
			{
				parseErrorf(state,firstBodyToken,"%s",exception.message.c_str());
			}
			catch(RecoverParseException) {}
			catch(FatalParseException) {}

			std::vector<uint8>&& functionCodeBytes = functionState.codeByteStream.getBytes();
			state.module.functions.defs[functionDefIndex].code = CodeRef {state.module.code.size(),functionCodeBytes.size()};
			state.module.code.insert(state.module.code.end(),functionCodeBytes.begin(),functionCodeBytes.end());
			state.disassemblyNames.functions[functionIndex].locals = std::move(*localDisassemblyNames);
			delete localDisassemblyNames;
		});

		// Continue parsing after the closing parenthesis.
		findClosingParenthesis(state);
		--state.nextToken;
	
		return {functionType,std::move(nonParameterLocalTypes),CodeRef()};
	}
}