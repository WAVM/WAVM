#pragma once

#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/TaggedValue.h"
#include "IR/Types.h"
#include "IR/Validate.h"
#include "Runtime/Linker.h"
#include "Runtime/Runtime.h"

struct StubResolver : Runtime::Resolver
{
	Runtime::Compartment* compartment;

	StubResolver(Runtime::Compartment* inCompartment) : compartment(inCompartment) {}

	bool resolve(const std::string& moduleName,
				 const std::string& exportName,
				 IR::ObjectType type,
				 Runtime::Object*& outObject) override
	{
		outObject = getStubObject(exportName, type);
		return true;
	}

	Runtime::Object* getStubObject(const std::string& exportName, IR::ObjectType type) const
	{
		// If the import couldn't be resolved, stub it in.
		switch(type.kind)
		{
		case IR::ObjectKind::function:
		{
			// Generate a function body that just uses the unreachable op to fault if called.
			Serialization::ArrayOutputStream codeStream;
			IR::OperatorEncoderStream encoder(codeStream);
			for(IR::ValueType result : asFunctionType(type).results())
			{
				switch(result)
				{
				case IR::ValueType::i32: encoder.i32_const({0}); break;
				case IR::ValueType::i64: encoder.i64_const({0}); break;
				case IR::ValueType::f32: encoder.f32_const({0.0f}); break;
				case IR::ValueType::f64: encoder.f64_const({0.0}); break;
				case IR::ValueType::v128: encoder.v128_const({V128{{0, 0}}}); break;
				default: Errors::unreachable();
				};
			}
			encoder.end();

			// Generate a module for the stub function.
			IR::Module stubModule;
			IR::DisassemblyNames stubModuleNames;
			stubModule.types.push_back(asFunctionType(type));
			stubModule.functions.defs.push_back({{0}, {}, std::move(codeStream.getBytes()), {}});
			stubModule.exports.push_back({"importStub", IR::ObjectKind::function, 0});
			stubModuleNames.functions.push_back({"importStub: " + exportName, {}, {}});
			IR::setDisassemblyNames(stubModule, stubModuleNames);
			IR::validateDefinitions(stubModule);

			// Instantiate the module and return the stub function instance.
			auto stubModuleInstance = instantiateModule(compartment, stubModule, {}, "importStub");
			return getInstanceExport(stubModuleInstance, "importStub");
		}
		case IR::ObjectKind::memory:
		{
			return asObject(Runtime::createMemory(compartment, asMemoryType(type)));
		}
		case IR::ObjectKind::table:
		{
			return asObject(Runtime::createTable(compartment, asTableType(type)));
		}
		case IR::ObjectKind::global:
		{
			return asObject(Runtime::createGlobal(
				compartment,
				asGlobalType(type),
				IR::Value(asGlobalType(type).valueType, IR::UntaggedValue())));
		}
		case IR::ObjectKind::exceptionType:
		{
			return asObject(
				Runtime::createExceptionTypeInstance(asExceptionType(type), "importStub"));
		}
		default: Errors::unreachable();
		};
	}
};
