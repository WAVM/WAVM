#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Runtime.h"
#include "RuntimePrivate.h"
#include "Linker.h"
#include "Intrinsics.h"
#include "IR/Module.h"

#include <string.h>

namespace Runtime
{
	const FunctionType* resolveImportType(const IR::Module& module,IndexedFunctionType type)
	{
		return module.types[type.index];
	}
	TableType resolveImportType(const IR::Module& module,TableType type) { return type; }
	MemoryType resolveImportType(const IR::Module& module,MemoryType type) { return type; }
	GlobalType resolveImportType(const IR::Module& module,GlobalType type) { return type; }

	template<typename Instance,typename Type>
	void linkImport(const IR::Module& module,const Import<Type>& import,Resolver& resolver,LinkResult& linkResult,std::vector<Instance*>& resolvedImports)
	{
		// Ask the resolver for a value for this import.
		Object* importValue;
		if(resolver.resolve(import.moduleName,import.exportName,resolveImportType(module,import.type),importValue))
		{
			// Sanity check that the resolver returned an object of the right type.
			wavmAssert(isA(importValue,resolveImportType(module,import.type)));
			resolvedImports.push_back(as<Instance>(importValue));
		}
		else { linkResult.missingImports.push_back({import.moduleName,import.exportName,resolveImportType(module,import.type)}); }
	}

	LinkResult linkModule(const IR::Module& module,Resolver& resolver)
	{
		LinkResult linkResult;
		for(const auto& import : module.functions.imports)
		{
			linkImport(module,import,resolver,linkResult,linkResult.resolvedImports.functions);
		}
		for(const auto& import : module.tables.imports)
		{
			linkImport(module,import,resolver,linkResult,linkResult.resolvedImports.tables);
		}
		for(const auto& import : module.memories.imports)
		{
			linkImport(module,import,resolver,linkResult,linkResult.resolvedImports.memories);
		}
		for(const auto& import : module.globals.imports)
		{
			linkImport(module,import,resolver,linkResult,linkResult.resolvedImports.globals);
		}

		linkResult.success = linkResult.missingImports.size() == 0;
		return linkResult;
	}
}