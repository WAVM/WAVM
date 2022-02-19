ADD_WAST_TESTS(
	NAME_PREFIX wavm/
	SOURCES
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
		wavm_atomic.wast
	WAVM_ARGS --test-cloning --strict-assert-invalid --strict-assert-malformed --enable all)

if(WAVM_ENABLE_RUNTIME)
	# TODO: fix the memory leak in this test.
	set_tests_properties(wavm/exceptions.wast PROPERTIES ENVIRONMENT ASAN_OPTIONS=detect_leaks=0)
endif()
