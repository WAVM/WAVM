if(WAVM_ENABLE_RUNTIME)
	WAVM_ADD_EXECUTABLE(invoke-bench
		FOLDER Testing/Benchmarks
		SOURCES invoke-bench.cpp
		PRIVATE_LIB_COMPONENTS IR Platform Logging Runtime)
endif()