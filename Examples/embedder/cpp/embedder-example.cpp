#include <inttypes.h>
#include <stdio.h>
#include <vector>
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

WAVM_DEFINE_INTRINSIC_MODULE(embedder)
WAVM_DEFINE_INTRINSIC_FUNCTION(embedder, "hello", I32, hello, I32 argument)
{
	printf("Hello world! (argument = %i)\n", argument);
	return argument + 1;
}

int main(int argc, char** argv)
{
	// Compile a WASM module from text.
	char helloWAST[]
		= "(module\n"
		  "  (import \"\" \"hello\" (func $1 (param i32) (result i32)))\n"
		  "  (func (export \"run\") (param i32) (result i32)\n"
		  "    (call $1 (local.get 0))\n"
		  "  )\n"
		  ")";

	IR::Module irModule;
	std::vector<WAST::Error> wastErrors;
	if(!WAST::parseModule(helloWAST, sizeof(helloWAST), irModule, wastErrors))
	{ return EXIT_FAILURE; }

	ModuleRef module = compileModule(irModule);

	// Create a WAVM compartment and context.
	GCPointer<Compartment> compartment = createCompartment();
	Context* context = createContext(compartment);

	// Create an instance that encapsulates the intrinsic function in a way that allows it to be
	// imported by WASM instances.
	Instance* intrinsicsInstance = WAVM::Intrinsics::instantiateModule(
		compartment, {WAVM_INTRINSIC_MODULE_REF(embedder)}, "embedder");
	const FunctionType i32_to_i32({ValueType::i32}, {ValueType::i32});
	Function* intrinsicFunction = getTypedInstanceExport(intrinsicsInstance, "hello", i32_to_i32);

	// Instantiate the WASM module using the intrinsic function as its import.
	Instance* instance
		= instantiateModule(compartment, module, {asObject(intrinsicFunction)}, "hello");

	// Call the WASM module's "run" function.
	Function* runFunction = getTypedInstanceExport(instance, "run", i32_to_i32);

	UntaggedValue args[1]{I32(100)};
	UntaggedValue results[1];
	invokeFunction(context, runFunction, i32_to_i32, args, results);

	printf("WASM call returned: %i\n", results[0].i32);

	// Clean up the WAVM runtime objects.
	WAVM_ERROR_UNLESS(tryCollectCompartment(std::move(compartment)));

	return 0;
}
