set(Sources
	Print.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/WASTPrint/WASTPrint.h)

WAVM_ADD_LIBRARY(WASTPrint ${Sources} ${PublicHeaders})
target_link_libraries(WASTPrint PRIVATE IR Platform)
