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
	struct WAVM_API Resolver
	{
		virtual ~Resolver() {}
		virtual bool resolve(const std::string& moduleName,
							 const std::string& exportName,
							 IR::ExternType type,
							 Object*& outObject)
			= 0;
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

	// A resolver that generates stubs for objects that the inner resolver can't find.
	struct WAVM_API StubResolver : Resolver
	{
		enum class FunctionBehavior
		{
			zero,
			trap,
		};

		StubResolver(Compartment* inCompartment,
					 FunctionBehavior inFunctionBehavior = FunctionBehavior::trap,
					 bool inLogErrorOnStubGeneration = true,
					 ResourceQuotaRefParam resourceQuota = ResourceQuotaRef());

		virtual bool resolve(const std::string& moduleName,
							 const std::string& exportName,
							 IR::ExternType type,
							 Runtime::Object*& outObject) override;

	private:
		GCPointer<Compartment> compartment;
		ResourceQuotaRef resourceQuota;
		FunctionBehavior functionBehavior;
		bool logErrorOnStubGeneration;
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
		bool success{false};
	};

	WAVM_API LinkResult linkModule(const IR::Module& module, Resolver& resolver);
}}
