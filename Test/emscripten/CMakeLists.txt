set(TestNames
	args
	clock
	exit
	stdout)

set(TestSources "")
foreach(TestName ${TestNames})
	list(APPEND TestSources ${TestName}.cpp)
endforeach()

# Run the stdout test as a basic test of "wavm run" and the WASI subsystem.
if(WAVM_ENABLE_RUNTIME)
	add_test(
		NAME emscripten_args
		COMMAND $<TARGET_FILE:wavm> run --abi=emscripten ${CMAKE_CURRENT_LIST_DIR}/args.wasm arg1 arg2)
	set_tests_properties(emscripten_args PROPERTIES PASS_REGULAR_EXPRESSION
		"argc=3\nargv\\[0\\]: .+\nargv\\[1\\]: arg1\nargv\\[2\\]: arg2\nargv\\[3\\]: <null>")

	add_test(
		NAME emscripten_clock
		COMMAND $<TARGET_FILE:wavm> run --abi=emscripten ${CMAKE_CURRENT_LIST_DIR}/clock.wasm)
		
	add_test(
		NAME emscripten_exit
		COMMAND $<TARGET_FILE:wavm> run --abi=emscripten ${CMAKE_CURRENT_LIST_DIR}/exit.wasm)
		
	add_test(
		NAME emscripten_stdout
		COMMAND $<TARGET_FILE:wavm> run --abi=emscripten ${CMAKE_CURRENT_LIST_DIR}/stdout.wasm)
	set_tests_properties(emscripten_stdout PROPERTIES PASS_REGULAR_EXPRESSION "Hello world!")
			
	# Test emscripten_stdout without an explicit ABI specified on the command-line.
	add_test(
		NAME emscripten_stdout_detected_abi
		COMMAND $<TARGET_FILE:wavm> run ${CMAKE_CURRENT_LIST_DIR}/stdout.wasm)
	set_tests_properties(emscripten_stdout_detected_abi PROPERTIES PASS_REGULAR_EXPRESSION "Hello world!")
endif()

add_custom_target(EmscriptenTests SOURCES ${TestSources})
