#include <inttypes.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Lexer.h"
#include "Parse.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Platform/Intrinsic.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::WAST;

namespace WAVM { namespace WAST {
	// State associated with parsing a function.
	struct FunctionState
	{
		FunctionDef& functionDef;

		std::shared_ptr<NameToIndexMap> localNameToIndexMap;
		Uptr numLocals;

		NameToIndexMap branchTargetNameToIndexMap;
		Uptr branchTargetDepth;
		std::vector<std::string> labelDisassemblyNames;

		Serialization::ArrayOutputStream codeByteStream;
		OperatorEncoderStream operationEncoder;
		CodeValidationProxyStream<OperatorEncoderStream> validatingCodeStream;

		FunctionState(const std::shared_ptr<NameToIndexMap>& inLocalNameToIndexMap,
					  FunctionDef& inFunctionDef,
					  ModuleState* moduleState)
		: functionDef(inFunctionDef)
		, localNameToIndexMap(inLocalNameToIndexMap)
		, numLocals(inFunctionDef.nonParameterLocalTypes.size()
					+ moduleState->module.types[inFunctionDef.type.index].params().size())
		, branchTargetDepth(0)
		, operationEncoder(codeByteStream)
		, validatingCodeStream(moduleState->module, functionDef, operationEncoder)
		{
		}
	};
}}

namespace {
	// While in scope, pushes a branch target onto the branch target stack.
	// Also maintains the branchTargetNameToIndexMap
	struct ScopedBranchTarget
	{
		ScopedBranchTarget(FunctionState* inFunctionState, Name inName)
		: functionState(inFunctionState), name(inName), previousBranchTargetIndex(UINTPTR_MAX)
		{
			branchTargetIndex = ++functionState->branchTargetDepth;
			if(name)
			{
				Uptr& mapValueRef
					= functionState->branchTargetNameToIndexMap.getOrAdd(name, UINTPTR_MAX);
				if(mapValueRef != UINTPTR_MAX)
				{
					// If the name was already bound to a branch target, remember the previously
					// bound branch target.
					previousBranchTargetIndex = mapValueRef;
					mapValueRef = branchTargetIndex;
				}
				else
				{
					mapValueRef = branchTargetIndex;
				}
			}
		}

		~ScopedBranchTarget()
		{
			wavmAssert(branchTargetIndex == functionState->branchTargetDepth);
			--functionState->branchTargetDepth;
			if(name)
			{
				wavmAssert(functionState->branchTargetNameToIndexMap.contains(name));
				wavmAssert(functionState->branchTargetNameToIndexMap[name] == branchTargetIndex);
				if(previousBranchTargetIndex == UINTPTR_MAX)
				{ errorUnless(functionState->branchTargetNameToIndexMap.remove(name)); }
				else
				{
					functionState->branchTargetNameToIndexMap.set(name, previousBranchTargetIndex);
				}
			}
		}

	private:
		FunctionState* functionState;
		Name name;
		Uptr branchTargetIndex;
		Uptr previousBranchTargetIndex;
	};
}

static bool tryParseAndResolveBranchTargetRef(CursorState* cursor, Uptr& outTargetDepth)
{
	Reference branchTargetRef;
	if(tryParseNameOrIndexRef(cursor, branchTargetRef))
	{
		switch(branchTargetRef.type)
		{
		case Reference::Type::index: outTargetDepth = branchTargetRef.index; break;
		case Reference::Type::name:
		{
			const HashMapPair<Name, Uptr>* nameIndexPair
				= cursor->functionState->branchTargetNameToIndexMap.getPair(branchTargetRef.name);
			if(!nameIndexPair)
			{
				parseErrorf(cursor->parseState, branchTargetRef.token, "unknown name");
				outTargetDepth = UINTPTR_MAX;
			}
			else
			{
				outTargetDepth = cursor->functionState->branchTargetDepth - nameIndexPair->value;
			}
			break;
		}
		default: Errors::unreachable();
		};
		return true;
	}
	return false;
}

static void parseAndValidateRedundantBranchTargetName(CursorState* cursor,
													  Name branchTargetName,
													  const char* context,
													  const char* redundantContext)
{
	Name redundantName;
	if(tryParseName(cursor, redundantName) && branchTargetName != redundantName)
	{
		parseErrorf(cursor->parseState,
					cursor->nextToken - 1,
					"%s label doesn't match %s label",
					redundantContext,
					context);
	}
}

static void parseImm(CursorState* cursor, NoImm&) {}
static void parseImm(CursorState* cursor, MemoryImm& outImm) { outImm.memoryIndex = 0; }
static void parseImm(CursorState* cursor, MemoryCopyImm& outImm)
{
	outImm.sourceMemoryIndex = 0;
	outImm.destMemoryIndex = 0;
}
static void parseImm(CursorState* cursor, TableImm& outImm)
{
	if(!tryParseAndResolveNameOrIndexRef(cursor,
										 cursor->moduleState->tableNameToIndexMap,
										 cursor->moduleState->module.tables.size(),
										 "table",
										 outImm.tableIndex))
	{ outImm.tableIndex = 0; }
}
static void parseImm(CursorState* cursor, TableCopyImm& outImm)
{
	if(!tryParseAndResolveNameOrIndexRef(cursor,
										 cursor->moduleState->tableNameToIndexMap,
										 cursor->moduleState->module.tables.size(),
										 "table",
										 outImm.sourceTableIndex))
	{ outImm.sourceTableIndex = 0; }

	if(!tryParseAndResolveNameOrIndexRef(cursor,
										 cursor->moduleState->tableNameToIndexMap,
										 cursor->moduleState->module.tables.size(),
										 "table",
										 outImm.destTableIndex))
	{ outImm.destTableIndex = outImm.sourceTableIndex; }
}

static void parseImm(CursorState* cursor, LiteralImm<I32>& outImm)
{
	outImm.value = (I32)parseI32(cursor);
}
static void parseImm(CursorState* cursor, LiteralImm<I64>& outImm)
{
	outImm.value = (I64)parseI64(cursor);
}
static void parseImm(CursorState* cursor, LiteralImm<F32>& outImm)
{
	outImm.value = parseF32(cursor);
}
static void parseImm(CursorState* cursor, LiteralImm<F64>& outImm)
{
	outImm.value = parseF64(cursor);
}

static void parseImm(CursorState* cursor, BranchImm& outImm)
{
	if(!tryParseAndResolveBranchTargetRef(cursor, outImm.targetDepth))
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected branch target name or index");
		throw RecoverParseException();
	}
}

static void parseImm(CursorState* cursor, BranchTableImm& outImm)
{
	std::vector<Uptr> targetDepths;
	Uptr targetDepth = 0;
	while(tryParseAndResolveBranchTargetRef(cursor, targetDepth))
	{ targetDepths.push_back(targetDepth); };

	if(!targetDepths.size())
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected branch target name or index");
		throw RecoverParseException();
	}
	else
	{
		outImm.defaultTargetDepth = targetDepths.back();
		targetDepths.pop_back();
		outImm.branchTableIndex = cursor->functionState->functionDef.branchTables.size();
		cursor->functionState->functionDef.branchTables.push_back(std::move(targetDepths));
	}
}

template<bool isGlobal>
static void parseImm(CursorState* cursor, GetOrSetVariableImm<isGlobal>& outImm)
{
	outImm.variableIndex = parseAndResolveNameOrIndexRef(
		cursor,
		isGlobal ? cursor->moduleState->globalNameToIndexMap
				 : *cursor->functionState->localNameToIndexMap,
		isGlobal ? cursor->moduleState->module.globals.size() : cursor->functionState->numLocals,
		isGlobal ? "global" : "local");
}

static void parseImm(CursorState* cursor, FunctionImm& outImm)
{
	outImm.functionIndex
		= parseAndResolveNameOrIndexRef(cursor,
										cursor->moduleState->functionNameToIndexMap,
										cursor->moduleState->module.functions.size(),
										"function");
}

static void parseImm(CursorState* cursor, CallIndirectImm& outImm)
{
	if(cursor->nextToken->type == t_name || cursor->nextToken->type == t_quotedName
	   || cursor->nextToken->type == t_decimalInt || cursor->nextToken->type == t_hexInt)
	{
		// Parse a table name or index.
		outImm.tableIndex = parseAndResolveNameOrIndexRef(cursor,
														  cursor->moduleState->tableNameToIndexMap,
														  cursor->moduleState->module.tables.size(),
														  "table");
	}
	else
	{
		outImm.tableIndex = 0;
	}

	// Parse the callee type, as a reference or explicit declaration.
	const Token* firstTypeToken = cursor->nextToken;
	std::vector<std::string> paramDisassemblyNames;
	NameToIndexMap paramNameToIndexMap;
	const UnresolvedFunctionType unresolvedFunctionType
		= parseFunctionTypeRefAndOrDecl(cursor, paramNameToIndexMap, paramDisassemblyNames);
	outImm.type.index = resolveFunctionType(cursor->moduleState, unresolvedFunctionType).index;

	// Disallow named parameters.
	if(paramNameToIndexMap.size())
	{
		auto paramNameIt = paramNameToIndexMap.begin();
		parseErrorf(cursor->parseState,
					firstTypeToken,
					"call_indirect callee type declaration may not declare parameter names ($%s)",
					paramNameIt->key.getString().c_str());
	}
}

template<Uptr naturalAlignmentLog2>
static void parseImm(CursorState* cursor, LoadOrStoreImm<naturalAlignmentLog2>& outImm)
{
	outImm.offset = 0;
	if(cursor->nextToken->type == t_offset)
	{
		++cursor->nextToken;
		require(cursor, t_equals);
		outImm.offset = parseI32(cursor);
	}

	const U32 naturalAlignment = 1 << naturalAlignmentLog2;
	U32 alignment = naturalAlignment;
	if(cursor->nextToken->type == t_align)
	{
		++cursor->nextToken;
		require(cursor, t_equals);
		alignment = parseI32(cursor);
		if(alignment > naturalAlignment)
		{
			parseErrorf(
				cursor->parseState, cursor->nextToken, "alignment must be <= natural alignment");
			alignment = naturalAlignment;
		}
	}

	outImm.alignmentLog2 = (U8)Platform::floorLogTwo(alignment);
	if(!alignment || alignment & (alignment - 1))
	{ parseErrorf(cursor->parseState, cursor->nextToken, "alignment must be power of 2"); }
}

static void parseImm(CursorState* cursor, LiteralImm<V128>& outImm)
{
	outImm.value = parseV128(cursor);
}

template<Uptr numLanes> static void parseImm(CursorState* cursor, LaneIndexImm<numLanes>& outImm)
{
	const U8 laneIndex = parseI8(cursor);
	if(laneIndex > numLanes)
	{
		parseErrorf(cursor->parseState,
					cursor->nextToken - 1,
					"lane index must be in the range 0..%" PRIuPTR,
					numLanes);
	}
	outImm.laneIndex = laneIndex;
}

template<Uptr numLanes> static void parseImm(CursorState* cursor, ShuffleImm<numLanes>& outImm)
{
	for(Uptr destLaneIndex = 0; destLaneIndex < numLanes; ++destLaneIndex)
	{
		const U8 sourceLaneIndex = parseI8(cursor);
		if(sourceLaneIndex >= numLanes * 2)
		{
			parseErrorf(cursor->parseState,
						cursor->nextToken - 1,
						"lane index must be in the range 0..%" PRIuPTR,
						numLanes * 2);
		}
		outImm.laneIndices[destLaneIndex] = sourceLaneIndex;
	}
}

template<Uptr naturalAlignmentLog2>
static void parseImm(CursorState* cursor, AtomicLoadOrStoreImm<naturalAlignmentLog2>& outImm)
{
	LoadOrStoreImm<naturalAlignmentLog2> loadOrStoreImm;
	parseImm(cursor, loadOrStoreImm);
	outImm.alignmentLog2 = loadOrStoreImm.alignmentLog2;
	outImm.offset = loadOrStoreImm.offset;
}

static void parseImm(CursorState* cursor, ExceptionTypeImm& outImm)
{
	outImm.exceptionTypeIndex
		= parseAndResolveNameOrIndexRef(cursor,
										cursor->moduleState->exceptionTypeNameToIndexMap,
										cursor->moduleState->module.exceptionTypes.size(),
										"exception type");
}
static void parseImm(CursorState* cursor, RethrowImm& outImm)
{
	if(!tryParseAndResolveBranchTargetRef(cursor, outImm.catchDepth))
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected try label or index");
		throw RecoverParseException();
	}
}

static void parseImm(CursorState* cursor, DataSegmentAndMemImm& outImm)
{
	outImm.dataSegmentIndex
		= parseAndResolveNameOrIndexRef(cursor,
										cursor->moduleState->dataNameToIndexMap,
										cursor->moduleState->module.dataSegments.size(),
										"data");
	if(!tryParseAndResolveNameOrIndexRef(cursor,
										 cursor->moduleState->memoryNameToIndexMap,
										 cursor->moduleState->module.memories.size(),
										 "memory",
										 outImm.memoryIndex))
	{ outImm.memoryIndex = 0; }
}

static void parseImm(CursorState* cursor, DataSegmentImm& outImm)
{
	outImm.dataSegmentIndex
		= parseAndResolveNameOrIndexRef(cursor,
										cursor->moduleState->dataNameToIndexMap,
										cursor->moduleState->module.dataSegments.size(),
										"data");
}

static void parseImm(CursorState* cursor, ElemSegmentAndTableImm& outImm)
{
	outImm.elemSegmentIndex
		= parseAndResolveNameOrIndexRef(cursor,
										cursor->moduleState->elemNameToIndexMap,
										cursor->moduleState->module.elemSegments.size(),
										"elem");
	if(!tryParseAndResolveNameOrIndexRef(cursor,
										 cursor->moduleState->tableNameToIndexMap,
										 cursor->moduleState->module.tables.size(),
										 "table",
										 outImm.tableIndex))
	{ outImm.tableIndex = 0; }
}

static void parseImm(CursorState* cursor, ElemSegmentImm& outImm)
{
	outImm.elemSegmentIndex
		= parseAndResolveNameOrIndexRef(cursor,
										cursor->moduleState->elemNameToIndexMap,
										cursor->moduleState->module.elemSegments.size(),
										"elem");
}

static void parseInstrSequence(CursorState* cursor);
static void parseExpr(CursorState* cursor);

static void parseControlImm(CursorState* cursor,
							Name& outBranchTargetName,
							ControlStructureImm& imm)
{
	tryParseName(cursor, outBranchTargetName);
	cursor->functionState->labelDisassemblyNames.push_back(outBranchTargetName.getString());

	FunctionType functionType;

	// For backward compatibility, handle a naked result type.
	ValueType singleResultType;
	if(tryParseValueType(cursor, singleResultType))
	{ functionType = FunctionType(TypeTuple(singleResultType)); }
	else
	{
		// Parse the callee type, as a reference or explicit declaration.
		const Token* firstTypeToken = cursor->nextToken;
		std::vector<std::string> paramDisassemblyNames;
		NameToIndexMap paramNameToIndexMap;
		const UnresolvedFunctionType unresolvedFunctionType
			= parseFunctionTypeRefAndOrDecl(cursor, paramNameToIndexMap, paramDisassemblyNames);

		// Disallow named parameters.
		if(paramNameToIndexMap.size())
		{
			auto paramNameIt = paramNameToIndexMap.begin();
			parseErrorf(cursor->parseState,
						firstTypeToken,
						"block type declaration may not declare parameter names ($%s)",
						paramNameIt->key.getString().c_str());
		}

		if(!unresolvedFunctionType.reference)
		{
			// If there wasn't a type reference, just use the inline declared params and results.
			functionType = unresolvedFunctionType.explicitType;
		}
		else
		{
			// If there was a type reference, resolve it. This also verifies that if there were also
			// params and/or results declared inline that they match the resolved type reference.
			const Uptr referencedFunctionTypeIndex
				= resolveFunctionType(cursor->moduleState, unresolvedFunctionType).index;
			if(referencedFunctionTypeIndex != UINTPTR_MAX)
			{
				wavmAssert(referencedFunctionTypeIndex < cursor->moduleState->module.types.size());
				functionType = cursor->moduleState->module.types[referencedFunctionTypeIndex];
			}
		}
	}

	// Translate the function type into an indexed block type.
	if(functionType.params().size() == 0 && functionType.results().size() == 0)
	{
		imm.type.format = IndexedBlockType::noParametersOrResult;
		imm.type.resultType = ValueType::any;
	}
	else if(functionType.params().size() == 0 && functionType.results().size() == 1)
	{
		imm.type.format = IndexedBlockType::oneResult;
		imm.type.resultType = functionType.results()[0];
	}
	else
	{
		imm.type.format = IndexedBlockType::functionType;
		imm.type.index = getUniqueFunctionTypeIndex(cursor->moduleState, functionType).index;
	}
}

static void parseBlock(CursorState* cursor, bool isExpr)
{
	Name branchTargetName;
	ControlStructureImm imm;
	parseControlImm(cursor, branchTargetName, imm);

	ScopedBranchTarget branchTarget(cursor->functionState, branchTargetName);
	cursor->functionState->validatingCodeStream.block(imm);
	parseInstrSequence(cursor);
	cursor->functionState->validatingCodeStream.end();

	if(!isExpr)
	{
		require(cursor, t_end);
		parseAndValidateRedundantBranchTargetName(cursor, branchTargetName, "block", "end");
	}
}

static void parseLoop(CursorState* cursor, bool isExpr)
{
	Name branchTargetName;
	ControlStructureImm imm;
	parseControlImm(cursor, branchTargetName, imm);

	ScopedBranchTarget branchTarget(cursor->functionState, branchTargetName);
	cursor->functionState->validatingCodeStream.loop(imm);
	parseInstrSequence(cursor);
	cursor->functionState->validatingCodeStream.end();

	if(!isExpr)
	{
		require(cursor, t_end);
		parseAndValidateRedundantBranchTargetName(cursor, branchTargetName, "loop", "end");
	}
}

static void parseExprSequence(CursorState* cursor)
{
	while(cursor->nextToken->type != t_rightParenthesis) { parseExpr(cursor); };
}

#define VISIT_OP(opcode, name, nameString, Imm, ...)                                               \
	static void parseOp_##name(CursorState* cursor, bool isExpression)                             \
	{                                                                                              \
		++cursor->nextToken;                                                                       \
		Imm imm;                                                                                   \
		parseImm(cursor, imm);                                                                     \
		if(isExpression) { parseExprSequence(cursor); }                                            \
		cursor->functionState->validatingCodeStream.name(imm);                                     \
	}
ENUM_NONCONTROL_OPERATORS(VISIT_OP)
#undef VISIT_OP

static void tryEmitElse(const Token* errorToken, CursorState* cursor)
{
	try
	{
		cursor->functionState->validatingCodeStream.else_();
	}
	catch(ValidationException const& exception)
	{
		parseErrorf(cursor->parseState, errorToken, "%s", exception.message.c_str());
		cursor->functionState->validatingCodeStream.unreachable();
		cursor->functionState->validatingCodeStream.else_();
		cursor->functionState->validatingCodeStream.unreachable();
		cursor->functionState->validatingCodeStream.end();
		throw RecoverParseException();
	}
}

static void parseExpr(CursorState* cursor)
{
	parseParenthesized(cursor, [&] {
		const Token* opcodeToken = cursor->nextToken;
		try
		{
			switch(cursor->nextToken->type)
			{
			case t_block:
			{
				++cursor->nextToken;
				parseBlock(cursor, true);
				break;
			}
			case t_loop:
			{
				++cursor->nextToken;
				parseLoop(cursor, true);
				break;
			}
			case t_if_:
			{
				++cursor->nextToken;

				Name branchTargetName;
				ControlStructureImm imm;
				parseControlImm(cursor, branchTargetName, imm);

				// Parse an optional condition expression.
				if(cursor->nextToken[0].type != t_leftParenthesis
				   || cursor->nextToken[1].type != t_then)
				{ parseExpr(cursor); }

				ScopedBranchTarget branchTarget(cursor->functionState, branchTargetName);
				cursor->functionState->validatingCodeStream.if_(imm);

				// Parse the if clauses.
				if(cursor->nextToken[0].type == t_leftParenthesis
				   && cursor->nextToken[1].type == t_then)
				{
					// First syntax: (then <instr>)* (else <instr>*)?
					parseParenthesized(cursor, [&] {
						require(cursor, t_then);
						parseInstrSequence(cursor);
					});
					if(cursor->nextToken->type == t_leftParenthesis)
					{
						parseParenthesized(cursor, [&] {
							const Token* elseToken = cursor->nextToken;
							require(cursor, t_else_);
							tryEmitElse(elseToken, cursor);
							parseInstrSequence(cursor);
						});
					}
				}
				else
				{
					// Second syntax option: <expr> <expr>?
					parseExpr(cursor);
					if(cursor->nextToken->type != t_rightParenthesis)
					{
						tryEmitElse(cursor->nextToken, cursor);
						parseExpr(cursor);
					}
				}
				cursor->functionState->validatingCodeStream.end();
				break;
			}
#define VISIT_OP(opcode, name, nameString, Imm, ...)                                               \
	case t_##name:                                                                                 \
		parseOp_##name(cursor, true);                                                              \
		break;
				ENUM_NONCONTROL_OPERATORS(VISIT_OP)
#undef VISIT_OP
			default:
				parseErrorf(cursor->parseState, cursor->nextToken, "expected opcode");
				throw RecoverParseException();
			}
		}
		catch(RecoverParseException const&)
		{
			cursor->functionState->validatingCodeStream.unreachable();
			throw RecoverParseException();
		}
		catch(ValidationException const& exception)
		{
			parseErrorf(cursor->parseState, opcodeToken, "%s", exception.message.c_str());
			cursor->functionState->validatingCodeStream.unreachable();
			throw RecoverParseException();
		}
	});
}

static void parseInstrSequence(CursorState* cursor)
{
	while(true)
	{
		const Token* opcodeToken = cursor->nextToken;
		try
		{
			switch(cursor->nextToken->type)
			{
			case t_leftParenthesis: parseExpr(cursor); break;
			case t_rightParenthesis: return;
			case t_else_: return;
			case t_end: return;
			case t_block:
			{
				++cursor->nextToken;
				parseBlock(cursor, false);
				break;
			}
			case t_loop:
			{
				++cursor->nextToken;
				parseLoop(cursor, false);
				break;
			}
			case t_if_:
			{
				++cursor->nextToken;

				Name branchTargetName;
				ControlStructureImm imm;
				parseControlImm(cursor, branchTargetName, imm);

				ScopedBranchTarget branchTarget(cursor->functionState, branchTargetName);
				cursor->functionState->validatingCodeStream.if_(imm);

				// Parse the then clause.
				parseInstrSequence(cursor);

				// Parse the else clause.
				if(cursor->nextToken->type == t_else_)
				{
					++cursor->nextToken;
					parseAndValidateRedundantBranchTargetName(
						cursor, branchTargetName, "if", "else");

					cursor->functionState->validatingCodeStream.else_();
					parseInstrSequence(cursor);
				}
				cursor->functionState->validatingCodeStream.end();

				require(cursor, t_end);
				parseAndValidateRedundantBranchTargetName(cursor, branchTargetName, "if", "end");

				break;
			}
			case t_try_:
			{
				++cursor->nextToken;

				Name branchTargetName;
				ControlStructureImm imm;
				parseControlImm(cursor, branchTargetName, imm);

				ScopedBranchTarget branchTarget(cursor->functionState, branchTargetName);
				cursor->functionState->validatingCodeStream.try_(imm);

				// Parse the try clause.
				parseInstrSequence(cursor);

				// Parse catch clauses.
				while(cursor->nextToken->type != t_end)
				{
					if(cursor->nextToken->type == t_catch_)
					{
						++cursor->nextToken;
						ExceptionTypeImm exceptionTypeImm;
						parseImm(cursor, exceptionTypeImm);
						cursor->functionState->validatingCodeStream.catch_(exceptionTypeImm);
						parseInstrSequence(cursor);
					}
					else if(cursor->nextToken->type == t_catch_all)
					{
						++cursor->nextToken;
						cursor->functionState->validatingCodeStream.catch_all();
						parseInstrSequence(cursor);
					}
					else
					{
						parseErrorf(cursor->parseState,
									cursor->nextToken,
									"expected 'catch', 'catch_all', or 'end' following 'try'");
						throw RecoverParseException();
					}
				};

				require(cursor, t_end);
				parseAndValidateRedundantBranchTargetName(cursor, branchTargetName, "try", "end");
				cursor->functionState->validatingCodeStream.end();

				break;
			}
			case t_catch_: return;
			case t_catch_all: return;
#define VISIT_OP(opcode, name, nameString, Imm, ...)                                               \
	case t_##name: parseOp_##name(cursor, false); break;
				ENUM_NONCONTROL_OPERATORS(VISIT_OP)
#undef VISIT_OP
			default:
				parseErrorf(cursor->parseState, cursor->nextToken, "expected opcode");
				throw RecoverParseException();
			}
		}
		catch(RecoverParseException const&)
		{
			cursor->functionState->validatingCodeStream.unreachable();
			throw RecoverParseException();
		}
		catch(ValidationException const& exception)
		{
			parseErrorf(cursor->parseState, opcodeToken, "%s", exception.message.c_str());
			cursor->functionState->validatingCodeStream.unreachable();
			throw RecoverParseException();
		}
	};
}

FunctionDef WAST::parseFunctionDef(CursorState* cursor, const Token* funcToken)
{
	std::shared_ptr<std::vector<std::string>> localDisassemblyNames
		= std::make_shared<std::vector<std::string>>();
	std::shared_ptr<NameToIndexMap> localNameToIndexMap = std::make_shared<NameToIndexMap>();

	// Parse the function type, as a reference or explicit declaration.
	const UnresolvedFunctionType unresolvedFunctionType
		= parseFunctionTypeRefAndOrDecl(cursor, *localNameToIndexMap, *localDisassemblyNames);

	// Defer resolving the function type until all type declarations have been parsed.
	const Uptr functionIndex = cursor->moduleState->module.functions.size();
	const Uptr functionDefIndex = cursor->moduleState->module.functions.defs.size();
	const Token* firstBodyToken = cursor->nextToken;
	cursor->moduleState->postTypeCallbacks.push_back([functionIndex,
													  functionDefIndex,
													  firstBodyToken,
													  localNameToIndexMap,
													  localDisassemblyNames,
													  unresolvedFunctionType](
														 ModuleState* moduleState) {
		// Resolve the function type and set it on the FunctionDef.
		const IndexedFunctionType functionTypeIndex
			= resolveFunctionType(moduleState, unresolvedFunctionType);
		moduleState->module.functions.defs[functionDefIndex].type = functionTypeIndex;

		// Defer parsing the body of the function until all function types have been resolved.
		moduleState->postDeclarationCallbacks.push_back([functionIndex,
														 functionDefIndex,
														 firstBodyToken,
														 localNameToIndexMap,
														 localDisassemblyNames,
														 functionTypeIndex](
															ModuleState* moduleState) {
			FunctionDef& functionDef = moduleState->module.functions.defs[functionDefIndex];
			FunctionType functionType = functionTypeIndex.index == UINTPTR_MAX
											? FunctionType()
											: moduleState->module.types[functionTypeIndex.index];

			// Parse the function's local variables.
			CursorState functionCursorState(firstBodyToken, moduleState->parseState, moduleState);
			while(tryParseParenthesizedTagged(&functionCursorState, t_local, [&] {
				Name localName;
				if(tryParseName(&functionCursorState, localName))
				{
					bindName(
						moduleState->parseState,
						*localNameToIndexMap,
						localName,
						functionType.params().size() + functionDef.nonParameterLocalTypes.size());
					localDisassemblyNames->push_back(localName.getString());
					functionDef.nonParameterLocalTypes.push_back(
						parseValueType(&functionCursorState));
				}
				else
				{
					while(functionCursorState.nextToken->type != t_rightParenthesis)
					{
						localDisassemblyNames->push_back(std::string());
						functionDef.nonParameterLocalTypes.push_back(
							parseValueType(&functionCursorState));
					};
				}
			}))
				;
			moduleState->disassemblyNames.functions[functionIndex].locals
				= std::move(*localDisassemblyNames);

			// Parse the function's code.
			FunctionState functionState(localNameToIndexMap, functionDef, moduleState);
			functionCursorState.functionState = &functionState;
			const Token* validationErrorToken = firstBodyToken;
			try
			{
				parseInstrSequence(&functionCursorState);
				if(!moduleState->parseState->unresolvedErrors.size())
				{
					validationErrorToken = functionCursorState.nextToken;
					functionState.validatingCodeStream.end();
					functionState.validatingCodeStream.finishValidation();
				}
			}
			catch(ValidationException const& exception)
			{
				parseErrorf(
					moduleState->parseState, validationErrorToken, "%s", exception.message.c_str());
			}
			catch(RecoverParseException const&)
			{
			}
			catch(FatalParseException const&)
			{
			}
			functionDef.code = std::move(functionState.codeByteStream.getBytes());
			moduleState->disassemblyNames.functions[functionIndex].labels
				= std::move(functionState.labelDisassemblyNames);
		});
	});

	// Continue parsing after the closing parenthesis.
	findClosingParenthesis(cursor, funcToken - 1);
	--cursor->nextToken;

	return {{UINTPTR_MAX}, {}, {}, {}};
}
