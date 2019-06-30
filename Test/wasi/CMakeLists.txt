set(TestNames
	append
	args
	cat
	clock
	fd_filestat_set_size
	fd_filestat_set_times
	fd_renumber
	ls
	mkdir
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
	#		COMMAND $<TARGET_FILE:wavm-run-wasi> "${CMAKE_CURRENT_LIST_DIR}/${TestName}.wasm")
	#endif()
endforeach()

add_custom_target(WASITests SOURCES ${TestSources})
