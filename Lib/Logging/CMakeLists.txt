set(Sources
	Logging.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/Logging/Logging.h)

WAVM_ADD_LIB_COMPONENT(Logging 
	SOURCES ${Sources} ${PublicHeaders}
	PRIVATE_LIB_COMPONENTS Platform)
