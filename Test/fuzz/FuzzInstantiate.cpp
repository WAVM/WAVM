#include "IR/Module.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/CLI.h"
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

using namespace IR;
using namespace Runtime;
using namespace WAST;

namespace LLVMJIT
{
	RUNTIME_API void deinit();
}

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
			auto stubModuleInstance = Runtime::instantiateModule(
				compartment, Runtime::compileModule(stubModule), {}, "importStub");
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

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	IR::Module module;
	module.featureSpec.maxLabelsPerFunction = 65536;
	module.featureSpec.maxLocals = 1024;
	if(!loadBinaryModule(data, numBytes, module, Log::debug)) { return 0; }

	Compartment* compartment = createCompartment();
	StubResolver stubResolver(compartment);
	LinkResult linkResult = linkModule(module, stubResolver);
	if(linkResult.success)
	{
		catchRuntimeExceptions(
			[&] {
				instantiateModule(compartment,
								  compileModule(module),
								  std::move(linkResult.resolvedImports),
								  "fuzz");
			},
			[&](Exception&& exception) {});

		collectGarbage();
	}

	// De-initialize LLVM to avoid the accumulation of de-duped debug metadata in the LLVMContext.
	LLVMJIT::deinit();

	return 0;
}

#if !ENABLE_LIBFUZZER
I32 main(int argc, char** argv)
{
	if(argc != 2)
	{
		Log::printf(Log::error, "Usage: FuzzInstantiate in.wasm\n");
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];

	std::vector<U8> wasmBytes;
	if(!loadFile(inputFilename, wasmBytes)) { return EXIT_FAILURE; }

	LLVMFuzzerTestOneInput(wasmBytes.data(), wasmBytes.size());
	return EXIT_SUCCESS;
}
#endif
