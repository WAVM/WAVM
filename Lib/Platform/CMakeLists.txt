set(POSIXSources
	POSIX.cpp
	POSIX.S)

set(WindowsSources
	Windows.cpp
	Windows.asm)

set(PublicHeaders
	${WAVM_INCLUDE_DIR}/Platform/Defines.h
	${WAVM_INCLUDE_DIR}/Platform/Diagnostics.h
	${WAVM_INCLUDE_DIR}/Platform/Event.h
	${WAVM_INCLUDE_DIR}/Platform/Exception.h
	${WAVM_INCLUDE_DIR}/Platform/File.h
	${WAVM_INCLUDE_DIR}/Platform/Intrinsic.h
	${WAVM_INCLUDE_DIR}/Platform/Memory.h
	${WAVM_INCLUDE_DIR}/Platform/Mutex.h
	${WAVM_INCLUDE_DIR}/Platform/Thread.h)

if(MSVC)
	set(Sources ${POSIXSources} ${WindowsSources})
	enable_language(ASM_MASM)
else()
	set(Sources ${POSIXSources})
endif()

add_definitions(-DPLATFORM_API=DLL_EXPORT)

WAVM_ADD_LIBRARY(Platform ${Sources} ${PublicHeaders})
set_target_properties(Platform PROPERTIES FOLDER Libraries)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	# Link with dl on Linux for dladdr.
	target_link_libraries(Platform pthread rt)
endif()

if(NOT MSVC)
	include_directories(${WAVM_SOURCE_DIR}/ThirdParty/libunwind/include)
	target_link_libraries(Platform dl WAVMUnwind)
endif()
