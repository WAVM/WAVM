# Helper function for adding WAST test scripts.
set(TestScriptSources "" CACHE INTERNAL "" FORCE)
function(ADD_WAST_TESTS)
	cmake_parse_arguments(TEST
		"RUN_SERIAL"
		"NAME_PREFIX"
		"SOURCES;WAVM_ARGS"
		${ARGN})

	foreach(TEST_SOURCE ${TEST_SOURCES})
		get_filename_component(TEST_SOURCE_ABSOLUTE ${TEST_SOURCE} ABSOLUTE)
		list(APPEND TestScriptSources ${TEST_SOURCE_ABSOLUTE})
		set(TestScriptSources ${TestScriptSources} CACHE INTERNAL "" FORCE)
			
		if(WAVM_ENABLE_RUNTIME)
			get_filename_component(TEST_NAME ${TEST_SOURCE} NAME)
			add_test(
				NAME ${TEST_NAME_PREFIX}${TEST_NAME}
				COMMAND $<TARGET_FILE:wavm> test script ${TEST_SOURCE_ABSOLUTE} ${TEST_WAVM_ARGS})
			if(TEST_RUN_SERIAL)
				set_tests_properties(${TEST_NAME_PREFIX}${TEST_NAME} PROPERTIES RUN_SERIAL TRUE)
			endif()
		endif()
	endforeach()
endfunction()

if(WAVM_ENABLE_FUZZ_TARGETS)
	add_subdirectory(fuzz)
endif()

add_subdirectory(benchmark)
add_subdirectory(emscripten)
add_subdirectory(wasi)
add_subdirectory(wavm)
add_subdirectory(WebAssembly)

add_custom_target(TestScripts SOURCES ${TestScriptSources})
set_target_properties(TestScripts PROPERTIES FOLDER Testing)
