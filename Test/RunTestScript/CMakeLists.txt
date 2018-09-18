if(WAVM_ENABLE_RUNTIME)
	WAVM_ADD_EXECUTABLE(RunTestScript Testing RunTestScript.cpp)
	target_link_libraries(RunTestScript Logging IR WASTParse Runtime ThreadTest)
endif()