#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "WAVM/wavm-c/wavm-c.h"

#define own

wasm_compartment_t* compartment = NULL;
wasm_memory_t* memory = NULL;

static void handle_trap(wasm_trap_t* trap)
{
	char* message_buffer = malloc(1024);
	size_t num_message_bytes = 1024;
	if(!wasm_trap_message(trap, message_buffer, &num_message_bytes))
	{
		message_buffer = malloc(num_message_bytes);
		assert(wasm_trap_message(trap, message_buffer, &num_message_bytes));
	}

	fprintf(stderr, "Runtime error: %.*s\n", (int)num_message_bytes, message_buffer);

	wasm_trap_delete(trap);
	free(message_buffer);
}

// A function to be called from Wasm code.
static own wasm_trap_t* hello_callback(const wasm_val_t args[], wasm_val_t results[])
{
	// Make a copy of the string passed as an argument, and ensure that it is null terminated.
	const uint32_t address = (uint32_t)args[0].i32;
	const uint32_t num_chars = (uint32_t)args[1].i32;
	char buffer[1025];
	if(num_chars > 1024)
	{
		fprintf(stderr, "Callback called with too many characters: num_chars=%u.\n", num_chars);
		exit(1);
	}

	const size_t num_memory_bytes = wasm_memory_data_size(memory);
	if(((uint64_t)address + (uint64_t)num_chars) > num_memory_bytes)
	{
		fprintf(stderr,
				"Callback called with out-of-bounds string address:\n"
				"  address=%u\n"
				"  num_chars=%u\n"
				"  wasm_memory_data_size(memory)=%zu\n",
				address,
				num_chars,
				wasm_memory_data_size(memory));
		exit(1);
	}

	memcpy(buffer, wasm_memory_data(memory) + address, num_chars);
	buffer[num_chars] = 0;

	printf("Hello, %s!\n", buffer);
	results[0].i32 = num_chars;
	return NULL;
}

int main(int argc, char** argv)
{
	// Initialize.
	wasm_engine_t* engine = wasm_engine_new();
	compartment = wasm_compartment_new(engine, "compartment");
	wasm_store_t* store = wasm_store_new(compartment, "store");

	char hello_wast[]
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

	// Compile.
	own wasm_module_t* module = wasm_module_new_text(engine, hello_wast, sizeof(hello_wast));
	if(!module)
	{
		fprintf(stderr, "Failed to compile module.\n");
		return 1;
	}

	// Create external print functions.
	own wasm_functype_t* hello_type = wasm_functype_new_2_1(
		wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32());
	own wasm_func_t* hello_func = wasm_func_new(compartment, hello_type, hello_callback, "hello");

	wasm_functype_delete(hello_type);

	// Instantiate.
	const wasm_extern_t* imports[1];
	imports[0] = wasm_func_as_extern(hello_func);
	wasm_trap_t* trap = NULL;
	own wasm_instance_t* instance = wasm_instance_new(store, module, imports, &trap, "instance");
	if(!instance)
	{
		handle_trap(trap);
		return 1;
	}

	wasm_func_delete(hello_func);

	// Extract exports.
	wasm_extern_t* memory_extern = wasm_instance_export(instance, 0);
	assert(memory_extern);
	memory = wasm_extern_as_memory(memory_extern);
	assert(memory);
	wasm_extern_t* malloc_extern = wasm_instance_export(instance, 1);
	assert(malloc_extern);
	const wasm_func_t* malloc_func = wasm_extern_as_func(malloc_extern);
	assert(malloc_func);
	wasm_extern_t* run_extern = wasm_instance_export(instance, 2);
	assert(run_extern);
	const wasm_func_t* run_func = wasm_extern_as_func(run_extern);
	assert(run_func);

	wasm_module_delete(module);
	wasm_instance_delete(instance);

	// Allocate a buffer in WebAssembly memory to hold our input string.
	const char* input_string = "embedder-example.c user";
	const size_t num_string_chars = strlen(input_string);

	wasm_val_t malloc_args[1];
	wasm_val_t malloc_results[1];
	malloc_args[0].i32 = (int32_t)num_string_chars;
	trap = wasm_func_call(store, malloc_func, malloc_args, malloc_results);
	if(trap)
	{
		handle_trap(trap);
		return 1;
	}

	// Check that the returned address is within the bounds of the WebAssembly memory.
	const uint32_t string_address = (uint32_t)malloc_results[0].i32;
	if(string_address + num_string_chars > wasm_memory_data_size(memory))
	{
		fprintf(stderr,
				"malloc returned out-of-bounds buffer:\n"
				"  string_address=%u\n"
				"  num_string_chars=%zu\n"
				"  wasm_memory_data_size(memory)=%zu\n",
				string_address,
				num_string_chars,
				wasm_memory_data_size(memory));
		return 1;
	}

	// Copy the input string into the WebAssembly memory.
	memcpy(wasm_memory_data(memory) + string_address, input_string, num_string_chars);

	// Pass the WebAssembly memory copy of the input string to the run function.
	wasm_val_t run_args[2];
	wasm_val_t run_results[1];
	run_args[0].i32 = string_address;
	run_args[1].i32 = (int32_t)num_string_chars;
	trap = wasm_func_call(store, run_func, run_args, run_results);
	if(trap)
	{
		handle_trap(trap);
		return 1;
	}

	printf("WASM call returned: %i\n", run_results[0].i32);

	// Shut down.
	wasm_store_delete(store);
	wasm_compartment_delete(compartment);
	wasm_engine_delete(engine);

	return 0;
}
