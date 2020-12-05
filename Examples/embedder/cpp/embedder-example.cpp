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

Memory* memory = nullptr;

WAVM_DEFINE_INTRINSIC_MODULE(embedder)
WAVM_DEFINE_INTRINSIC_FUNCTION(embedder, "hello", U32, hello, U32 address, U32 numChars)
{
	// Make a copy of the string passed as an argument, and ensure that it is null terminated.
	char buffer[1025];
	if(numChars > 1024) { throwException(ExceptionTypes::invalidArgument); }
	memcpy(buffer, memoryArrayPtr<char>(memory, address, numChars), numChars);
	buffer[numChars] = 0;

	printf("Hello, %s!\n", buffer);
	return numChars;
}

int main(int argc, char** argv)
{
	// Compile a WASM module from text.
	char helloWAST[]
		= "(module\n"
		  "  (import \"\" \"hello\" (func $1 (param i32 i32) (result i32)))\n"
		  "  (memory (export \"memory\") 1)\n"
		  "  (global $nextFreeMemoryAddress (mut i32) (i32.const 0))\n"
		  "  (func (export \"malloc\") (param $numBytes i32) (result i32)\n"
		  "    (local $address i32)\n"
		  "    (local.set $address (global.get $nextFreeMemoryAddress))\n"
		  "    (global.set $nextFreeMemoryAddress\n"
		  "      (i32.add (local.get $address) (local.get $numBytes)))\n"
		  "    (local.get $address)\n"
		  "  )\n"
		  "  (func (export \"run\") (param $address i32) (param $num_chars i32) (result i32)\n"
		  "    (call $1 (local.get $address) (local.get $num_chars))\n"
		  "  )\n"
		  ")";

	IR::Module irModule;
	std::vector<WAST::Error> wastErrors;
	if(!WAST::parseModule(helloWAST, sizeof(helloWAST), irModule, wastErrors))
	{ return EXIT_FAILURE; }

	ModuleRef module = compileModule(irModule);

	// Create a WAVM compartment and context.
	GCPointer<Compartment> compartment = createCompartment();
	GCPointer<Context> context = createContext(compartment);

	// Create an instance that encapsulates the intrinsic function in a way that allows it to be
	// imported by WASM instances.
	Instance* intrinsicsInstance = WAVM::Intrinsics::instantiateModule(
		compartment, {WAVM_INTRINSIC_MODULE_REF(embedder)}, "embedder");
	const FunctionType i32_i32_to_i32({ValueType::i32}, {ValueType::i32, ValueType::i32});
	Function* intrinsicFunction
		= getTypedInstanceExport(intrinsicsInstance, "hello", i32_i32_to_i32);

	catchRuntimeExceptions(
		[&]() {
			// Instantiate the WASM module using the intrinsic function as its import.
			GCPointer<Instance> instance
				= instantiateModule(compartment, module, {asObject(intrinsicFunction)}, "hello");

			// Extract exports.
			const FunctionType i32_to_i32({ValueType::i32}, {ValueType::i32});
			Function* mallocFunction = getTypedInstanceExport(instance, "malloc", i32_to_i32);
			Function* runFunction = getTypedInstanceExport(instance, "run", i32_i32_to_i32);
			memory = getTypedInstanceExport(
				instance,
				"memory",
				MemoryType(false, IndexType::i32, SizeConstraints{1, UINT64_MAX}));
			WAVM_ASSERT(mallocFunction);
			WAVM_ASSERT(runFunction);
			WAVM_ASSERT(memory);

			// Allocate a buffer in WebAssembly memory to hold our input string.
			const char* inputString = "embedder-example.cpp user";
			const size_t numStringChars = strlen(inputString);

			UntaggedValue mallocArgs[1]{U32(numStringChars)};
			UntaggedValue mallocResults[1];
			invokeFunction(context, mallocFunction, i32_to_i32, mallocArgs, mallocResults);

			// Copy the input string into the WebAssembly memory.
			const uint32_t stringAddress = mallocResults[0].u32;
			memcpy(memoryArrayPtr<char>(memory, stringAddress, numStringChars),
				   inputString,
				   numStringChars);

			// Pass the WebAssembly memory copy of the input string to the run function.
			UntaggedValue runArgs[2]{stringAddress, U32(numStringChars)};
			UntaggedValue runResults[1];
			invokeFunction(context, runFunction, i32_i32_to_i32, runArgs, runResults);

			printf("WASM call returned: %u\n", runResults[0].u32);
		},
		[&](Exception* exception) {
			// Treat any unhandled exception as a fatal error.
			Errors::fatalf("Runtime exception: %s", describeException(exception).c_str());

			destroyException(exception);
		});

	// Clean up the WAVM runtime objects.
	context = nullptr;
	WAVM_ERROR_UNLESS(tryCollectCompartment(std::move(compartment)));

	return 0;
}
