#include "Inline/BasicTypes.h"
#include "Inline/Timing.h"
#include "IR/Module.h"
#include "IR/Validate.h"
#include "Lexer.h"
#include "Logging/Logging.h"
#include "Parse.h"
#include "WAST.h"

using namespace WAST;
using namespace IR;

static bool tryParseSizeConstraints(CursorState* cursor,U64 maxMax,SizeConstraints& outSizeConstraints)
{
	outSizeConstraints.min = 0;
	outSizeConstraints.max = UINT64_MAX;

	// Parse a minimum.
	if(!tryParseI64(cursor,outSizeConstraints.min))
	{
		return false;
	}
	else
	{
		// Parse an optional maximum.
		if(!tryParseI64(cursor,outSizeConstraints.max)) { outSizeConstraints.max = UINT64_MAX; }
		else
		{
			// Validate that the maximum size is within the limit, and that the size contraints is not disjoint.
			if(outSizeConstraints.max > maxMax)
			{
				parseErrorf(cursor->parseState,cursor->nextToken-1,"maximum size exceeds limit (%u>%u)",outSizeConstraints.max,maxMax);
				outSizeConstraints.max = maxMax;
			}
			else if(outSizeConstraints.max < outSizeConstraints.min)
			{
				parseErrorf(cursor->parseState,cursor->nextToken-1,"maximum size is less than minimum size (%u<%u)",outSizeConstraints.max,outSizeConstraints.min);
				outSizeConstraints.max = outSizeConstraints.min;
			}
		}

		return true;
	}
}

static SizeConstraints parseSizeConstraints(CursorState* cursor,U64 maxMax)
{
	SizeConstraints result;
	if(!tryParseSizeConstraints(cursor,maxMax,result))
	{
		parseErrorf(cursor->parseState,cursor->nextToken,"expected size constraints");
	}
	return result;
}

static GlobalType parseGlobalType(CursorState* cursor)
{
	GlobalType result;
	result.isMutable = tryParseParenthesizedTagged(cursor,t_mut,[&]
	{
		result.valueType = parseValueType(cursor);
	});
	if(!result.isMutable)
	{
		result.valueType = parseValueType(cursor);
	}
	return result;
}

static const TupleType* parseTupleType(CursorState* cursor)
{
	std::vector<ValueType> parameters;
	ValueType elementType;
	while(tryParseValueType(cursor,elementType)) { parameters.push_back(elementType); }
	return TupleType::get(parameters);
}

static InitializerExpression parseInitializerExpression(CursorState* cursor)
{
	InitializerExpression result;
	parseParenthesized(cursor,[&]
	{
		switch(cursor->nextToken->type)
		{
		case t_i32_const: { ++cursor->nextToken; result = (I32)parseI32(cursor); break; }
		case t_i64_const: { ++cursor->nextToken; result = (I64)parseI64(cursor); break; }
		case t_f32_const: { ++cursor->nextToken; result = parseF32(cursor); break; }
		case t_f64_const: { ++cursor->nextToken; result = parseF64(cursor); break; }
		case t_get_global:
		{
			++cursor->nextToken;
			result.type = InitializerExpression::Type::get_global;
			result.globalIndex = parseAndResolveNameOrIndexRef(
				cursor,
				cursor->moduleState->globalNameToIndexMap,
				cursor->moduleState->module.globals.size(),
				"global"
				);
			break;
		}
		default:
			parseErrorf(cursor->parseState,cursor->nextToken,"expected initializer expression");
			result.type = InitializerExpression::Type::error;
			throw RecoverParseException();
		};
	});

	return result;
}

static void errorIfFollowsDefinitions(CursorState* cursor)
{
	if(cursor->moduleState->module.functions.defs.size()
	|| cursor->moduleState->module.tables.defs.size()
	|| cursor->moduleState->module.memories.defs.size()
	|| cursor->moduleState->module.globals.defs.size())
	{
		parseErrorf(cursor->parseState,cursor->nextToken,"import declarations must precede all definitions");
	}
}

template<typename Def,typename Type,typename DisassemblyName>
static Uptr createImport(
	CursorState* cursor,Name name,std::string&& moduleName,std::string&& exportName,
	NameToIndexMap& nameToIndexMap,IndexSpace<Def,Type>& indexSpace,std::vector<DisassemblyName>& disassemblyNameArray,
	Type type)
{
	const Uptr importIndex = indexSpace.imports.size();
	bindName(cursor->parseState,nameToIndexMap,name,indexSpace.size());
	disassemblyNameArray.push_back({name.getString()});
	indexSpace.imports.push_back({type,std::move(moduleName),std::move(exportName)});
	return importIndex;
}

static bool parseOptionalSharedDeclaration(CursorState* cursor)
{
	if(cursor->nextToken->type == t_shared) { ++cursor->nextToken; return true; }
	else { return false; }
}

static void parseImport(CursorState* cursor)
{
	errorIfFollowsDefinitions(cursor);

	require(cursor,t_import);

	std::string moduleName = parseUTF8String(cursor);
	std::string exportName = parseUTF8String(cursor);

	parseParenthesized(cursor,[&]
	{
		// Parse the import kind.
		const Token* importKindToken = cursor->nextToken;
		switch(importKindToken->type)
		{
		case t_func:
		case t_table:
		case t_memory:
		case t_global:
		case t_exception_type:
			++cursor->nextToken;
			break;
		default:
			parseErrorf(cursor->parseState,cursor->nextToken,"invalid import type");
			throw RecoverParseException();
		}
		
		// Parse an optional internal name for the import.
		Name name;
		tryParseName(cursor,name);

		// Parse the import type and create the import in the appropriate name/index spaces.
		switch(importKindToken->type)
		{
		case t_func:
		{
			NameToIndexMap localNameToIndexMap;
			std::vector<std::string> localDissassemblyNames;
			const UnresolvedFunctionType unresolvedFunctionType = parseFunctionTypeRefAndOrDecl(cursor,localNameToIndexMap,localDissassemblyNames);
			const Uptr importIndex = createImport(
				cursor,name,
				std::move(moduleName),
				std::move(exportName),
				cursor->moduleState->functionNameToIndexMap,
				cursor->moduleState->module.functions,
				cursor->moduleState->disassemblyNames.functions,
				{UINT32_MAX});
			cursor->moduleState->disassemblyNames.functions.back().locals = localDissassemblyNames;

			// Resolve the function import type after all type declarations have been parsed.
			cursor->moduleState->postTypeCallbacks.push_back([unresolvedFunctionType,importIndex](ModuleState* moduleState)
			{
				moduleState->module.functions.imports[importIndex].type = resolveFunctionType(moduleState,unresolvedFunctionType);
			});
			break;
		}
		case t_table:
		{
			const SizeConstraints sizeConstraints = parseSizeConstraints(cursor,IR::maxTableElems);
			const bool isShared = parseOptionalSharedDeclaration(cursor);
			const TableElementType elementType = TableElementType::anyfunc;
			require(cursor,t_anyfunc);
			createImport(cursor,name,std::move(moduleName),std::move(exportName),
				cursor->moduleState->tableNameToIndexMap,
				cursor->moduleState->module.tables,
				cursor->moduleState->disassemblyNames.tables,
				{elementType,isShared,sizeConstraints});
			break;
		}
		case t_memory:
		{
			const SizeConstraints sizeConstraints = parseSizeConstraints(cursor,IR::maxMemoryPages);
			const bool isShared = parseOptionalSharedDeclaration(cursor);
			createImport(cursor,name,std::move(moduleName),std::move(exportName),
				cursor->moduleState->memoryNameToIndexMap,
				cursor->moduleState->module.memories,
				cursor->moduleState->disassemblyNames.memories,
				MemoryType{isShared,sizeConstraints});
			break;
		}
		case t_global:
		{
			const GlobalType globalType = parseGlobalType(cursor);
			createImport(cursor,name,std::move(moduleName),std::move(exportName),
				cursor->moduleState->globalNameToIndexMap,
				cursor->moduleState->module.globals,
				cursor->moduleState->disassemblyNames.globals,
				globalType);
			break;
		}
		case t_exception_type:
		{
			const TupleType* tupleType = parseTupleType(cursor);
			createImport(cursor,name,std::move(moduleName),std::move(exportName),
				cursor->moduleState->exceptionTypeNameToIndexMap,
				cursor->moduleState->module.exceptionTypes,
				cursor->moduleState->disassemblyNames.exceptionTypes,
				tupleType);
			break;
		}
		default: Errors::unreachable();
		};
	});
}

static void parseExport(CursorState* cursor)
{
	require(cursor,t_export);

	const std::string exportName = parseUTF8String(cursor);

	parseParenthesized(cursor,[&]
	{
		ObjectKind exportKind;
		switch(cursor->nextToken->type)
		{
		case t_func: exportKind = ObjectKind::function; break;
		case t_table: exportKind = ObjectKind::table; break;
		case t_memory: exportKind = ObjectKind::memory; break;
		case t_global: exportKind = ObjectKind::global; break;
		default:
			parseErrorf(cursor->parseState,cursor->nextToken,"invalid export kind");
			throw RecoverParseException();
		};
		++cursor->nextToken;
	
		Reference exportRef;
		if(!tryParseNameOrIndexRef(cursor,exportRef))
		{
			parseErrorf(cursor->parseState,cursor->nextToken,"expected name or index");
			throw RecoverParseException();
		}

		const Uptr exportIndex = cursor->moduleState->module.exports.size();
		cursor->moduleState->module.exports.push_back({std::move(exportName),exportKind,0});

		cursor->moduleState->postDeclarationCallbacks.push_back([=](ModuleState* moduleState)
		{
			Uptr& exportedObjectIndex = moduleState->module.exports[exportIndex].index;
			switch(exportKind)
			{
			case ObjectKind::function: exportedObjectIndex = resolveRef(moduleState->parseState,moduleState->functionNameToIndexMap,moduleState->module.functions.size(),exportRef); break;
			case ObjectKind::table:    exportedObjectIndex = resolveRef(moduleState->parseState,moduleState->tableNameToIndexMap,   moduleState->module.tables.size(),exportRef); break;
			case ObjectKind::memory:   exportedObjectIndex = resolveRef(moduleState->parseState,moduleState->memoryNameToIndexMap,  moduleState->module.memories.size(),exportRef); break;
			case ObjectKind::global:   exportedObjectIndex = resolveRef(moduleState->parseState,moduleState->globalNameToIndexMap,  moduleState->module.globals.size(),exportRef); break;
			default:
				Errors::unreachable();
			}
		});
	});
}

static void parseType(CursorState* cursor)
{
	require(cursor,t_type);

	Name name;
	tryParseName(cursor,name);

	parseParenthesized(cursor,[&]
	{
		require(cursor,t_func);
		
		NameToIndexMap parameterNameToIndexMap;
		std::vector<std::string> localDisassemblyNames;
		const FunctionType* functionType = parseFunctionType(cursor,parameterNameToIndexMap,localDisassemblyNames);

		errorUnless(cursor->moduleState->module.types.size() < UINT32_MAX);
		const U32 functionTypeIndex = U32(cursor->moduleState->module.types.size());
		cursor->moduleState->module.types.push_back(functionType);
		cursor->moduleState->functionTypeToIndexMap.set(functionType,functionTypeIndex);

		bindName(cursor->parseState,cursor->moduleState->typeNameToIndexMap,name,functionTypeIndex);
		cursor->moduleState->disassemblyNames.types.push_back(name.getString());
	});
}

static void parseData(CursorState* cursor)
{
	const Token* firstToken = cursor->nextToken;
	require(cursor,t_data);

	// Parse an optional memory name.
	Reference memoryRef;
	bool hasMemoryRef = tryParseNameOrIndexRef(cursor,memoryRef);

	// Parse an initializer expression for the base address of the data.
	InitializerExpression baseAddress;
	if(cursor->nextToken[0].type == t_leftParenthesis
	&& cursor->nextToken[1].type == t_offset)
	{
		// The initializer expression can optionally be wrapped in (offset ...)
		parseParenthesized(cursor,[&]
		{
			require(cursor,t_offset);
			baseAddress = parseInitializerExpression(cursor);
		});
	}
	else
	{
		baseAddress = parseInitializerExpression(cursor);
	}

	// Parse a list of strings that contains the segment's data.
	std::string dataString;
	while(tryParseString(cursor,dataString)) {};
	
	// Create the data segment.
	std::vector<U8> dataVector((const U8*)dataString.data(),(const U8*)dataString.data() + dataString.size());
	const Uptr dataSegmentIndex = cursor->moduleState->module.dataSegments.size();
	cursor->moduleState->module.dataSegments.push_back({UINTPTR_MAX,baseAddress,std::move(dataVector)});

	// Enqueue a callback that is called after all declarations are parsed to resolve the memory to put the data segment in.
	cursor->moduleState->postDeclarationCallbacks.push_back([=](ModuleState* moduleState)
	{
		if(!moduleState->module.memories.size())
		{
			parseErrorf(moduleState->parseState,firstToken,"data segments aren't allowed in modules without any memory declarations");
		}
		else
		{
			moduleState->module.dataSegments[dataSegmentIndex].memoryIndex =
				hasMemoryRef
					? resolveRef(
						moduleState->parseState,
						moduleState->memoryNameToIndexMap,
						moduleState->module.memories.size(),
						memoryRef)
					: 0;
		}
	});
}

static Uptr parseElemSegmentBody(CursorState* cursor,Reference tableRef,InitializerExpression baseIndex,const Token* elemToken)
{
	// Allocate the elementReferences array on the heap so it doesn't need to be copied for the post-declaration callback.
	std::vector<Reference>* elementReferences = new std::vector<Reference>();
	
	Reference elementRef;
	while(tryParseNameOrIndexRef(cursor,elementRef))
	{
		elementReferences->push_back(elementRef);
	};
	
	// Create the table segment.
	const Uptr tableSegmentIndex = cursor->moduleState->module.tableSegments.size();
	cursor->moduleState->module.tableSegments.push_back({UINTPTR_MAX,baseIndex,std::vector<Uptr>()});

	// Enqueue a callback that is called after all declarations are parsed to resolve the table elements' references.
	cursor->moduleState->postDeclarationCallbacks.push_back([tableRef,tableSegmentIndex,elementReferences,elemToken](ModuleState* moduleState)
	{
		if(!moduleState->module.tables.size())
		{
			parseErrorf(moduleState->parseState,elemToken,"data segments aren't allowed in modules without any memory declarations");
		}
		else
		{
			TableSegment& tableSegment = moduleState->module.tableSegments[tableSegmentIndex];
			tableSegment.tableIndex = tableRef
				? resolveRef(
					moduleState->parseState,
					moduleState->tableNameToIndexMap,
					moduleState->module.tables.size(),
					tableRef)
				: 0;

			tableSegment.indices.resize(elementReferences->size());
			for(Uptr elementIndex = 0;elementIndex < elementReferences->size();++elementIndex)
			{
				tableSegment.indices[elementIndex] = resolveRef(
					moduleState->parseState,
					moduleState->functionNameToIndexMap,
					moduleState->module.functions.size(),
					(*elementReferences)[elementIndex]);
			}
		}
		
		// Free the elementReferences array that was allocated on the heap.
		delete elementReferences;
	});

	return elementReferences->size();
}

static void parseElem(CursorState* cursor)
{
	const Token* elemToken = cursor->nextToken;
	require(cursor,t_elem);

	// Parse an optional table name.
	Reference tableRef;
	tryParseNameOrIndexRef(cursor,tableRef);

	// Parse an initializer expression for the base index of the table segment.
	InitializerExpression baseIndex;
	if(cursor->nextToken[0].type == t_leftParenthesis
		&& cursor->nextToken[1].type == t_offset)
	{
		// The initializer expression can optionally be wrapped in (offset ...)
		parseParenthesized(cursor,[&]
		{
			require(cursor,t_offset);
			baseIndex = parseInitializerExpression(cursor);
		});
	}
	else
	{
		baseIndex = parseInitializerExpression(cursor);
	}

	parseElemSegmentBody(cursor,tableRef,baseIndex,elemToken);
}

template<typename Def,typename Type,typename ParseImport,typename ParseDef,typename DisassemblyName>
static void parseObjectDefOrImport(
	CursorState* cursor,
	NameToIndexMap& nameToIndexMap,
	IR::IndexSpace<Def,Type>& indexSpace,
	std::vector<DisassemblyName>& disassemblyNameArray,
	TokenType declarationTag,
	IR::ObjectKind kind,
	ParseImport parseImportFunc,
	ParseDef parseDefFunc)
{
	const Token* declarationTagToken = cursor->nextToken;
	require(cursor,declarationTag);

	Name name;
	tryParseName(cursor,name);
	
	// Handle inline export declarations.
	while(true)
	{
		const bool isExport = tryParseParenthesizedTagged(cursor,t_export,[&]
		{
			cursor->moduleState->module.exports.push_back({parseUTF8String(cursor),kind,indexSpace.size()});
		});
		if(!isExport) { break; }
	};

	// Handle an inline import declaration.
	std::string importModuleName;
	std::string exportName;
	const bool isImport = tryParseParenthesizedTagged(cursor,t_import,[&]
	{
		errorIfFollowsDefinitions(cursor);

		importModuleName = parseUTF8String(cursor);
		exportName = parseUTF8String(cursor);
	});
	if(isImport)
	{
		Type importType = parseImportFunc(cursor);
		createImport(cursor,name,std::move(importModuleName),std::move(exportName),
			nameToIndexMap,indexSpace,disassemblyNameArray,
			importType);
	}
	else
	{
		Def def = parseDefFunc(cursor,declarationTagToken);
		bindName(cursor->parseState,nameToIndexMap,name,indexSpace.size());
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
		ObjectKind::function,
		[&](CursorState* cursor)
		{
			// Parse the imported function's type.
			NameToIndexMap localNameToIndexMap;
			std::vector<std::string> localDisassemblyNames;
			const UnresolvedFunctionType unresolvedFunctionType = parseFunctionTypeRefAndOrDecl(cursor,localNameToIndexMap,localDisassemblyNames);

			// Resolve the function import type after all type declarations have been parsed.
			const Uptr importIndex = cursor->moduleState->module.functions.imports.size();
			cursor->moduleState->postTypeCallbacks.push_back([unresolvedFunctionType,importIndex](ModuleState* moduleState)
			{
				moduleState->module.functions.imports[importIndex].type = resolveFunctionType(moduleState,unresolvedFunctionType);
			});
			return IndexedFunctionType {UINT32_MAX};
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
		ObjectKind::table,
		// Parse a table import.
		[](CursorState* cursor)
		{
			const SizeConstraints sizeConstraints = parseSizeConstraints(cursor,IR::maxTableElems);
			const bool isShared = parseOptionalSharedDeclaration(cursor);
			const TableElementType elementType = TableElementType::anyfunc;
			require(cursor,t_anyfunc);
			return TableType {elementType,isShared,sizeConstraints};
		},
		// Parse a table definition.
		[](CursorState* cursor,const Token*)
		{
			// Parse the table type.
			SizeConstraints sizeConstraints;
			const bool hasSizeConstraints = tryParseSizeConstraints(cursor,IR::maxTableElems,sizeConstraints);
			const bool isShared = parseOptionalSharedDeclaration(cursor);
		
			const TableElementType elementType = TableElementType::anyfunc;
			require(cursor,t_anyfunc);

			// If we couldn't parse an explicit size constraints, the table definition must contain an table segment that implicitly defines the size.
			if(!hasSizeConstraints)
			{
				parseParenthesized(cursor,[&]
				{
					require(cursor,t_elem);

					const Uptr tableIndex = cursor->moduleState->module.tables.size();
					errorUnless(tableIndex < UINT32_MAX);
					const Uptr numElements = parseElemSegmentBody(cursor,Reference(U32(tableIndex)),InitializerExpression((I32)0),cursor->nextToken-1);
					sizeConstraints.min = sizeConstraints.max = numElements;
				});
			}
			
			return TableDef {TableType(elementType,isShared,sizeConstraints)};
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
		ObjectKind::memory,
		// Parse a memory import.
		[](CursorState* cursor)
		{
			const SizeConstraints sizeConstraints = parseSizeConstraints(cursor,IR::maxMemoryPages);
			const bool isShared = parseOptionalSharedDeclaration(cursor);
			return MemoryType {isShared,sizeConstraints};
		},
		// Parse a memory definition
		[](CursorState* cursor,const Token*)
		{
			SizeConstraints sizeConstraints;
			if(!tryParseSizeConstraints(cursor,IR::maxMemoryPages,sizeConstraints))
			{
				std::string dataString;

				parseParenthesized(cursor,[&]
				{
					require(cursor,t_data);
				
					while(tryParseString(cursor,dataString)) {};
				});

				std::vector<U8> dataVector((const U8*)dataString.data(),(const U8*)dataString.data() + dataString.size());
				sizeConstraints.min = sizeConstraints.max = (dataVector.size() + IR::numBytesPerPage - 1) / IR::numBytesPerPage;
				cursor->moduleState->module.dataSegments.push_back({cursor->moduleState->module.memories.size(),InitializerExpression(I32(0)),std::move(dataVector)});
			}

			const bool isShared = parseOptionalSharedDeclaration(cursor);
			return MemoryDef {MemoryType(isShared,sizeConstraints)};
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
		ObjectKind::global,
		// Parse a global import.
		parseGlobalType,
		// Parse a global definition
		[](CursorState* cursor,const Token*)
		{
			const GlobalType globalType = parseGlobalType(cursor);
			const InitializerExpression initializerExpression = parseInitializerExpression(cursor);
			return GlobalDef {globalType,initializerExpression};
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
		ObjectKind::exceptionType,
		// Parse a global import.
		parseTupleType,
		// Parse a global definition
		[](CursorState* cursor,const Token*)
		{
			const TupleType* tupleType = parseTupleType(cursor);
			return ExceptionTypeDef {tupleType};
		});
}

static void parseStart(CursorState* cursor)
{
	require(cursor,t_start);

	Reference functionRef;
	if(!tryParseNameOrIndexRef(cursor,functionRef))
	{
		parseErrorf(cursor->parseState,cursor->nextToken,"expected function name or index");
	}

	cursor->moduleState->postDeclarationCallbacks.push_back([functionRef](ModuleState* moduleState)
	{
		moduleState->module.startFunctionIndex = resolveRef(
			moduleState->parseState,
			moduleState->functionNameToIndexMap,
			moduleState->module.functions.size(),
			functionRef);
	});
}

static void parseDeclaration(CursorState* cursor)
{
	parseParenthesized(cursor,[&]
	{
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
			parseErrorf(cursor->parseState,cursor->nextToken,"unrecognized definition in module");
			throw RecoverParseException();
		};
	});
}

template<typename Map>
void dumpHashMapSpaceAnalysis(const Map& map, const char* description)
{
	Uptr totalMemoryBytes = 0;
	Uptr maxProbeCount = 0;
	F32 occupancy = 0.0f;
	F32 averageProbeCount = 0.0f;
	map.analyzeSpaceUsage(totalMemoryBytes, maxProbeCount, occupancy, averageProbeCount);
	Log::printf(
		Log::Category::metrics,
		"%s used %.1fKB for %u elements (%.0f%% occupancy, %.1f bytes/element). Avg/max probe length: %f/%u\n",
		description,
		totalMemoryBytes / 1024.0f,
		map.num(),
		occupancy * 100.0f,
		F32(totalMemoryBytes) / map.num(),
		averageProbeCount,
		maxProbeCount
		);
}

namespace WAST
{
	void parseModuleBody(CursorState* cursor,IR::Module& outModule)
	{
		try
		{
			const Token* firstToken = cursor->nextToken;
			ModuleState moduleState(cursor->parseState,outModule);
			cursor->moduleState = &moduleState;

			// Parse the module's declarations.
			while(cursor->nextToken->type != t_rightParenthesis)
			{
				parseDeclaration(cursor);
			};

			// Process the callbacks requested after all type declarations have been parsed.
			if(!cursor->parseState->unresolvedErrors.size())
			{
				for(const auto& callback : cursor->moduleState->postTypeCallbacks)
				{
					callback(&moduleState);
				}
			}

			// Process the callbacks requested after all declarations have been parsed.
			if(!cursor->parseState->unresolvedErrors.size())
			{
				for(const auto& callback : cursor->moduleState->postDeclarationCallbacks)
				{
					callback(&moduleState);
				}
			}
		
			// Validate the module's definitions (excluding function code, which is validated as it is parsed).
			if(!cursor->parseState->unresolvedErrors.size())
			{
				try
				{
					IR::validateDefinitions(outModule);
				}
				catch(ValidationException validationException)
				{
					parseErrorf(cursor->parseState,firstToken,"validation exception: %s",validationException.message.c_str());
				}
			}

			// Set the module's disassembly names.
			wavmAssert(outModule.functions.size() == moduleState.disassemblyNames.functions.size());
			wavmAssert(outModule.tables.size() == moduleState.disassemblyNames.tables.size());
			wavmAssert(outModule.memories.size() == moduleState.disassemblyNames.memories.size());
			wavmAssert(outModule.globals.size() == moduleState.disassemblyNames.globals.size());
			IR::setDisassemblyNames(outModule,moduleState.disassemblyNames);
			
			// If metrics logging is enabled, log some statistics about the module's name maps.
			if(Log::isCategoryEnabled(Log::Category::metrics))
			{
				dumpHashMapSpaceAnalysis(moduleState.functionNameToIndexMap, "functionNameToIndexMap");
				dumpHashMapSpaceAnalysis(moduleState.globalNameToIndexMap, "globalNameToIndexMap");
				dumpHashMapSpaceAnalysis(moduleState.functionTypeToIndexMap, "functionTypeToIndexMap");
				dumpHashMapSpaceAnalysis(moduleState.typeNameToIndexMap, "typeNameToIndexMap");
			}
		}
		catch(RecoverParseException)
		{
			cursor->moduleState = nullptr;
			throw RecoverParseException();
		}
		cursor->moduleState = nullptr;
	}

	bool parseModule(const char* string,Uptr stringLength,IR::Module& outModule,std::vector<Error>& outErrors)
	{
		Timing::Timer timer;
		
		// Lex the string.
		LineInfo* lineInfo = nullptr;
		Token* tokens = lex(string,stringLength,lineInfo);
		ParseState parseState(string,lineInfo);
		CursorState cursor(tokens,&parseState);

		try
		{
			// Parse (module ...)<eof>
			parseParenthesized(&cursor,[&]
			{
				require(&cursor,t_module);
				parseModuleBody(&cursor,outModule);
			});
			require(&cursor,t_eof);
		}
		catch(RecoverParseException) {}
		catch(FatalParseException) {}

		// Resolve line information for any errors, and write them to outErrors.
		for(const auto& unresolvedError : parseState.unresolvedErrors)
		{
			TextFileLocus locus = calcLocusFromOffset(string,lineInfo,unresolvedError.charOffset);
			outErrors.push_back({std::move(locus),std::move(unresolvedError.message)});
		}

		// Free the tokens and line info.
		freeTokens(tokens);
		freeLineInfo(lineInfo);

		Timing::logRatePerSecond("lexed and parsed WAST",timer,stringLength / 1024.0 / 1024.0,"MB");

		return outErrors.size() == 0;
	}
}