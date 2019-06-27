set(TestNames
	args
	cat
	clock
	fd_renumber
	ls
	random
	stat
	stdout
	write)

set(TestSources "")
foreach(TestName ${TestNames})
	list(APPEND TestSources ${TestName}.cpp)
	
	#if(WAVM_ENABLE_RUNTIME)
	#	add_test(
	#		NAME "wasi/${TestName}"
	#		COMMAND $<TARGET_FILE:wavm-run-wasi> "${CMAKE_CURRENT_LIST_DIR}/${TestName}.wasm")
	#endif()
endforeach()

add_custom_target(WASITests SOURCES ${TestSources})
