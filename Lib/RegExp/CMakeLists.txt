set(Sources
	RegExp.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/RegExp/RegExp.h)

WAVM_ADD_LIBRARY(RegExp ${Sources} ${PublicHeaders})
target_link_libraries(RegExp PUBLIC NFA PRIVATE Platform)
