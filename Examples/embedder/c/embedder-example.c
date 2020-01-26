#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "WAVM/wavm-c/wavm-c.h"

#define own

// A function to be called from Wasm code.
own wasm_trap_t* hello_callback(const wasm_val_t args[], wasm_val_t results[])
{
	printf("Hello world! (argument = %i)\n", args[0].i32);
	results[0].i32 = args[0].i32 + 1;
	return NULL;
}

int main(int argc, char** argv)
{
	// Initialize.
	wasm_engine_t* engine = wasm_engine_new();
	wasm_compartment_t* compartment = wasm_compartment_new(engine, "compartment");
	wasm_store_t* store = wasm_store_new(compartment, "store");

	char hello_wast[]
		= "(module\n"
		  "  (import \"\" \"hello\" (func $1 (param i32) (result i32)))\n"
		  "  (func (export \"run\") (param i32) (result i32)\n"
		  "    (call $1 (local.get 0))\n"
		  "  )\n"
		  ")";

	// Compile.
	own wasm_module_t* module = wasm_module_new_text(engine, hello_wast, sizeof(hello_wast));
	if(!module) { return 1; }

	// Create external print functions.
	own wasm_functype_t* hello_type
		= wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
	own wasm_func_t* hello_func = wasm_func_new(compartment, hello_type, hello_callback, "hello");

	wasm_functype_delete(hello_type);

	// Instantiate.
	const wasm_extern_t* imports[1];
	imports[0] = wasm_func_as_extern(hello_func);
	own wasm_instance_t* instance = wasm_instance_new(store, module, imports, NULL, "instance");
	if(!instance) { return 1; }

	wasm_func_delete(hello_func);

	// Extract export.
	wasm_extern_t* run_extern = wasm_instance_export(instance, 0);
	if(run_extern == NULL) { return 1; }
	const wasm_func_t* run_func = wasm_extern_as_func(run_extern);
	if(run_func == NULL) { return 1; }

	wasm_module_delete(module);
	wasm_instance_delete(instance);

	// Call.
	wasm_val_t args[1];
	wasm_val_t results[1];
	args[0].i32 = 100;
	if(wasm_func_call(store, run_func, args, results)) { return 1; }

	printf("WASM call returned: %i\n", results[0].i32);

	// Shut down.
	wasm_store_delete(store);
	wasm_compartment_delete(compartment);
	wasm_engine_delete(engine);

	return 0;
}
