#include "WAVM/Runtime/Linker.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Runtime/Runtime.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

static IR::FunctionType resolveImportType(const IR::Module& module, IR::IndexedFunctionType type)
{
	return module.types[type.index];
}
static IR::TableType resolveImportType(const IR::Module& module, IR::TableType type)
{
	return type;
}
static IR::MemoryType resolveImportType(const IR::Module& module, IR::MemoryType type)
{
	return type;
}
static IR::GlobalType resolveImportType(const IR::Module& module, IR::GlobalType type)
{
	return type;
}
static IR::ExceptionType resolveImportType(const IR::Module& module, IR::ExceptionType type)
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
