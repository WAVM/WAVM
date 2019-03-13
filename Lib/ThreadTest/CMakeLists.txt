set(Sources ThreadTest.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/ThreadTest/ThreadTest.h)

WAVM_ADD_LIB_COMPONENT(ThreadTest
	SOURCES ${Sources} ${PublicHeaders}
	PRIVATE_LIB_COMPONENTS Logging Platform Runtime)
