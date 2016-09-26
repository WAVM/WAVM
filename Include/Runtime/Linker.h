#pragma once

#include "Core/Core.h"
#include "WebAssembly/Module.h"
#include "Runtime.h"

#include <functional>

namespace Runtime
{
	// An abstract resolver: maps module+export name pairs to a Runtime::Object.
	struct Resolver
	{
		virtual bool resolve(const char* moduleName,const char* exportName,WebAssembly::ObjectType type,Object*& outObject) = 0;
	};

	// A resolver for intrinsics.
	struct IntrinsicResolver : Resolver
	{
		static RUNTIME_API IntrinsicResolver singleton;
		RUNTIME_API bool resolve(const char* moduleName,const char* exportName,WebAssembly::ObjectType type,Object*& outObject) override;
	};

	// A resolver that ignores the moduleName, and looks for the exportName in a single module.
	struct ModuleExportResolver : Resolver
	{
		ModuleExportResolver(const WebAssembly::Module& inModule,ModuleInstance* inModuleInstance): module(inModule), moduleInstance(inModuleInstance) {}

		bool resolve(const char* moduleName,const char* exportName,WebAssembly::ObjectType type,Object*& outObject) override;
	private:
		const WebAssembly::Module& module;
		ModuleInstance* moduleInstance;
	};

	// A resolver that lazily creates an inner resolver when it's first used, then forwards all queries to the inner resolver.
	struct LazyResolver : Resolver
	{
		LazyResolver(std::function<Resolver*()>& inInnerResolverThunk)
		: innerResolverThunk(std::move(inInnerResolverThunk)), innerResolver(nullptr) {}

		bool resolve(const char* moduleName,const char* exportName,WebAssembly::ObjectType type,Runtime::Object*& outObject) override
		{
			if(!innerResolver) { innerResolver = innerResolverThunk(); }
			return innerResolver->resolve(moduleName,exportName,type,outObject);		
		}

	private:

		std::function<Resolver*()> innerResolverThunk;
		Resolver* innerResolver;
	};

	// A resolver that always returns failure.
	struct NullResolver : Resolver
	{
		bool resolve(const char* moduleName,const char* exportName,WebAssembly::ObjectType type,Runtime::Object*& outObject) override
		{
			return false; 
		}
	};

	// An exception that is thrown by linkModule/linkAndInstantiateModule if any imports were missing.
	struct LinkException
	{
		struct MissingImport
		{
			std::string moduleName;
			std::string exportName;
			WebAssembly::ObjectType type;
		};

		std::vector<MissingImport> missingImports;
	};

	// Links a module using the given resolver, returning an array mapping import indices to objects.
	// If the resolver fails to resolve any imports, throws a LinkException.
	RUNTIME_API std::vector<Object*> linkModule(const WebAssembly::Module& module,Resolver& resolver);

	// Links and instantiates a module using the given resolver.
	// If the resolver fails to resolve any imports, throws a LinkException.
	// Calls Runtime::instantiateModule, so may also throw an InstantiationException.
	RUNTIME_API ModuleInstance* linkAndInstantiateModule(const WebAssembly::Module& module,Resolver& resolver);
}