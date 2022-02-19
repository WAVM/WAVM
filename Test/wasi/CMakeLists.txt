set(TestNames
	append
	args
	cat
	clock
	exit
	fd_filestat_set_size
	fd_filestat_set_times
	fd_renumber
	largefile
	ls
	mkdir
	mv
	path_filestat_set_times
	preadwrite
	random
	rm
	rmdir
	stat
	stdout
	write)

set(TestSources "")
foreach(TestName ${TestNames})
	list(APPEND TestSources ${TestName}.cpp)
	
	#if(WAVM_ENABLE_RUNTIME)
	#	add_test(
	#		NAME "wasi/${TestName}"
	#		COMMAND $<TARGET_FILE:wavm> run --abi=wasi "${CMAKE_CURRENT_LIST_DIR}/${TestName}.wasm")
	#endif()
endforeach()

# Run some of the tests as a basic test of "wavm run" and the WASI subsystem.
if(WAVM_ENABLE_RUNTIME)
	add_test(
		NAME wasi_args
		COMMAND $<TARGET_FILE:wavm> run --abi=wasi ${CMAKE_CURRENT_LIST_DIR}/args.wasm arg1 arg2)
	set_tests_properties(wasi_args PROPERTIES PASS_REGULAR_EXPRESSION
		"argc=3\nargv\\[0\\]: .+\nargv\\[1\\]: arg1\nargv\\[2\\]: arg2\nargv\\[3\\]: <null>")

	add_test(
		NAME wasi_clock
		COMMAND $<TARGET_FILE:wavm> run --abi=wasi ${CMAKE_CURRENT_LIST_DIR}/clock.wasm)
		
	add_test(
		NAME wasi_exit
		COMMAND $<TARGET_FILE:wavm> run --abi=wasi ${CMAKE_CURRENT_LIST_DIR}/exit.wasm)

	add_test(
		NAME wasi_random
		COMMAND $<TARGET_FILE:wavm> run --abi=wasi ${CMAKE_CURRENT_LIST_DIR}/random.wasm)

	add_test(
		NAME wasi_stdout
		COMMAND $<TARGET_FILE:wavm> run --abi=wasi ${CMAKE_CURRENT_LIST_DIR}/stdout.wasm)
	set_tests_properties(wasi_stdout PROPERTIES PASS_REGULAR_EXPRESSION "Hello world!")
	
	# Test wasi_stdout without an explicit ABI specified on the command-line.
	add_test(
		NAME wasi_stdout_detected_abi
		COMMAND $<TARGET_FILE:wavm> run ${CMAKE_CURRENT_LIST_DIR}/stdout.wasm)
	set_tests_properties(wasi_stdout_detected_abi PROPERTIES PASS_REGULAR_EXPRESSION "Hello world!")
endif()

add_custom_target(WASITests SOURCES ${TestSources})
