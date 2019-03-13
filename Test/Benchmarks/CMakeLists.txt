if(WAVM_ENABLE_RUNTIME)
	WAVM_ADD_EXECUTABLE(invoke-bench Testing/Benchmarks invoke-bench.cpp)
	target_link_libraries(invoke-bench PRIVATE IR Platform Logging Runtime)
endif()