#include "Core/Core.h"
#include "Runtime.h"
#include "Core/Platform.h"
#include "RuntimePrivate.h"
#include "WebAssembly/WebAssembly.h"
#include "Linker.h"
#include "Intrinsics.h"

namespace Runtime
{
	RUNTIME_API IntrinsicResolver IntrinsicResolver::singleton;

	bool IntrinsicResolver::resolve(const char* moduleName,const char* exportName,ObjectType type,Object*& outObject)
	{
		// Make sure the wavmIntrinsics module can't be directly imported.
		if(!strcmp(moduleName,"wavmIntrinsics")) { return false; }

		outObject = Intrinsics::find((std::string(moduleName) + "." + exportName).c_str(),type);
		return outObject != nullptr;
	}

	std::vector<Object*> linkModule(const WebAssembly::Module& module,Resolver& resolver)
	{
		std::vector<Object*> imports;
		std::vector<LinkException::MissingImport> missingImports;
		for(const Import& import : module.imports)
		{
			const ObjectType objectType = resolveImportType(module,import.type);

			// Ask the resolver for a value for this import.
			Object* importValue;
			if(resolver.resolve(import.module.c_str(),import.exportName.c_str(),objectType,importValue))
			{
				// Sanity check that the resolver returned an object of the right type.
				errorUnless(isA(importValue,objectType));
				imports.push_back(importValue);
			}
			else { missingImports.push_back({import.module,import.exportName,objectType}); }
		}

		if(missingImports.size()) { throw LinkException {std::move(missingImports)}; }
		else { return imports; }
	}

	ModuleInstance* linkAndInstantiateModule(const Module& module,Resolver& resolver)
	{
		std::vector<Object*> imports = linkModule(module,resolver);
		return instantiateModule(module,std::move(imports));
	}
}