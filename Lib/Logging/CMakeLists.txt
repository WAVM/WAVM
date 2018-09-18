set(Sources
	Logging.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/Logging/Logging.h)

WAVM_ADD_LIBRARY(Logging ${Sources} ${PublicHeaders})
target_link_libraries(Logging Platform)

# Link with dl on Linux for dladdr.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(Logging dl pthread)
endif()
