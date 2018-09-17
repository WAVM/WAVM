# Helper functions for adding WAST test scripts.
function(ADD_WAST_TEST WAST_PATH)
	get_filename_component(WAST_NAME ${WAST_PATH} NAME)
	add_test(
		NAME ${WAST_NAME}
		COMMAND $<TARGET_FILE:RunTestScript> ${CMAKE_CURRENT_LIST_DIR}/${WAST_PATH} ${ARGN})
endfunction()

function(ADD_WAST_TESTS WAST_PATHS)
	foreach(WAST_PATH ${WAST_PATHS})
		ADD_WAST_TEST(${WAST_PATH} ${ARGN})
	endforeach()
endfunction()

set(NonReferenceTypesWASTTests
	bulk_memory_ops.wast
	exceptions.wast
	misc.wast
	simd.wast
	threads.wast
	trunc_sat.wast
	wavm_atomic.wast
)
add_custom_target(WAVMTests SOURCES ${NonReferenceTypesWASTTests} reference_types.wast)
set_target_properties(WAVMTests PROPERTIES FOLDER Testing)

if(ENABLE_RUNTIME)
	ADD_WAST_TESTS("${NonReferenceTypesWASTTests}")
	ADD_WAST_TEST("reference_types.wast" --enable-reference-types)
endif()

add_subdirectory(Containers)
add_subdirectory(DumpTestModules)
add_subdirectory(fuzz)
add_subdirectory(RunTestScript)
add_subdirectory(spec)
