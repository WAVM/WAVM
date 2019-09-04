#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "WAVM/wavm-c/wavm-c.h"
#include "wavm-test.h"

#define own

// A function to be called from Wasm code.
own wasm_trap_t* hello_callback(const wasm_val_t args[], wasm_val_t results[])
{
	printf("Calling back...\n");
	printf("Hello World!\n");
	return NULL;
}

int execCAPITest(int argc, char** argv)
{
	// Initialize.
	printf("Initializing...\n");
	wasm_engine_t* engine = wasm_engine_new();
	wasm_compartment_t* compartment = wasm_compartment_new(engine);
	wasm_store_t* store = wasm_store_new(compartment);

	// Load binary.
	printf("Loading binary...\n");
	char hello_wasm[]
		= {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x84, 0x80, 0x80, 0x80, 0x00, 0x01,
		   0x60, 0x00, 0x00, 0x02, 0x8a, 0x80, 0x80, 0x80, 0x00, 0x01, 0x00, 0x05, 0x68, 0x65, 0x6c,
		   0x6c, 0x6f, 0x00, 0x00, 0x03, 0x82, 0x80, 0x80, 0x80, 0x00, 0x01, 0x00, 0x07, 0x87, 0x80,
		   0x80, 0x80, 0x00, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x01, 0x0a, 0x8a, 0x80, 0x80, 0x80,
		   0x00, 0x01, 0x84, 0x80, 0x80, 0x80, 0x00, 0x00, 0x10, 0x00, 0x0b};

	// Compile.
	printf("Compiling module...\n");
	own wasm_module_t* module = wasm_module_new(engine, hello_wasm, sizeof(hello_wasm));
	if(!module)
	{
		printf("> Error compiling module!\n");
		return 1;
	}

	// Create external print functions.
	printf("Creating callback...\n");
	own wasm_functype_t* hello_type = wasm_functype_new_0_0();
	own wasm_func_t* hello_func = wasm_func_new(compartment, hello_type, hello_callback);

	wasm_functype_delete(hello_type);

	// Instantiate.
	printf("Instantiating module...\n");
	const wasm_extern_t* imports[1];
	imports[0] = wasm_func_as_extern(hello_func);
	own wasm_instance_t* instance = wasm_instance_new(store, module, imports, NULL);
	if(!instance)
	{
		printf("> Error instantiating module!\n");
		return 1;
	}

	wasm_func_delete(hello_func);

	// Extract export.
	printf("Extracting export...\n");
	wasm_extern_t* run_extern = wasm_instance_export(instance, 0);
	if(run_extern == NULL)
	{
		printf("> Error accessing export!\n");
		return 1;
	}
	const wasm_func_t* run_func = wasm_extern_as_func(run_extern);
	if(run_func == NULL)
	{
		printf("> Export is not a function!\n");
		return 1;
	}

	wasm_module_delete(module);
	wasm_instance_delete(instance);

	// Call.
	printf("Calling export...\n");
	if(wasm_func_call(store, run_func, NULL, NULL))
	{
		printf("> Error calling function!\n");
		return 1;
	}

	// Shut down.
	printf("Shutting down...\n");
	wasm_store_delete(store);
	wasm_compartment_delete(compartment);
	wasm_engine_delete(engine);

	// All done.
	printf("Done.\n");
	return 0;
}
