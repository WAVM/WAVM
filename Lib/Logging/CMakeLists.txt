set(Sources
	Logging.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/Logging/Logging.h)

WAVM_ADD_LIBRARY(Logging ${Sources} ${PublicHeaders})
target_link_libraries(Logging PRIVATE Platform)
