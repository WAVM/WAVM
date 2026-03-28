set(Sources
	RegExp.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/RegExp/RegExp.h)

WAVM_ADD_LIB_COMPONENT(RegExp
	SOURCES ${Sources}
	HEADERS ${PublicHeaders})
