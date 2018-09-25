#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Runtime/Runtime.h"

namespace WAVM { namespace Runtime {
	// An abstract resolver: maps module+export name pairs to a Runtime::Object.
	struct Resolver
	{
		virtual bool resolve(const std::string& moduleName,
							 const std::string& exportName,
							 IR::ExternType type,
							 Object*& outObject)
			= 0;
	};

	// A resolver that ignores the moduleName, and looks for the exportName in a single module.
	struct ModuleExportResolver : Resolver
	{
		ModuleExportResolver(const IR::Module& inModule, ModuleInstance* inModuleInstance)
		: module(inModule), moduleInstance(inModuleInstance)
		{
		}

		bool resolve(const std::string& moduleName,
					 const std::string& exportName,
					 IR::ExternType type,
					 Object*& outObject) override;

	private:
		const IR::Module& module;
		ModuleInstance* moduleInstance;
	};

	// A resolver that lazily creates an inner resolver when it's first used, then forwards all
	// queries to the inner resolver.
	struct LazyResolver : Resolver
	{
		LazyResolver(std::function<Resolver*()>& inInnerResolverThunk)
		: innerResolverThunk(std::move(inInnerResolverThunk)), innerResolver(nullptr)
		{
		}

		bool resolve(const std::string& moduleName,
					 const std::string& exportName,
					 IR::ExternType type,
					 Runtime::Object*& outObject) override
		{
			if(!innerResolver) { innerResolver = innerResolverThunk(); }
			return innerResolver->resolve(moduleName, exportName, type, outObject);
		}

	private:
		std::function<Resolver*()> innerResolverThunk;
		Resolver* innerResolver;
	};

	// A resolver that always returns failure.
	struct NullResolver : Resolver
	{
		bool resolve(const std::string& moduleName,
					 const std::string& exportName,
					 IR::ExternType type,
					 Runtime::Object*& outObject) override
		{
			return false;
		}
	};

	// Links a module using the given resolver, returning an array mapping import indices to
	// objects. If the resolver fails to resolve any imports, throws a LinkException.
	struct LinkResult
	{
		struct MissingImport
		{
			std::string moduleName;
			std::string exportName;
			IR::ExternType type;
		};

		std::vector<MissingImport> missingImports;
		ImportBindings resolvedImports;
		bool success;
	};

	RUNTIME_API LinkResult linkModule(const IR::Module& module, Resolver& resolver);
}}