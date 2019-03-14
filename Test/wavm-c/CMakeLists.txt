WAVM_ADD_EXECUTABLE(wavm-c-test
	FOLDER Testing
	SOURCES wavm-c-test.c
	PRIVATE_LIB_COMPONENTS wavm-c)
add_test(NAME wavm-c-test COMMAND $<TARGET_FILE:wavm-c-test>)
