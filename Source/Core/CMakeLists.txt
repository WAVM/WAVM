set(Sources
	Diagnostics.cpp
	POSIX.cpp
	Windows.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/Core/Core.h
	${WAVM_INCLUDE_DIR}/Core/Platform.h)
include_directories(${WAVM_INCLUDE_DIR}/Core)

add_definitions(-DCORE_API=DLL_EXPORT)

add_library(Core SHARED ${Sources} ${PublicHeaders})

# Link with dl on Linux for dladdr.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(Core dl pthread)
endif()
