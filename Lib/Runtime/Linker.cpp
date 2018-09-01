#include "Runtime/Linker.h"
#include "IR/Module.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Runtime.h"
#include "RuntimePrivate.h"

#include <string.h>

using namespace IR;
using namespace Runtime;

static FunctionType resolveImportType(const IR::Module& module, IndexedFunctionType type)
{
	return module.types[type.index];
}
static TableType resolveImportType(const IR::Module& module, TableType type) { return type; }
static MemoryType resolveImportType(const IR::Module& module, MemoryType type) { return type; }
static GlobalType resolveImportType(const IR::Module& module, GlobalType type) { return type; }
static ExceptionType resolveImportType(const IR::Module& module, ExceptionType type)
{
	return type;
}

template<typename Instance, typename Type>
void linkImport(const IR::Module& module,
				const Import<Type>& import,
				Resolver& resolver,
				LinkResult& linkResult,
				std::vector<Instance*>& resolvedImports)
{
	// Ask the resolver for a value for this import.
	Object* importValue;
	if(resolver.resolve(import.moduleName,
						import.exportName,
						resolveImportType(module, import.type),
						importValue))
	{
		// Sanity check that the resolver returned an object of the right type.
		wavmAssert(isA(importValue, resolveImportType(module, import.type)));
		resolvedImports.push_back(as<Instance>(importValue));
	}
	else
	{
		linkResult.missingImports.push_back(
			{import.moduleName, import.exportName, resolveImportType(module, import.type)});
	}
}

LinkResult Runtime::linkModule(const IR::Module& module, Resolver& resolver)
{
	LinkResult linkResult;
	for(const auto& import : module.functions.imports)
	{ linkImport(module, import, resolver, linkResult, linkResult.resolvedImports.functions); }
	for(const auto& import : module.tables.imports)
	{ linkImport(module, import, resolver, linkResult, linkResult.resolvedImports.tables); }
	for(const auto& import : module.memories.imports)
	{ linkImport(module, import, resolver, linkResult, linkResult.resolvedImports.memories); }
	for(const auto& import : module.globals.imports)
	{ linkImport(module, import, resolver, linkResult, linkResult.resolvedImports.globals); }
	for(const auto& import : module.exceptionTypes.imports)
	{
		linkImport(module, import, resolver, linkResult, linkResult.resolvedImports.exceptionTypes);
	}

	linkResult.success = linkResult.missingImports.size() == 0;
	return linkResult;
}
