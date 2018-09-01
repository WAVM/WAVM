set(Sources
	Print.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/WASTPrint/WASTPrint.h)

WAVM_ADD_LIBRARY(WASTPrint ${Sources} ${PublicHeaders})
set_target_properties(WASTPrint PROPERTIES FOLDER Libraries)

add_definitions(-DWASTPRINT_API=DLL_EXPORT)
target_link_libraries(WASTPrint IR)
