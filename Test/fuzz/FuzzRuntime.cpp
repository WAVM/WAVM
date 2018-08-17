#include "IR/Module.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Linker.h"
#include "Runtime/Runtime.h"
#include "WASM/WASM.h"
#include "WAST/TestScript.h"
#include "WAST/WAST.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

using namespace WAST;
using namespace IR;
using namespace Runtime;

namespace LLVMJIT
{
	RUNTIME_API void deinit();
}

struct StubResolver : Resolver
{
	Compartment* compartment;

	StubResolver(Compartment* inCompartment)
	: compartment(inCompartment)
	{
	}

	bool resolve(
		const std::string& moduleName,
		const std::string& exportName,
		ObjectType type,
		Object*& outObject) override
	{
		outObject = getStubObject(exportName, type);
		return true;
	}

	Object* getStubObject(const std::string& exportName, ObjectType type) const
	{
		// If the import couldn't be resolved, stub it in.
		switch(type.kind)
		{
		case IR::ObjectKind::function:
		{
			// Generate a function body that just uses the unreachable op to fault if called.
			Serialization::ArrayOutputStream codeStream;
			OperatorEncoderStream encoder(codeStream);
			for(ValueType result : asFunctionType(type).results())
			{
				switch(result)
				{
				case ValueType::i32: encoder.i32_const({0}); break;
				case ValueType::i64: encoder.i64_const({0}); break;
				case ValueType::f32: encoder.f32_const({0.0f}); break;
				case ValueType::f64: encoder.f64_const({0.0}); break;
				case ValueType::v128: encoder.v128_const({V128{{0, 0}}}); break;
				default: Errors::unreachable();
				};
			}
			encoder.end();

			// Generate a module for the stub function.
			Module stubModule;
			DisassemblyNames stubModuleNames;
			stubModule.types.push_back(asFunctionType(type));
			stubModule.functions.defs.push_back({{0}, {}, std::move(codeStream.getBytes()), {}});
			stubModule.exports.push_back({"importStub", IR::ObjectKind::function, 0});
			stubModuleNames.functions.push_back({"importStub: " + exportName, {}, {}});
			setDisassemblyNames(stubModule, stubModuleNames);
			validateDefinitions(stubModule);

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
				Value(asGlobalType(type).valueType, UntaggedValue())));
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

inline bool loadBinaryModule(const std::string& wasmBytes, Module& outModule)
{
	// Load the module from a binary WebAssembly file.
	try
	{
		Serialization::MemoryInputStream stream((const U8*)wasmBytes.data(), wasmBytes.size());
		WASM::serialize(stream, outModule);
		return true;
	}
	catch(Serialization::FatalSerializationException exception)
	{
		return false;
	}
	catch(ValidationException exception)
	{
		return false;
	}
	catch(std::bad_alloc)
	{
		return false;
	}
}

inline bool loadTextModule(const std::string& wastString, IR::Module& outModule)
{
	try
	{
		std::vector<WAST::Error> parseErrors;
		WAST::parseModule(wastString.c_str(), wastString.size(), outModule, parseErrors);
		return !parseErrors.size();
	}
	catch(...)
	{
		Log::printf(Log::Category::error, "unknown exception!\n");
		return false;
	}
}

inline bool loadModule(const std::string& dataString, IR::Module& outModule)
{
	return loadBinaryModule(dataString, outModule);
}

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	Module module;
	if(!loadBinaryModule(std::string((const char*)data, numBytes), module)) { return 0; }

	Compartment* compartment = createCompartment();
	StubResolver stubResolver(compartment);
	LinkResult linkResult = linkModule(module, stubResolver);
	if(linkResult.success)
	{
		catchRuntimeExceptions(
			[&] {
				instantiateModule(
					compartment, module, std::move(linkResult.resolvedImports), "fuzz");
			},
			[&](Exception&& exception) {});

		collectGarbage();
	}

	// De-initialize LLVM to avoid the accumulation of de-duped debug metadata in the LLVMContext.
	LLVMJIT::deinit();

	return 0;
}

#if !ENABLE_LIBFUZZER
I32 main()
{
	LLVMFuzzerTestOneInput((const U8*)"", 0);
	return EXIT_SUCCESS;
}
#endif
