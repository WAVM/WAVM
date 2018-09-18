set(Sources
	NFA.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/NFA/NFA.h)

WAVM_ADD_LIBRARY(NFA ${Sources} ${PublicHeaders})
target_link_libraries(NFA PRIVATE Platform Logging)
