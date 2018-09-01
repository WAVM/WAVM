set(Sources
	RegExp.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/RegExp/RegExp.h)

add_definitions(-DREGEXP_API=DLL_EXPORT)

WAVM_ADD_LIBRARY(RegExp ${Sources} ${PublicHeaders})
set_target_properties(RegExp PROPERTIES FOLDER Libraries)

target_link_libraries(RegExp NFA)
