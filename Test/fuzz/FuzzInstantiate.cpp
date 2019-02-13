#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/WASM/WASM.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;
using namespace WAVM::WASM;

struct StubResolver : Runtime::Resolver
{
	Compartment* compartment;

	StubResolver(Compartment* inCompartment) : compartment(inCompartment) {}

	bool resolve(const std::string& moduleName,
				 const std::string& exportName,
				 IR::ExternType type,
				 Runtime::Object*& outObject) override
	{
		outObject = getStubObject(exportName, type);
		return true;
	}

	Runtime::Object* getStubObject(const std::string& exportName, IR::ExternType type) const
	{
		// If the import couldn't be resolved, stub it in.
		switch(type.kind)
		{
		case IR::ExternKind::function:
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
				case IR::ValueType::anyref:
				case IR::ValueType::funcref:
				case IR::ValueType::nullref: encoder.ref_null(); break;
				default: Errors::unreachable();
				};
			}
			encoder.end();

			// Generate a module for the stub function.
			IR::Module stubModule;
			IR::DisassemblyNames stubModuleNames;
			stubModule.types.push_back(asFunctionType(type));
			stubModule.functions.defs.push_back({{0}, {}, std::move(codeStream.getBytes()), {}});
			stubModule.exports.push_back({"importStub", IR::ExternKind::function, 0});
			stubModuleNames.functions.push_back({"importStub: " + exportName, {}, {}});
			IR::setDisassemblyNames(stubModule, stubModuleNames);
			IR::validatePreCodeSections(stubModule);
			IR::validatePostCodeSections(stubModule);

			// Instantiate the module and return the stub function instance.
			auto stubModuleInstance = Runtime::instantiateModule(
				compartment, Runtime::compileModule(stubModule), {}, "importStub");
			return getInstanceExport(stubModuleInstance, "importStub");
		}
		case IR::ExternKind::memory:
		{
			return asObject(
				Runtime::createMemory(compartment, asMemoryType(type), std::string(exportName)));
		}
		case IR::ExternKind::table:
		{
			return asObject(
				Runtime::createTable(compartment, asTableType(type), std::string(exportName)));
		}
		case IR::ExternKind::global:
		{
			return asObject(Runtime::createGlobal(compartment, asGlobalType(type)));
		}
		case IR::ExternKind::exceptionType:
		{
			return asObject(
				Runtime::createExceptionType(compartment, asExceptionType(type), "importStub"));
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
	module.featureSpec.maxDataSegments = 65536;
	if(!WASM::loadBinaryModule(data, numBytes, module, Log::debug)) { return 0; }

	GCPointer<Compartment> compartment = createCompartment();
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
			[&](Exception* exception) { destroyException(exception); });
	}
	errorUnless(tryCollectCompartment(std::move(compartment)));

	return 0;
}

#if !WAVM_ENABLE_LIBFUZZER
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
