set(Sources
	blake2b.wast
	echo.wast
	helloworld.wast
	tee.wast
	trap.wast
	zlib.wasm)
add_custom_target(Examples SOURCES ${Sources})
set_target_properties(Examples PROPERTIES FOLDER Examples)

if(WAVM_ENABLE_RUNTIME)
	add_test(
		NAME examples_zlib
		COMMAND $<TARGET_FILE:wavm> run ${CMAKE_CURRENT_LIST_DIR}/zlib.wasm)
	set_tests_properties(examples_zlib PROPERTIES PASS_REGULAR_EXPRESSION
		"sizes: 100000,25906\nok.")
endif()