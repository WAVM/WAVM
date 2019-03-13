WAVM_ADD_EXECUTABLE(HashSetTest
	FOLDER Testing
	SOURCES HashSetTest.cpp
	PRIVATE_LIB_COMPONENTS Platform Logging)
add_test(NAME HashSetTest COMMAND $<TARGET_FILE:HashSetTest>)

WAVM_ADD_EXECUTABLE(HashMapTest
	FOLDER Testing
	SOURCES HashMapTest.cpp
	PRIVATE_LIB_COMPONENTS Platform Logging)
add_test(NAME HashMapTest COMMAND $<TARGET_FILE:HashMapTest>)