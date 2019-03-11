#include <inttypes.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Lexer.h"
#include "Parse.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::WAST;

static bool tryParseSizeConstraints(CursorState* cursor,
									U64 maxMax,
									SizeConstraints& outSizeConstraints)
{
	outSizeConstraints.min = 0;
	outSizeConstraints.max = UINT64_MAX;

	// Parse a minimum.
	if(!tryParseI64(cursor, outSizeConstraints.min)) { return false; }
	else
	{
		// Parse an optional maximum.
		if(!tryParseI64(cursor, outSizeConstraints.max)) { outSizeConstraints.max = UINT64_MAX; }
		else
		{
			// Validate that the maximum size is within the limit, and that the size contraints is
			// not disjoint.
			if(outSizeConstraints.max > maxMax)
			{
				parseErrorf(cursor->parseState,
							cursor->nextToken - 1,
							"maximum size exceeds limit (%" PRIu64 ">%" PRIu64 ")",
							outSizeConstraints.max,
							maxMax);
				outSizeConstraints.max = maxMax;
			}
			else if(outSizeConstraints.max < outSizeConstraints.min)
			{
				parseErrorf(cursor->parseState,
							cursor->nextToken - 1,
							"maximum size is less than minimum size (%" PRIu64 "<%" PRIu64 ")",
							outSizeConstraints.max,
							outSizeConstraints.min);
				outSizeConstraints.max = outSizeConstraints.min;
			}
		}

		return true;
	}
}

static SizeConstraints parseSizeConstraints(CursorState* cursor, U64 maxMax)
{
	SizeConstraints result;
	if(!tryParseSizeConstraints(cursor, maxMax, result))
	{ parseErrorf(cursor->parseState, cursor->nextToken, "expected size constraints"); }
	return result;
}

static GlobalType parseGlobalType(CursorState* cursor)
{
	GlobalType result;
	result.isMutable = tryParseParenthesizedTagged(
		cursor, t_mut, [&] { result.valueType = parseValueType(cursor); });
	if(!result.isMutable) { result.valueType = parseValueType(cursor); }
	return result;
}

static TypeTuple parseTypeTuple(CursorState* cursor)
{
	std::vector<ValueType> parameters;
	ValueType elementType;
	while(tryParseValueType(cursor, elementType)) { parameters.push_back(elementType); }
	return TypeTuple(parameters);
}

// An unresolved initializer expression: uses a Reference instead of an index for global.get and
// ref.func.
typedef InitializerExpressionBase<Reference> UnresolvedInitializerExpression;

static UnresolvedInitializerExpression parseInitializerExpression(CursorState* cursor)
{
	UnresolvedInitializerExpression result;
	parseParenthesized(cursor, [&] {
		switch(cursor->nextToken->type)
		{
		case t_i32_const:
		{
			++cursor->nextToken;
			result = (I32)parseI32(cursor);
			break;
		}
		case t_i64_const:
		{
			++cursor->nextToken;
			result = (I64)parseI64(cursor);
			break;
		}
		case t_f32_const:
		{
			++cursor->nextToken;
			result = parseF32(cursor);
			break;
		}
		case t_f64_const:
		{
			++cursor->nextToken;
			result = parseF64(cursor);
			break;
		}
		case t_v128_const:
		{
			++cursor->nextToken;
			result = parseV128(cursor);
			break;
		}
		case t_global_get:
		{
			++cursor->nextToken;
			Reference globalRef;
			if(!tryParseNameOrIndexRef(cursor, globalRef))
			{
				parseErrorf(cursor->parseState, cursor->nextToken, "expected global name or index");
				throw RecoverParseException();
			}
			result = UnresolvedInitializerExpression(
				UnresolvedInitializerExpression::Type::global_get, globalRef);
			break;
		}
		case t_ref_null:
		{
			++cursor->nextToken;
			result = nullptr;
			break;
		}
		case t_ref_func:
		{
			++cursor->nextToken;
			Reference funcRef;
			if(!tryParseNameOrIndexRef(cursor, funcRef))
			{
				parseErrorf(
					cursor->parseState, cursor->nextToken, "expected function name or index");
				throw RecoverParseException();
			}
			result = UnresolvedInitializerExpression(
				UnresolvedInitializerExpression::Type::ref_func, funcRef);
			break;
		}
		default:
			parseErrorf(cursor->parseState, cursor->nextToken, "expected initializer expression");
			throw RecoverParseException();
		};
	});

	return result;
}

static InitializerExpression resolveInitializerExpression(
	ModuleState* moduleState,
	UnresolvedInitializerExpression unresolvedExpression)
{
	switch(unresolvedExpression.type)
	{
	case UnresolvedInitializerExpression::Type::i32_const:
		return InitializerExpression(unresolvedExpression.i32);
	case UnresolvedInitializerExpression::Type::i64_const:
		return InitializerExpression(unresolvedExpression.i64);
	case UnresolvedInitializerExpression::Type::f32_const:
		return InitializerExpression(unresolvedExpression.f32);
	case UnresolvedInitializerExpression::Type::f64_const:
		return InitializerExpression(unresolvedExpression.f64);
	case UnresolvedInitializerExpression::Type::v128_const:
		return InitializerExpression(unresolvedExpression.v128);
	case UnresolvedInitializerExpression::Type::global_get:
		return InitializerExpression(InitializerExpression::Type::global_get,
									 resolveRef(moduleState->parseState,
												moduleState->globalNameToIndexMap,
												moduleState->module.globals.size(),
												unresolvedExpression.ref));
	case UnresolvedInitializerExpression::Type::ref_null: return InitializerExpression(nullptr);
	case UnresolvedInitializerExpression::Type::ref_func:
		return InitializerExpression(InitializerExpression::Type::ref_func,
									 resolveRef(moduleState->parseState,
												moduleState->functionNameToIndexMap,
												moduleState->module.functions.size(),
												unresolvedExpression.ref));
	case UnresolvedInitializerExpression::Type::error: return InitializerExpression();
	default: Errors::unreachable();
	}
}

static void errorIfFollowsDefinitions(CursorState* cursor)
{
	if(cursor->moduleState->module.functions.defs.size()
	   || cursor->moduleState->module.tables.defs.size()
	   || cursor->moduleState->module.memories.defs.size()
	   || cursor->moduleState->module.globals.defs.size())
	{
		parseErrorf(cursor->parseState,
					cursor->nextToken,
					"import declarations must precede all definitions");
	}
}

template<typename Def, typename Type, typename DisassemblyName>
static Uptr createImport(CursorState* cursor,
						 Name name,
						 std::string&& moduleName,
						 std::string&& exportName,
						 NameToIndexMap& nameToIndexMap,
						 IndexSpace<Def, Type>& indexSpace,
						 std::vector<DisassemblyName>& disassemblyNameArray,
						 Type type)
{
	const Uptr importIndex = indexSpace.imports.size();
	bindName(cursor->parseState, nameToIndexMap, name, indexSpace.size());
	disassemblyNameArray.push_back({name.getString()});
	indexSpace.imports.push_back({type, std::move(moduleName), std::move(exportName)});
	return importIndex;
}

static bool parseOptionalSharedDeclaration(CursorState* cursor)
{
	if(cursor->nextToken->type == t_shared)
	{
		++cursor->nextToken;
		return true;
	}
	else
	{
		return false;
	}
}

static void parseImport(CursorState* cursor)
{
	errorIfFollowsDefinitions(cursor);

	require(cursor, t_import);

	std::string moduleName = parseUTF8String(cursor);
	std::string exportName = parseUTF8String(cursor);

	parseParenthesized(cursor, [&] {
		// Parse the import kind.
		const Token* importKindToken = cursor->nextToken;
		switch(importKindToken->type)
		{
		case t_func:
		case t_table:
		case t_memory:
		case t_global:
		case t_exception_type: ++cursor->nextToken; break;
		default:
			parseErrorf(cursor->parseState, cursor->nextToken, "invalid import type");
			throw RecoverParseException();
		}

		// Parse an optional internal name for the import.
		Name name;
		tryParseName(cursor, name);

		// Parse the import type and create the import in the appropriate name/index spaces.
		switch(importKindToken->type)
		{
		case t_func:
		{
			NameToIndexMap localNameToIndexMap;
			std::vector<std::string> localDissassemblyNames;
			const UnresolvedFunctionType unresolvedFunctionType = parseFunctionTypeRefAndOrDecl(
				cursor, localNameToIndexMap, localDissassemblyNames);
			const Uptr importIndex = createImport(cursor,
												  name,
												  std::move(moduleName),
												  std::move(exportName),
												  cursor->moduleState->functionNameToIndexMap,
												  cursor->moduleState->module.functions,
												  cursor->moduleState->disassemblyNames.functions,
												  {UINTPTR_MAX});
			cursor->moduleState->disassemblyNames.functions.back().locals = localDissassemblyNames;

			// Resolve the function import type after all type declarations have been parsed.
			cursor->moduleState->postTypeCallbacks.push_back(
				[unresolvedFunctionType, importIndex](ModuleState* moduleState) {
					moduleState->module.functions.imports[importIndex].type
						= resolveFunctionType(moduleState, unresolvedFunctionType);
				});
			break;
		}
		case t_table:
		{
			const SizeConstraints sizeConstraints = parseSizeConstraints(cursor, IR::maxTableElems);
			const bool isShared = parseOptionalSharedDeclaration(cursor);
			const ReferenceType elemType = parseReferenceType(cursor);
			createImport(cursor,
						 name,
						 std::move(moduleName),
						 std::move(exportName),
						 cursor->moduleState->tableNameToIndexMap,
						 cursor->moduleState->module.tables,
						 cursor->moduleState->disassemblyNames.tables,
						 {elemType, isShared, sizeConstraints});
			break;
		}
		case t_memory:
		{
			const SizeConstraints sizeConstraints
				= parseSizeConstraints(cursor, IR::maxMemoryPages);
			const bool isShared = parseOptionalSharedDeclaration(cursor);
			createImport(cursor,
						 name,
						 std::move(moduleName),
						 std::move(exportName),
						 cursor->moduleState->memoryNameToIndexMap,
						 cursor->moduleState->module.memories,
						 cursor->moduleState->disassemblyNames.memories,
						 MemoryType{isShared, sizeConstraints});
			break;
		}
		case t_global:
		{
			const GlobalType globalType = parseGlobalType(cursor);
			createImport(cursor,
						 name,
						 std::move(moduleName),
						 std::move(exportName),
						 cursor->moduleState->globalNameToIndexMap,
						 cursor->moduleState->module.globals,
						 cursor->moduleState->disassemblyNames.globals,
						 globalType);
			break;
		}
		case t_exception_type:
		{
			TypeTuple params = parseTypeTuple(cursor);
			createImport(cursor,
						 name,
						 std::move(moduleName),
						 std::move(exportName),
						 cursor->moduleState->exceptionTypeNameToIndexMap,
						 cursor->moduleState->module.exceptionTypes,
						 cursor->moduleState->disassemblyNames.exceptionTypes,
						 ExceptionType{params});
			break;
		}
		default: Errors::unreachable();
		};
	});
}

static void parseExport(CursorState* cursor)
{
	require(cursor, t_export);

	const std::string exportName = parseUTF8String(cursor);

	parseParenthesized(cursor, [&] {
		ExternKind exportKind;
		switch(cursor->nextToken->type)
		{
		case t_func: exportKind = ExternKind::function; break;
		case t_table: exportKind = ExternKind::table; break;
		case t_memory: exportKind = ExternKind::memory; break;
		case t_global: exportKind = ExternKind::global; break;
		case t_exception_type: exportKind = ExternKind::exceptionType; break;
		default:
			parseErrorf(cursor->parseState, cursor->nextToken, "invalid export kind");
			throw RecoverParseException();
		};
		++cursor->nextToken;

		Reference exportRef;
		if(!tryParseNameOrIndexRef(cursor, exportRef))
		{
			parseErrorf(cursor->parseState, cursor->nextToken, "expected name or index");
			throw RecoverParseException();
		}

		const Uptr exportIndex = cursor->moduleState->module.exports.size();
		cursor->moduleState->module.exports.push_back({std::move(exportName), exportKind, 0});

		cursor->moduleState->postDeclarationCallbacks.push_back([=](ModuleState* moduleState) {
			Uptr& exportedObjectIndex = moduleState->module.exports[exportIndex].index;
			switch(exportKind)
			{
			case ExternKind::function:
				exportedObjectIndex = resolveRef(moduleState->parseState,
												 moduleState->functionNameToIndexMap,
												 moduleState->module.functions.size(),
												 exportRef);
				break;
			case ExternKind::table:
				exportedObjectIndex = resolveRef(moduleState->parseState,
												 moduleState->tableNameToIndexMap,
												 moduleState->module.tables.size(),
												 exportRef);
				break;
			case ExternKind::memory:
				exportedObjectIndex = resolveRef(moduleState->parseState,
												 moduleState->memoryNameToIndexMap,
												 moduleState->module.memories.size(),
												 exportRef);
				break;
			case ExternKind::global:
				exportedObjectIndex = resolveRef(moduleState->parseState,
												 moduleState->globalNameToIndexMap,
												 moduleState->module.globals.size(),
												 exportRef);
				break;
			case ExternKind::exceptionType:
				exportedObjectIndex = resolveRef(moduleState->parseState,
												 moduleState->exceptionTypeNameToIndexMap,
												 moduleState->module.exceptionTypes.size(),
												 exportRef);
				break;
			default: Errors::unreachable();
			}
		});
	});
}

static void parseType(CursorState* cursor)
{
	require(cursor, t_type);

	Name name;
	tryParseName(cursor, name);

	parseParenthesized(cursor, [&] {
		require(cursor, t_func);

		NameToIndexMap parameterNameToIndexMap;
		std::vector<std::string> localDisassemblyNames;
		FunctionType functionType
			= parseFunctionType(cursor, parameterNameToIndexMap, localDisassemblyNames);

		const Uptr functionTypeIndex = cursor->moduleState->module.types.size();
		cursor->moduleState->module.types.push_back(functionType);
		cursor->moduleState->functionTypeToIndexMap.set(functionType, functionTypeIndex);

		bindName(
			cursor->parseState, cursor->moduleState->typeNameToIndexMap, name, functionTypeIndex);
		cursor->moduleState->disassemblyNames.types.push_back(name.getString());
	});
}

static bool parseSegmentDeclaration(CursorState* cursor,
									Name& outSegmentName,
									Reference& outMemoryOrTableRef,
									UnresolvedInitializerExpression& outBaseIndex)
{
	// The segment can have a name, and for active segments, a reference to a memory or table. If
	// there are two names, the first is the segment name, and the second is the reference to a
	// memory or table. If there is one name on a passive segment, it is the segment name. If there
	// is one name on an active segment, it is a reference to a memory or table.
	bool isActive = true;
	switch(cursor->nextToken[0].type)
	{
	case t_passive:
		// passive ...
		require(cursor, t_passive);
		isActive = false;
		break;
	case t_quotedName:
	case t_name:
		switch(cursor->nextToken[1].type)
		{
		case t_passive:
			// <s:name> passive ...
			errorUnless(tryParseName(cursor, outSegmentName));
			require(cursor, t_passive);
			isActive = false;
			break;
		case t_quotedName:
		case t_name:
		case t_hexInt:
		case t_decimalInt:
			// <s:name> <m:ref> ...
			errorUnless(tryParseName(cursor, outSegmentName));
			errorUnless(tryParseNameOrIndexRef(cursor, outMemoryOrTableRef));
			break;
		default:
			// <m:name> ...
			errorUnless(tryParseNameOrIndexRef(cursor, outMemoryOrTableRef));
		}
		break;
	case t_hexInt:
	case t_decimalInt:
		// <m:ref> ...
		errorUnless(tryParseNameOrIndexRef(cursor, outMemoryOrTableRef));
		break;
	default:
		// ...
		break;
	};

	if(isActive)
	{
		// Parse an initializer expression for the base index of the elem segment.
		if(cursor->nextToken[0].type == t_leftParenthesis && cursor->nextToken[1].type == t_offset)
		{
			// The initializer expression can optionally be wrapped in (offset ...)
			parseParenthesized(cursor, [&] {
				require(cursor, t_offset);
				outBaseIndex = parseInitializerExpression(cursor);
			});
		}
		else
		{
			outBaseIndex = parseInitializerExpression(cursor);
		}
	}

	return isActive;
}

static void parseData(CursorState* cursor)
{
	const Token* firstToken = cursor->nextToken;
	require(cursor, t_data);

	Name segmentName;
	Reference memoryRef;
	UnresolvedInitializerExpression baseAddress;
	bool isActive = parseSegmentDeclaration(cursor, segmentName, memoryRef, baseAddress);

	// Parse a list of strings that contains the segment's data.
	std::string dataString;
	while(tryParseString(cursor, dataString)) {};

	// Create the data segment.
	std::vector<U8> dataVector((const U8*)dataString.data(),
							   (const U8*)dataString.data() + dataString.size());
	const Uptr dataSegmentIndex = cursor->moduleState->module.dataSegments.size();
	cursor->moduleState->module.dataSegments.push_back(
		{isActive, UINTPTR_MAX, InitializerExpression(), std::move(dataVector)});

	if(segmentName)
	{
		bindName(cursor->parseState,
				 cursor->moduleState->dataNameToIndexMap,
				 segmentName,
				 dataSegmentIndex);
	}
	cursor->moduleState->disassemblyNames.dataSegments.push_back(segmentName.getString());

	// Enqueue a callback that is called after all declarations are parsed to resolve the memory to
	// put the data segment in, and the base offset.
	if(isActive)
	{
		cursor->moduleState->postDeclarationCallbacks.push_back([=](ModuleState* moduleState) {
			if(!moduleState->module.memories.size())
			{
				parseErrorf(
					moduleState->parseState,
					firstToken,
					"data segments aren't allowed in modules without any memory declarations");
			}
			else
			{
				DataSegment& dataSegment = moduleState->module.dataSegments[dataSegmentIndex];
				dataSegment.memoryIndex = memoryRef
											  ? resolveRef(moduleState->parseState,
														   moduleState->memoryNameToIndexMap,
														   moduleState->module.memories.size(),
														   memoryRef)
											  : 0;
				dataSegment.baseOffset = resolveInitializerExpression(moduleState, baseAddress);
			}
		});
	}
}

static Uptr parseElemSegmentBody(CursorState* cursor,
								 bool isActive,
								 Name segmentName,
								 Reference tableRef,
								 UnresolvedInitializerExpression baseIndex,
								 ReferenceType elemType,
								 const Token* elemToken)
{
	struct UnresolvedElem
	{
		Elem::Type type;
		Reference ref;
	};

	// Allocate the elementReferences array on the heap so it doesn't need to be copied for the
	// post-declaration callback.
	std::shared_ptr<std::vector<UnresolvedElem>> elementReferences
		= std::make_shared<std::vector<UnresolvedElem>>();

	while(cursor->nextToken->type != t_rightParenthesis)
	{
		if(!isActive && cursor->nextToken->type == t_leftParenthesis)
		{
			parseParenthesized(cursor, [&] {
				switch(cursor->nextToken->type)
				{
				case t_ref_null:
					++cursor->nextToken;
					elementReferences->push_back({Elem::Type::ref_null});
					break;
				case t_ref_func:
				{
					++cursor->nextToken;

					Reference elementRef;
					if(!tryParseNameOrIndexRef(cursor, elementRef))
					{
						parseErrorf(cursor->parseState,
									cursor->nextToken,
									"expected function name or index");
						throw RecoverParseException();
					}

					elementReferences->push_back({Elem::Type::ref_func, std::move(elementRef)});
					break;
				}
				default:
					parseErrorf(
						cursor->parseState, cursor->nextToken, "expected 'ref.func' or 'ref.null'");
					throw RecoverParseException();
				};
			});
		}
		else
		{
			Reference elementRef;
			if(!tryParseNameOrIndexRef(cursor, elementRef))
			{
				parseErrorf(
					cursor->parseState, cursor->nextToken, "expected function name or index");
				throw RecoverParseException();
			}

			elementReferences->push_back({Elem::Type::ref_func, std::move(elementRef)});
		}
	}

	// Create the elem segment.
	const Uptr elemSegmentIndex = cursor->moduleState->module.elemSegments.size();
	cursor->moduleState->module.elemSegments.push_back({isActive,
														UINTPTR_MAX,
														InitializerExpression(),
														ReferenceType::funcref,
														std::vector<Elem>()});

	if(segmentName)
	{
		bindName(cursor->parseState,
				 cursor->moduleState->elemNameToIndexMap,
				 segmentName,
				 elemSegmentIndex);
	}
	cursor->moduleState->disassemblyNames.elemSegments.push_back(segmentName.getString());

	// Enqueue a callback that is called after all declarations are parsed to resolve the table
	// elements' references.
	cursor->moduleState->postDeclarationCallbacks.push_back(
		[isActive, tableRef, elemSegmentIndex, elementReferences, elemToken, baseIndex](
			ModuleState* moduleState) {
			ElemSegment& elemSegment = moduleState->module.elemSegments[elemSegmentIndex];

			if(isActive)
			{
				if(!moduleState->module.tables.size())
				{
					parseErrorf(
						moduleState->parseState,
						elemToken,
						"elem segments aren't allowed in modules without any table declarations");
				}
				else
				{
					elemSegment.tableIndex = tableRef
												 ? resolveRef(moduleState->parseState,
															  moduleState->tableNameToIndexMap,
															  moduleState->module.tables.size(),
															  tableRef)
												 : 0;
					elemSegment.baseOffset = resolveInitializerExpression(moduleState, baseIndex);
				}
			}

			elemSegment.elems.resize(elementReferences->size());
			for(Uptr elementIndex = 0; elementIndex < elementReferences->size(); ++elementIndex)
			{
				const UnresolvedElem& unresolvedElem = (*elementReferences)[elementIndex];
				switch(unresolvedElem.type)
				{
				case Elem::Type::ref_null:
					elemSegment.elems[elementIndex] = {{Elem::Type::ref_null}, UINTPTR_MAX};
					break;
				case Elem::Type::ref_func:
					elemSegment.elems[elementIndex]
						= {{Elem::Type::ref_func},
						   resolveRef(moduleState->parseState,
									  moduleState->functionNameToIndexMap,
									  moduleState->module.functions.size(),
									  unresolvedElem.ref)};
					break;
				default: Errors::unreachable();
				}
			}
		});

	return elementReferences->size();
}

static void parseElem(CursorState* cursor)
{
	const Token* elemToken = cursor->nextToken;
	require(cursor, t_elem);

	Name segmentName;
	Reference tableRef;
	UnresolvedInitializerExpression baseIndex;
	bool isActive = parseSegmentDeclaration(cursor, segmentName, tableRef, baseIndex);

	ReferenceType elemType = ReferenceType::funcref;
	if(!isActive) { elemType = parseReferenceType(cursor); }

	parseElemSegmentBody(cursor, isActive, segmentName, tableRef, baseIndex, elemType, elemToken);
}

template<typename Def,
		 typename Type,
		 typename ParseImport,
		 typename ParseDef,
		 typename DisassemblyName>
static void parseObjectDefOrImport(CursorState* cursor,
								   NameToIndexMap& nameToIndexMap,
								   IR::IndexSpace<Def, Type>& indexSpace,
								   std::vector<DisassemblyName>& disassemblyNameArray,
								   TokenType declarationTag,
								   IR::ExternKind kind,
								   ParseImport parseImportFunc,
								   ParseDef parseDefFunc)
{
	const Token* declarationTagToken = cursor->nextToken;
	require(cursor, declarationTag);

	Name name;
	tryParseName(cursor, name);

	// Handle inline export declarations.
	while(true)
	{
		const bool isExport = tryParseParenthesizedTagged(cursor, t_export, [&] {
			cursor->moduleState->module.exports.push_back(
				{parseUTF8String(cursor), kind, indexSpace.size()});
		});
		if(!isExport) { break; }
	};

	// Handle an inline import declaration.
	std::string importModuleName;
	std::string exportName;
	const bool isImport = tryParseParenthesizedTagged(cursor, t_import, [&] {
		errorIfFollowsDefinitions(cursor);

		importModuleName = parseUTF8String(cursor);
		exportName = parseUTF8String(cursor);
	});
	if(isImport)
	{
		Type importType = parseImportFunc(cursor);
		createImport(cursor,
					 name,
					 std::move(importModuleName),
					 std::move(exportName),
					 nameToIndexMap,
					 indexSpace,
					 disassemblyNameArray,
					 importType);
	}
	else
	{
		Def def = parseDefFunc(cursor, declarationTagToken);
		bindName(cursor->parseState, nameToIndexMap, name, indexSpace.size());
		indexSpace.defs.push_back(std::move(def));
		disassemblyNameArray.push_back({name.getString()});
	}
}

static void parseFunc(CursorState* cursor)
{
	parseObjectDefOrImport(
		cursor,
		cursor->moduleState->functionNameToIndexMap,
		cursor->moduleState->module.functions,
		cursor->moduleState->disassemblyNames.functions,
		t_func,
		ExternKind::function,
		[&](CursorState* cursor) {
			// Parse the imported function's type.
			NameToIndexMap localNameToIndexMap;
			std::vector<std::string> localDisassemblyNames;
			const UnresolvedFunctionType unresolvedFunctionType
				= parseFunctionTypeRefAndOrDecl(cursor, localNameToIndexMap, localDisassemblyNames);

			// Resolve the function import type after all type declarations have been parsed.
			const Uptr importIndex = cursor->moduleState->module.functions.imports.size();
			cursor->moduleState->postTypeCallbacks.push_back(
				[unresolvedFunctionType, importIndex](ModuleState* moduleState) {
					moduleState->module.functions.imports[importIndex].type
						= resolveFunctionType(moduleState, unresolvedFunctionType);
				});
			return IndexedFunctionType{UINTPTR_MAX};
		},
		parseFunctionDef);
}

static void parseTable(CursorState* cursor)
{
	parseObjectDefOrImport(
		cursor,
		cursor->moduleState->tableNameToIndexMap,
		cursor->moduleState->module.tables,
		cursor->moduleState->disassemblyNames.tables,
		t_table,
		ExternKind::table,
		// Parse a table import.
		[](CursorState* cursor) {
			const SizeConstraints sizeConstraints = parseSizeConstraints(cursor, IR::maxTableElems);
			const bool isShared = parseOptionalSharedDeclaration(cursor);
			const ReferenceType elemType = parseReferenceType(cursor);
			return TableType{elemType, isShared, sizeConstraints};
		},
		// Parse a table definition.
		[](CursorState* cursor, const Token*) {
			// Parse the table type.
			SizeConstraints sizeConstraints;
			const bool hasSizeConstraints
				= tryParseSizeConstraints(cursor, IR::maxTableElems, sizeConstraints);
			const bool isShared = parseOptionalSharedDeclaration(cursor);

			const ReferenceType elemType = parseReferenceType(cursor);

			// If we couldn't parse an explicit size constraints, the table definition must contain
			// an elem segment that implicitly defines the size.
			if(!hasSizeConstraints)
			{
				parseParenthesized(cursor, [&] {
					require(cursor, t_elem);

					const Uptr tableIndex = cursor->moduleState->module.tables.size();
					const Uptr numElements
						= parseElemSegmentBody(cursor,
											   true,
											   Name(),
											   Reference(tableIndex),
											   UnresolvedInitializerExpression((I32)0),
											   elemType,
											   cursor->nextToken - 1);
					sizeConstraints.min = sizeConstraints.max = numElements;
				});
			}

			return TableDef{TableType(elemType, isShared, sizeConstraints)};
		});
}

static void parseMemory(CursorState* cursor)
{
	parseObjectDefOrImport(
		cursor,
		cursor->moduleState->memoryNameToIndexMap,
		cursor->moduleState->module.memories,
		cursor->moduleState->disassemblyNames.memories,
		t_memory,
		ExternKind::memory,
		// Parse a memory import.
		[](CursorState* cursor) {
			const SizeConstraints sizeConstraints
				= parseSizeConstraints(cursor, IR::maxMemoryPages);
			const bool isShared = parseOptionalSharedDeclaration(cursor);
			return MemoryType{isShared, sizeConstraints};
		},
		// Parse a memory definition
		[](CursorState* cursor, const Token*) {
			SizeConstraints sizeConstraints;
			if(!tryParseSizeConstraints(cursor, IR::maxMemoryPages, sizeConstraints))
			{
				std::string dataString;

				parseParenthesized(cursor, [&] {
					require(cursor, t_data);

					while(tryParseString(cursor, dataString)) {};
				});

				std::vector<U8> dataVector((const U8*)dataString.data(),
										   (const U8*)dataString.data() + dataString.size());
				sizeConstraints.min = sizeConstraints.max
					= (dataVector.size() + IR::numBytesPerPage - 1) / IR::numBytesPerPage;
				cursor->moduleState->module.dataSegments.push_back(
					{true,
					 cursor->moduleState->module.memories.size(),
					 InitializerExpression(I32(0)),
					 std::move(dataVector)});
				cursor->moduleState->disassemblyNames.dataSegments.push_back(std::string());
			}

			const bool isShared = parseOptionalSharedDeclaration(cursor);
			return MemoryDef{MemoryType(isShared, sizeConstraints)};
		});
}

static void parseGlobal(CursorState* cursor)
{
	parseObjectDefOrImport(
		cursor,
		cursor->moduleState->globalNameToIndexMap,
		cursor->moduleState->module.globals,
		cursor->moduleState->disassemblyNames.globals,
		t_global,
		ExternKind::global,
		// Parse a global import.
		parseGlobalType,
		// Parse a global definition
		[](CursorState* cursor, const Token*) {
			const GlobalType globalType = parseGlobalType(cursor);

			// Parse the unresolved initializer expression, but defer resolving it until all
			// declarations have been parsed. This allows the initializer to reference
			// function/global names declared after this global.
			const UnresolvedInitializerExpression unresolvedInitializerExpression
				= parseInitializerExpression(cursor);
			const Uptr globalDefIndex = cursor->moduleState->module.globals.defs.size();
			cursor->moduleState->postDeclarationCallbacks.push_back(
				[cursor, globalDefIndex, unresolvedInitializerExpression](
					ModuleState* moduleState) {
					cursor->moduleState->module.globals.defs[globalDefIndex].initializer
						= resolveInitializerExpression(cursor->moduleState,
													   unresolvedInitializerExpression);
				});

			return GlobalDef{globalType, InitializerExpression()};
		});
}

static void parseExceptionType(CursorState* cursor)
{
	parseObjectDefOrImport(
		cursor,
		cursor->moduleState->exceptionTypeNameToIndexMap,
		cursor->moduleState->module.exceptionTypes,
		cursor->moduleState->disassemblyNames.exceptionTypes,
		t_exception_type,
		ExternKind::exceptionType,
		// Parse an exception type import.
		[](CursorState* cursor) {
			TypeTuple params = parseTypeTuple(cursor);
			return ExceptionType{params};
		},
		// Parse an exception type definition
		[](CursorState* cursor, const Token*) {
			TypeTuple params = parseTypeTuple(cursor);
			return ExceptionTypeDef{ExceptionType{params}};
		});
}

static void parseStart(CursorState* cursor)
{
	require(cursor, t_start);

	Reference functionRef;
	if(!tryParseNameOrIndexRef(cursor, functionRef))
	{ parseErrorf(cursor->parseState, cursor->nextToken, "expected function name or index"); }

	cursor->moduleState->postDeclarationCallbacks.push_back([functionRef](
																ModuleState* moduleState) {
		moduleState->module.startFunctionIndex = resolveRef(moduleState->parseState,
															moduleState->functionNameToIndexMap,
															moduleState->module.functions.size(),
															functionRef);
	});
}

static void parseDeclaration(CursorState* cursor)
{
	parseParenthesized(cursor, [&] {
		switch(cursor->nextToken->type)
		{
		case t_import: parseImport(cursor); return true;
		case t_export: parseExport(cursor); return true;
		case t_exception_type: parseExceptionType(cursor); return true;
		case t_global: parseGlobal(cursor); return true;
		case t_memory: parseMemory(cursor); return true;
		case t_table: parseTable(cursor); return true;
		case t_type: parseType(cursor); return true;
		case t_data: parseData(cursor); return true;
		case t_elem: parseElem(cursor); return true;
		case t_func: parseFunc(cursor); return true;
		case t_start: parseStart(cursor); return true;
		default:
			parseErrorf(cursor->parseState, cursor->nextToken, "unrecognized definition in module");
			throw RecoverParseException();
		};
	});
}

template<typename Map> void dumpHashMapSpaceAnalysis(const Map& map, const char* description)
{
	if(map.size())
	{
		Uptr totalMemoryBytes = 0;
		Uptr maxProbeCount = 0;
		F32 occupancy = 0.0f;
		F32 averageProbeCount = 0.0f;
		map.analyzeSpaceUsage(totalMemoryBytes, maxProbeCount, occupancy, averageProbeCount);
		Log::printf(
			Log::metrics,
			"%s used %.1fKB for %" PRIuPTR
			" elements (%.0f%% occupancy, %.1f bytes/element). Avg/max probe length: %f/%" PRIuPTR
			"\n",
			description,
			totalMemoryBytes / 1024.0f,
			map.size(),
			occupancy * 100.0f,
			F32(totalMemoryBytes) / map.size(),
			averageProbeCount,
			maxProbeCount);
	}
}

void WAST::parseModuleBody(CursorState* cursor, IR::Module& outModule)
{
	try
	{
		const Token* firstToken = cursor->nextToken;
		ModuleState moduleState(cursor->parseState, outModule);
		cursor->moduleState = &moduleState;

		// Parse the module's declarations.
		while(cursor->nextToken->type != t_rightParenthesis && cursor->nextToken->type != t_eof)
		{ parseDeclaration(cursor); };

		// Process the callbacks requested after all type declarations have been parsed.
		if(!cursor->parseState->unresolvedErrors.size())
		{
			for(const auto& callback : cursor->moduleState->postTypeCallbacks)
			{ callback(&moduleState); }
		}

		// Process the callbacks requested after all declarations have been parsed.
		if(!cursor->parseState->unresolvedErrors.size())
		{
			for(const auto& callback : cursor->moduleState->postDeclarationCallbacks)
			{ callback(&moduleState); }
		}

		// Validate the module's definitions (excluding function code, which is validated as it is
		// parsed).
		if(!cursor->parseState->unresolvedErrors.size())
		{
			try
			{
				IR::validatePreCodeSections(outModule);
				IR::validatePostCodeSections(outModule);
			}
			catch(ValidationException const& validationException)
			{
				parseErrorf(cursor->parseState,
							firstToken,
							"validation exception: %s",
							validationException.message.c_str());
			}
		}

		// Set the module's disassembly names.
		const DisassemblyNames& disassemblyNames = moduleState.disassemblyNames;
		wavmAssert(outModule.functions.size() == disassemblyNames.functions.size());
		wavmAssert(outModule.tables.size() == disassemblyNames.tables.size());
		wavmAssert(outModule.memories.size() == disassemblyNames.memories.size());
		wavmAssert(outModule.globals.size() == disassemblyNames.globals.size());
		wavmAssert(outModule.elemSegments.size() == disassemblyNames.elemSegments.size());
		wavmAssert(outModule.dataSegments.size() == disassemblyNames.dataSegments.size());
		wavmAssert(outModule.exceptionTypes.size() == disassemblyNames.exceptionTypes.size());
		IR::setDisassemblyNames(outModule, disassemblyNames);

		// If metrics logging is enabled, log some statistics about the module's name maps.
		if(Log::isCategoryEnabled(Log::metrics))
		{
			dumpHashMapSpaceAnalysis(moduleState.functionNameToIndexMap, "functionNameToIndexMap");
			dumpHashMapSpaceAnalysis(moduleState.globalNameToIndexMap, "globalNameToIndexMap");
			dumpHashMapSpaceAnalysis(moduleState.functionTypeToIndexMap, "functionTypeToIndexMap");
			dumpHashMapSpaceAnalysis(moduleState.typeNameToIndexMap, "typeNameToIndexMap");
		}
	}
	catch(RecoverParseException const&)
	{
		cursor->moduleState = nullptr;
		throw RecoverParseException();
	}
	cursor->moduleState = nullptr;
}

bool WAST::parseModule(const char* string,
					   Uptr stringLength,
					   IR::Module& outModule,
					   std::vector<Error>& outErrors)
{
	Timing::Timer timer;

	// Lex the string.
	LineInfo* lineInfo = nullptr;
	Token* tokens
		= lex(string, stringLength, lineInfo, outModule.featureSpec.allowLegacyOperatorNames);
	ParseState parseState(string, lineInfo);
	CursorState cursor(tokens, &parseState);

	try
	{
		if(cursor.nextToken[0].type == t_leftParenthesis && cursor.nextToken[1].type == t_module)
		{
			// Parse (module <module body>)
			parseParenthesized(&cursor, [&] {
				require(&cursor, t_module);
				parseModuleBody(&cursor, outModule);
			});
		}
		else
		{
			// Also allow a module body without any enclosing (module ...).
			parseModuleBody(&cursor, outModule);
		}
		require(&cursor, t_eof);
	}
	catch(RecoverParseException const&)
	{
	}
	catch(FatalParseException const&)
	{
	}

	// Resolve line information for any errors, and write them to outErrors.
	for(const auto& unresolvedError : parseState.unresolvedErrors)
	{
		TextFileLocus locus = calcLocusFromOffset(string, lineInfo, unresolvedError.charOffset);
		outErrors.push_back({std::move(locus), std::move(unresolvedError.message)});
	}

	// Free the tokens and line info.
	freeTokens(tokens);
	freeLineInfo(lineInfo);

	Timing::logRatePerSecond("lexed and parsed WAST", timer, stringLength / 1024.0 / 1024.0, "MB");

	return outErrors.size() == 0;
}
