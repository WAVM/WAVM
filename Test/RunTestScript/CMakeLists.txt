if(WAVM_ENABLE_RUNTIME)
	WAVM_ADD_EXECUTABLE(RunTestScript
		FOLDER Testing
		SOURCES RunTestScript.cpp
		PRIVATE_LIB_COMPONENTS Logging IR WASTParse Runtime ThreadTest)
endif()