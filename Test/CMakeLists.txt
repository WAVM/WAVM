# Helper functions for adding WAST test scripts.
function(ADD_WAST_TEST WAST_PATH)
	get_filename_component(WAST_NAME ${WAST_PATH} NAME)
	add_test(
		NAME ${WAST_NAME}
		COMMAND $<TARGET_FILE:wavm-test> script ${CMAKE_CURRENT_LIST_DIR}/${WAST_PATH} "--test-cloning" ${ARGN})
endfunction()

function(ADD_WAST_TESTS WAST_PATHS)
	foreach(WAST_PATH ${WAST_PATHS})
		ADD_WAST_TEST(${WAST_PATH} ${ARGN})
	endforeach()
endfunction()

set(WASTTests
	binaryen/binaryen_simd.wast
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
	reference_types.wast
	simd.wast
	syntax_recursion.wast
	threads.wast
	trunc_sat.wast
	wavm_atomic.wast
)
add_custom_target(WAVMTests SOURCES ${WASTTests})
set_target_properties(WAVMTests PROPERTIES FOLDER Testing)

if(WAVM_ENABLE_RUNTIME)
	ADD_WAST_TESTS("${WASTTests}")

	# TODO: fix the memory leak in this test.
	set_tests_properties(exceptions.wast PROPERTIES ENVIRONMENT ASAN_OPTIONS=detect_leaks=0)

endif()

add_subdirectory(fuzz)
add_subdirectory(spec)
add_subdirectory(wasi)
