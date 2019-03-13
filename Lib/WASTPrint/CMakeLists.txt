set(Sources
	Print.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/WASTPrint/WASTPrint.h)

WAVM_ADD_LIB_COMPONENT(WASTPrint
	SOURCES ${Sources} ${PublicHeaders}
	PRIVATE_LIB_COMPONENTS IR Platform)
