# Helper function for adding WAST test scripts.
set(TestScriptSources "" CACHE INTERNAL "" FORCE)
function(ADD_WAST_TESTS)
	cmake_parse_arguments(TEST
		""
		""
		"SOURCES"
		${ARGN})

	foreach(TEST_SOURCE ${TEST_SOURCES})
		get_filename_component(TEST_SOURCE_ABSOLUTE ${TEST_SOURCE} ABSOLUTE)
		list(APPEND TestScriptSources ${TEST_SOURCE_ABSOLUTE})
		set(TestScriptSources ${TestScriptSources} CACHE INTERNAL "" FORCE)
			
		if(WAVM_ENABLE_RUNTIME)
			get_filename_component(TEST_NAME ${TEST_SOURCE} NAME)
			add_test(
				NAME ${TEST_NAME}
				COMMAND $<TARGET_FILE:wavm> test script ${TEST_SOURCE_ABSOLUTE} "--test-cloning")
		endif()
	endforeach()
endfunction()

ADD_WAST_TESTS(
	SOURCES binaryen/binaryen_simd.wast
			wabt/wabt_simd_basic.wast
			wabt/wabt_simd_binary.wast
			wabt/wabt_simd_bitselect.wast
			wabt/wabt_simd_compare.wast
			wabt/wabt_simd_lane.wast
			wabt/wabt_simd_load_store.wast
			wabt/wabt_simd_shift.wast
			wabt/wabt_simd_splat.wast
			wabt/wabt_simd_unary.wast
			bulk_memory_ops.wast
			exceptions.wast
			misc.wast
			multi_memory.wast
			reference_types.wast
			simd.wast
			syntax_recursion.wast
			threads.wast
			trunc_sat.wast
			wat_custom_section.wast
			wavm_atomic.wast)

if(WAVM_ENABLE_RUNTIME)
	# TODO: fix the memory leak in this test.
	set_tests_properties(exceptions.wast PROPERTIES ENVIRONMENT ASAN_OPTIONS=detect_leaks=0)
endif()

if(WAVM_ENABLE_FUZZ_TARGETS)
	add_subdirectory(fuzz)
endif()

add_subdirectory(emscripten)
add_subdirectory(spec)
add_subdirectory(wasi)

add_custom_target(TestScripts SOURCES ${TestScriptSources})
set_target_properties(TestScripts PROPERTIES FOLDER Testing)
