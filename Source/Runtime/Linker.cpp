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
		const std::string decoratedName = Intrinsics::getDecoratedName((std::string(moduleName) + "." + exportName).c_str(),type);
		outObject = Intrinsics::find(decoratedName.c_str(),type);
		return outObject != nullptr;
	}

	std::vector<Object*> linkModule(const WebAssembly::Module& module,Resolver& resolver)
	{
		std::vector<Object*> imports;
		std::vector<Import> missingImports;
		for(const Import& import : module.imports)
		{
			Object* importValue;
			const ObjectType objectType = resolveImportType(module,import.type);
			if(!resolver.resolve(import.module.c_str(),import.exportName.c_str(),objectType,importValue)) { importValue = nullptr; }
			if(importValue && !isA(importValue,objectType)) { importValue = nullptr; }
			if(importValue) { imports.push_back(importValue); }
			else { missingImports.push_back(import); }
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