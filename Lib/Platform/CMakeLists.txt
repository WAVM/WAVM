set(POSIXSources
	POSIX/Diagnostics.cpp
	POSIX/Event.cpp
	POSIX/Signal.cpp
	POSIX/File.cpp
	POSIX/Memory.cpp
	POSIX/Mutex.cpp
	POSIX/Thread.cpp
	POSIX/POSIX.S
	POSIX/POSIXPrivate.h)

set(WindowsSources
	Windows/Diagnostics.cpp
	Windows/Event.cpp
	Windows/Signal.cpp
	Windows/File.cpp
	Windows/Memory.cpp
	Windows/Mutex.cpp
	Windows/Thread.cpp
	Windows/WindowsPrivate.h
	Windows/Win32.asm
	Windows/Win64.asm)

set(PublicHeaders
	${WAVM_INCLUDE_DIR}/Platform/Defines.h
	${WAVM_INCLUDE_DIR}/Platform/Diagnostics.h
	${WAVM_INCLUDE_DIR}/Platform/Event.h
	${WAVM_INCLUDE_DIR}/Platform/Signal.h
	${WAVM_INCLUDE_DIR}/Platform/File.h
	${WAVM_INCLUDE_DIR}/Platform/Intrinsic.h
	${WAVM_INCLUDE_DIR}/Platform/Memory.h
	${WAVM_INCLUDE_DIR}/Platform/Mutex.h
	${WAVM_INCLUDE_DIR}/Platform/Thread.h)

if(MSVC)
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set_source_files_properties(Windows/Win64.asm PROPERTIES HEADER_FILE_ONLY TRUE)
		set_source_files_properties(Windows/Win32.asm PROPERTIES COMPILE_FLAGS "/safeseh")
	else()
		set_source_files_properties(Windows/Win32.asm PROPERTIES HEADER_FILE_ONLY TRUE)
	endif()
	set_source_files_properties(${POSIXSources} PROPERTIES HEADER_FILE_ONLY TRUE)
	enable_language(ASM_MASM)
	
	set_source_files_properties(${POSIXSources} PROPERTIES HEADER_FILE_ONLY TRUE)
else()
	set(Sources ${POSIXSources})

	# Using if(CMAKE_SYSTEM_NAME STREQUAL Wavix) doesn't work because Wavix is interpreted as a
	# reference to the Wavix target in this project.
	string(COMPARE EQUAL "${CMAKE_SYSTEM_NAME}" Wavix IS_WAVIX)
	if(IS_WAVIX)
		set_source_files_properties(POSIX/POSIX.S PROPERTIES HEADER_FILE_ONLY TRUE)
	endif()

	set_source_files_properties(${WindowsSources} PROPERTIES HEADER_FILE_ONLY TRUE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND PLATFORM_PRIVATE_LIBS pthread rt)
endif()

if(NOT MSVC AND WAVM_ENABLE_RUNTIME)
	list(APPEND PLATFORM_PRIVATE_LIBS dl WAVMUnwind)

	# This can't be set in the WAVMUnwind target's public include directories without adding the
	# libunwind headers to the install, so just set it manually for source files in this directory.
	include_directories(${WAVM_SOURCE_DIR}/ThirdParty/libunwind/include)
endif()

WAVM_ADD_LIB_COMPONENT(Platform
	SOURCES ${WindowsSources} ${POSIXSources} ${PublicHeaders}
	PRIVATE_LIBS ${PLATFORM_PRIVATE_LIBS})
