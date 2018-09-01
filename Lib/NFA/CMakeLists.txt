set(Sources
	NFA.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/NFA/NFA.h)

add_definitions(-DNFA_API=DLL_EXPORT)

WAVM_ADD_LIBRARY(NFA ${Sources} ${PublicHeaders})
set_target_properties(NFA PROPERTIES FOLDER Libraries)

target_link_libraries(NFA Platform Logging)
