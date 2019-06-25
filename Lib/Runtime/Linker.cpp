#include "WAVM/Runtime/Linker.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Runtime/Runtime.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

template<typename Type, typename ResolvedType>
void linkImport(const IR::Module& module,
				const Import<Type>& import,
				ResolvedType resolvedType,
				Resolver& resolver,
				LinkResult& linkResult)
{
	// Ask the resolver for a value for this import.
	Object* importValue;
	if(resolver.resolve(import.moduleName, import.exportName, resolvedType, importValue))
	{
		// Sanity check that the resolver returned an object of the right type.
		wavmAssert(isA(importValue, resolvedType));
		linkResult.resolvedImports.push_back(importValue);
	}
	else
	{
		linkResult.missingImports.push_back({import.moduleName, import.exportName, resolvedType});
		linkResult.resolvedImports.push_back(nullptr);
	}
}

LinkResult Runtime::linkModule(const IR::Module& module, Resolver& resolver)
{
	LinkResult linkResult;
	wavmAssert(module.imports.size()
			   == module.functions.imports.size() + module.tables.imports.size()
					  + module.memories.imports.size() + module.globals.imports.size()
					  + module.exceptionTypes.imports.size());
	for(const auto& kindIndex : module.imports)
	{
		switch(kindIndex.kind)
		{
		case ExternKind::function:
		{
			const auto& functionImport = module.functions.imports[kindIndex.index];
			linkImport(module,
					   functionImport,
					   module.types[functionImport.type.index],
					   resolver,
					   linkResult);
			break;
		}
		case ExternKind::table:
		{
			const auto& tableImport = module.tables.imports[kindIndex.index];
			linkImport(module, tableImport, tableImport.type, resolver, linkResult);
			break;
		}
		case ExternKind::memory:
		{
			const auto& memoryImport = module.memories.imports[kindIndex.index];
			linkImport(module, memoryImport, memoryImport.type, resolver, linkResult);
			break;
		}
		case ExternKind::global:
		{
			const auto& globalImport = module.globals.imports[kindIndex.index];
			linkImport(module, globalImport, globalImport.type, resolver, linkResult);
			break;
		}
		case ExternKind::exceptionType:
		{
			const auto& exceptionTypeImport = module.exceptionTypes.imports[kindIndex.index];
			linkImport(module, exceptionTypeImport, exceptionTypeImport.type, resolver, linkResult);
			break;
		}

		case ExternKind::invalid:
		default: WAVM_UNREACHABLE();
		};
	}

	linkResult.success = linkResult.missingImports.size() == 0;
	return linkResult;
}
