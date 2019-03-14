set(Sources
	wavm-c.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/wavm-c/wavm-c.h)

WAVM_ADD_LIB_COMPONENT(wavm-c
	SOURCES ${Sources} ${PublicHeaders}
	PRIVATE_LIB_COMPONENTS IR Logging Platform Runtime WASM WASTParse WASTPrint)