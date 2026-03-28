set(Sources
	Print.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/WASTPrint/WASTPrint.h)

WAVM_ADD_LIB_COMPONENT(WASTPrint
	SOURCES ${Sources}
	HEADERS ${PublicHeaders})
