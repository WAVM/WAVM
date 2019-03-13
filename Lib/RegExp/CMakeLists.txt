set(Sources
	RegExp.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/RegExp/RegExp.h)

WAVM_ADD_LIB_COMPONENT(RegExp
	SOURCES ${Sources} ${PublicHeaders}
	PUBLIC_LIB_COMPONENTS NFA
	PRIVATE_LIB_COMPONENTS Platform)
