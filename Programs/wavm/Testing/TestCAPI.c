#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "WAVM/wavm-c/wavm-c.h"
#include "wavm-test.h"

#define own

static int numFailures = 0;

#define CAPI_CHECK(cond, msg)                                                                      \
	do                                                                                             \
	{                                                                                              \
		if(!(cond))                                                                                \
		{                                                                                          \
			fprintf(stderr, "%s(%d): FAIL: %s\n", __FILE__, __LINE__, msg);                        \
			++numFailures;                                                                         \
		}                                                                                          \
	} while(0)

#define CAPI_CHECK_NOT_NULL(ptr) CAPI_CHECK((ptr) != NULL, #ptr " != NULL")

static uintptr_t numCallbacks = 0;

// A function to be called from Wasm code.
own wasm_trap_t* hello_callback(const wasm_val_t args[], wasm_val_t results[])
{
	++numCallbacks;
	return NULL;
}

int execCAPITest(int argc, char** argv)
{
	// Initialize.
	wasm_engine_t* engine = wasm_engine_new();
	wasm_compartment_t* compartment = wasm_compartment_new(engine, "compartment");
	wasm_store_t* store = wasm_store_new(compartment, "store");

	// Load binary.
	// Module: imports (hello : [] -> []), defines (run : [] -> []) that calls hello.
	char hello_wasm[]
		= {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x84, 0x80, 0x80, 0x80, 0x00, 0x01,
		   0x60, 0x00, 0x00, 0x02, 0x8a, 0x80, 0x80, 0x80, 0x00, 0x01, 0x00, 0x05, 0x68, 0x65, 0x6c,
		   0x6c, 0x6f, 0x00, 0x00, 0x03, 0x82, 0x80, 0x80, 0x80, 0x00, 0x01, 0x00, 0x07, 0x87, 0x80,
		   0x80, 0x80, 0x00, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x01, 0x0a, 0x8a, 0x80, 0x80, 0x80,
		   0x00, 0x01, 0x84, 0x80, 0x80, 0x80, 0x00, 0x00, 0x10, 0x00, 0x0b};

	// Compile.
	own wasm_module_t* module = wasm_module_new(engine, hello_wasm, sizeof(hello_wasm));
	if(!module) { return 1; }

	// Create external print functions.
	own wasm_functype_t* hello_type = wasm_functype_new_0_0();
	own wasm_func_t* hello_func
		= wasm_func_new(compartment, hello_type, hello_callback, "hello_callback");

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
	if(wasm_func_call(store, run_func, NULL, NULL)) { return 1; }

	// Assert that the callback was called exactly once.
	CAPI_CHECK(numCallbacks == 1, "numCallbacks == 1");

	// =========================================================================
	// Config and features
	// =========================================================================
	{
		own wasm_config_t* config = wasm_config_new();
		CAPI_CHECK_NOT_NULL(config);
		wasm_config_feature_set_simd(config, true);
		wasm_config_feature_set_atomics(config, false);
		wasm_config_feature_set_reference_types(config, true);
		own wasm_engine_t* engine2 = wasm_engine_new_with_config(config);
		CAPI_CHECK_NOT_NULL(engine2);
		// config is consumed by wasm_engine_new_with_config
		wasm_engine_delete(engine2);
	}

	// =========================================================================
	// Value types
	// =========================================================================
	{
		own wasm_valtype_t* t_i32 = wasm_valtype_new_i32();
		CAPI_CHECK_NOT_NULL(t_i32);
		CAPI_CHECK(wasm_valtype_kind(t_i32) == WASM_I32, "valtype_kind == WASM_I32");

		own wasm_valtype_t* t_i64 = wasm_valtype_new_i64();
		CAPI_CHECK(wasm_valtype_kind(t_i64) == WASM_I64, "valtype_kind == WASM_I64");

		own wasm_valtype_t* t_f32 = wasm_valtype_new_f32();
		CAPI_CHECK(wasm_valtype_kind(t_f32) == WASM_F32, "valtype_kind == WASM_F32");

		own wasm_valtype_t* t_f64 = wasm_valtype_new_f64();
		CAPI_CHECK(wasm_valtype_kind(t_f64) == WASM_F64, "valtype_kind == WASM_F64");

		own wasm_valtype_t* t_copy = wasm_valtype_copy(t_i32);
		CAPI_CHECK_NOT_NULL(t_copy);
		CAPI_CHECK(wasm_valtype_kind(t_copy) == WASM_I32, "copied valtype kind == WASM_I32");

		wasm_valtype_delete(t_copy);
		wasm_valtype_delete(t_i32);
		wasm_valtype_delete(t_i64);
		wasm_valtype_delete(t_f32);
		wasm_valtype_delete(t_f64);
	}

	// =========================================================================
	// Function types
	// =========================================================================
	{
		own wasm_valtype_t* params[2];
		params[0] = wasm_valtype_new_i32();
		params[1] = wasm_valtype_new_i64();
		own wasm_valtype_t* results[1];
		results[0] = wasm_valtype_new_f32();

		own wasm_functype_t* ft = wasm_functype_new(params, 2, results, 1);
		CAPI_CHECK_NOT_NULL(ft);
		CAPI_CHECK(wasm_functype_num_params(ft) == 2, "functype num_params == 2");
		CAPI_CHECK(wasm_functype_num_results(ft) == 1, "functype num_results == 1");

		own wasm_valtype_t* p0 = wasm_functype_param(ft, 0);
		CAPI_CHECK_NOT_NULL(p0);
		CAPI_CHECK(wasm_valtype_kind(p0) == WASM_I32, "functype param 0 == i32");

		own wasm_valtype_t* p1 = wasm_functype_param(ft, 1);
		CAPI_CHECK_NOT_NULL(p1);
		CAPI_CHECK(wasm_valtype_kind(p1) == WASM_I64, "functype param 1 == i64");

		own wasm_valtype_t* r0 = wasm_functype_result(ft, 0);
		CAPI_CHECK_NOT_NULL(r0);
		CAPI_CHECK(wasm_valtype_kind(r0) == WASM_F32, "functype result 0 == f32");

		wasm_valtype_delete(p0);
		wasm_valtype_delete(p1);
		wasm_valtype_delete(r0);

		own wasm_functype_t* ft_copy = wasm_functype_copy(ft);
		CAPI_CHECK_NOT_NULL(ft_copy);
		CAPI_CHECK(wasm_functype_num_params(ft_copy) == 2, "copied functype num_params == 2");

		wasm_functype_delete(ft_copy);
		wasm_functype_delete(ft);
	}

	// =========================================================================
	// Memory operations
	// =========================================================================
	{
		wasm_limits_t limits = {1, 4};
		own wasm_memorytype_t* memtype
			= wasm_memorytype_new(&limits, WASM_NOTSHARED, WASM_INDEX_I32);
		CAPI_CHECK_NOT_NULL(memtype);

		own wasm_memory_t* mem = wasm_memory_new(compartment, memtype, "test_memory");
		CAPI_CHECK_NOT_NULL(mem);

		CAPI_CHECK(wasm_memory_size(mem) == 1, "memory initial size == 1");
		CAPI_CHECK(wasm_memory_data_size(mem) == 65536, "memory data_size == 65536");

		char* data = wasm_memory_data(mem);
		CAPI_CHECK_NOT_NULL(data);
		data[0] = 42;
		CAPI_CHECK(data[0] == 42, "memory read back == 42");

		wasm_memory_pages_t prev_size = 0;
		CAPI_CHECK(wasm_memory_grow(mem, 2, &prev_size), "memory grow succeeds");
		CAPI_CHECK(prev_size == 1, "memory grow prev_size == 1");
		CAPI_CHECK(wasm_memory_size(mem) == 3, "memory size after grow == 3");

		// Grow beyond max should fail
		CAPI_CHECK(!wasm_memory_grow(mem, 10, NULL), "memory grow beyond max fails");

		wasm_memorytype_delete(memtype);
		wasm_memory_delete(mem);
	}

	// =========================================================================
	// Table operations
	// =========================================================================
	{
		own wasm_valtype_t* elem_type = wasm_valtype_new_funcref();
		wasm_limits_t limits = {2, 10};
		own wasm_tabletype_t* tabtype
			= wasm_tabletype_new(elem_type, &limits, WASM_NOTSHARED, WASM_INDEX_I32);
		CAPI_CHECK_NOT_NULL(tabtype);

		own wasm_table_t* tab = wasm_table_new(compartment, tabtype, NULL, "test_table");
		CAPI_CHECK_NOT_NULL(tab);

		CAPI_CHECK(wasm_table_size(tab) == 2, "table initial size == 2");

		own wasm_ref_t* elem = wasm_table_get(tab, 0);
		CAPI_CHECK(elem == NULL, "table initial element is null");

		wasm_table_size_t prev_size = 0;
		CAPI_CHECK(wasm_table_grow(tab, 3, NULL, &prev_size), "table grow succeeds");
		CAPI_CHECK(prev_size == 2, "table grow prev_size == 2");
		CAPI_CHECK(wasm_table_size(tab) == 5, "table size after grow == 5");

		CAPI_CHECK(wasm_table_set(tab, 0, NULL), "table set null succeeds");

		wasm_tabletype_delete(tabtype);
		wasm_table_delete(tab);
	}

	// =========================================================================
	// Global operations
	// =========================================================================
	{
		own wasm_valtype_t* val_type = wasm_valtype_new_i32();
		own wasm_globaltype_t* gt = wasm_globaltype_new(val_type, WASM_VAR);
		CAPI_CHECK_NOT_NULL(gt);

		wasm_val_t init_val;
		init_val.i32 = 42;
		own wasm_global_t* glob = wasm_global_new(compartment, gt, &init_val, "test_global");
		CAPI_CHECK_NOT_NULL(glob);

		wasm_val_t got_val;
		wasm_global_get(store, glob, &got_val);
		CAPI_CHECK(got_val.i32 == 42, "global get == 42");

		wasm_val_t new_val;
		new_val.i32 = 99;
		wasm_global_set(store, glob, &new_val);

		wasm_global_get(store, glob, &got_val);
		CAPI_CHECK(got_val.i32 == 99, "global get after set == 99");

		own wasm_globaltype_t* gt2 = wasm_global_type(glob);
		CAPI_CHECK_NOT_NULL(gt2);
		CAPI_CHECK(wasm_globaltype_mutability(gt2) == WASM_VAR, "global is mutable");
		const wasm_valtype_t* content = wasm_globaltype_content(gt2);
		CAPI_CHECK_NOT_NULL(content);
		CAPI_CHECK(wasm_valtype_kind(content) == WASM_I32, "global content type == i32");

		wasm_globaltype_delete(gt2);
		wasm_globaltype_delete(gt);
		wasm_global_delete(glob);
	}

	// =========================================================================
	// Module introspection
	// =========================================================================
	{
		// Recompile the hello module for introspection
		own wasm_module_t* mod = wasm_module_new(engine, hello_wasm, sizeof(hello_wasm));
		CAPI_CHECK_NOT_NULL(mod);

		CAPI_CHECK(wasm_module_num_imports(mod) == 1, "module has 1 import");
		wasm_import_t imp;
		wasm_module_import(mod, 0, &imp);
		CAPI_CHECK(imp.num_name_bytes == 5, "import name length == 5");
		CAPI_CHECK(memcmp(imp.name, "hello", 5) == 0, "import name == 'hello'");
		wasm_externtype_delete(imp.type);

		CAPI_CHECK(wasm_module_num_exports(mod) == 1, "module has 1 export");
		wasm_export_t exp;
		wasm_module_export(mod, 0, &exp);
		CAPI_CHECK(exp.num_name_bytes == 3, "export name length == 3");
		CAPI_CHECK(memcmp(exp.name, "run", 3) == 0, "export name == 'run'");
		wasm_externtype_delete(exp.type);

		wasm_module_delete(mod);
	}

	// =========================================================================
	// Module validation
	// =========================================================================
	{
		// A minimal valid WASM binary (just the 8-byte header for an empty module).
		char valid_wasm[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
		CAPI_CHECK(wasm_module_validate(valid_wasm, sizeof(valid_wasm)),
				   "valid module passes validation");

		char invalid_wasm[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0xFF};
		CAPI_CHECK(!wasm_module_validate(invalid_wasm, sizeof(invalid_wasm)),
				   "invalid module fails validation");
	}

	// =========================================================================
	// Text module compilation
	// =========================================================================
	{
		static const char hello_wat[]
			= "(module"
			  "  (func (import \"\" \"hello\"))"
			  "  (func (export \"run\") (call 0))"
			  ")";

		own wasm_module_t* text_mod = wasm_module_new_text(engine, hello_wat, sizeof(hello_wat));
		CAPI_CHECK_NOT_NULL(text_mod);

		// Instantiate and call the text-compiled module.
		own wasm_functype_t* ft = wasm_functype_new_0_0();
		own wasm_func_t* func
			= wasm_func_new(compartment, ft, hello_callback, "text_hello_callback");
		const wasm_extern_t* text_imports[1];
		text_imports[0] = wasm_func_as_extern(func);
		own wasm_instance_t* text_instance
			= wasm_instance_new(store, text_mod, text_imports, NULL, "text_instance");
		CAPI_CHECK_NOT_NULL(text_instance);

		wasm_extern_t* text_run_extern = wasm_instance_export(text_instance, 0);
		CAPI_CHECK_NOT_NULL(text_run_extern);
		const wasm_func_t* text_run_func = wasm_extern_as_func(text_run_extern);
		CAPI_CHECK_NOT_NULL(text_run_func);

		wasm_module_delete(text_mod);
		wasm_instance_delete(text_instance);

		CAPI_CHECK(!wasm_func_call(store, text_run_func, NULL, NULL), "text module call succeeds");
		CAPI_CHECK(numCallbacks == 2, "numCallbacks == 2 after text module call");

		wasm_functype_delete(ft);
		wasm_func_delete(func);
	}

	// =========================================================================
	// Trap inspection
	// =========================================================================
	{
		own wasm_trap_t* trap = wasm_trap_new(compartment, "test error", 10);
		CAPI_CHECK_NOT_NULL(trap);

		char message_buf[256];
		size_t message_len = sizeof(message_buf);
		CAPI_CHECK(wasm_trap_message(trap, message_buf, &message_len), "trap_message succeeds");
		CAPI_CHECK(message_len > 0, "trap message is non-empty");

		wasm_trap_delete(trap);
	}

	// =========================================================================
	// Foreign objects
	// =========================================================================
	{
		own wasm_foreign_t* foreign = wasm_foreign_new(compartment, "test_foreign");
		CAPI_CHECK_NOT_NULL(foreign);
		wasm_foreign_delete(foreign);
	}

	// =========================================================================
	// Compartment cloning and contains
	// =========================================================================
	{
		own wasm_functype_t* ft = wasm_functype_new_0_0();
		own wasm_func_t* func = wasm_func_new(compartment, ft, hello_callback, "clone_test_func");
		CAPI_CHECK_NOT_NULL(func);

		const wasm_ref_t* func_ref = wasm_func_as_ref_const(func);
		CAPI_CHECK(wasm_compartment_contains(compartment, func_ref),
				   "compartment contains its func");

		own wasm_compartment_t* clone = wasm_compartment_clone(compartment);
		CAPI_CHECK_NOT_NULL(clone);

		// Functions are shared across cloned compartments, so the original func is in both.
		CAPI_CHECK(wasm_compartment_contains(clone, func_ref),
				   "clone contains original func (functions are shared)");

		// Delete the function before the clone, since shared functions with GC roots prevent
		// the cloned compartment from being collected.
		wasm_functype_delete(ft);
		wasm_func_delete(func);
		wasm_compartment_delete(clone);
	}

	// =========================================================================
	// Reference operations: copy, same, host info
	// =========================================================================
	{
		own wasm_functype_t* ft = wasm_functype_new_0_0();
		own wasm_func_t* func = wasm_func_new(compartment, ft, hello_callback, "ref_test_func");
		CAPI_CHECK_NOT_NULL(func);

		own wasm_func_t* func_copy = wasm_func_copy(func);
		CAPI_CHECK_NOT_NULL(func_copy);
		CAPI_CHECK(wasm_func_same(func, func_copy), "original and copy are same");

		int host_data = 123;
		wasm_func_set_host_info(func, &host_data);
		CAPI_CHECK(wasm_func_get_host_info(func) == &host_data, "get_host_info returns set value");

		const char* name = wasm_func_name(func);
		CAPI_CHECK_NOT_NULL(name);
		CAPI_CHECK(strlen(name) > 0, "func name is non-empty");

		wasm_func_delete(func_copy);
		wasm_functype_delete(ft);
		wasm_func_delete(func);
	}

	// Shut down.
	wasm_store_delete(store);
	wasm_compartment_delete(compartment);
	wasm_engine_delete(engine);

	return numFailures > 0 ? 1 : 0;
}
