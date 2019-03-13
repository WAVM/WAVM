set(Sources
	NFA.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/NFA/NFA.h)

WAVM_ADD_LIB_COMPONENT(NFA
	SOURCES ${Sources} ${PublicHeaders}
	PRIVATE_LIB_COMPONENTS Platform Logging)
