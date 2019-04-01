set(POSIXSources
	POSIX/Clock.cpp
	POSIX/Diagnostics.cpp
	POSIX/Event.cpp
	POSIX/Signal.cpp
	POSIX/File.cpp
	POSIX/Memory.cpp
	POSIX/Mutex.cpp
	POSIX/Thread.cpp
	POSIX/POSIXPrivate.h)

set(NonWavixSources
	POSIX/POSIX.S)

set(WindowsSources
	Windows/Clock.cpp
	Windows/Diagnostics.cpp
	Windows/Event.cpp
	Windows/Signal.cpp
	Windows/File.cpp
	Windows/Memory.cpp
	Windows/Mutex.cpp
	Windows/Thread.cpp
	Windows/WindowsPrivate.h)

set(Win32Sources
	Windows/Win32.asm)
	
set(Win64Sources
	Windows/Win64.asm)

set(Headers
	${WAVM_INCLUDE_DIR}/Platform/Clock.h
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
	list(APPEND Headers ${POSIXSources})
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(Sources ${WindowsSources} ${Win32Sources})
		set(NonCompiledSources ${Win64Sources} ${POSIXSources} ${NonWavixSources})
	else()
		set(Sources ${WindowsSources} ${Win64Sources})
		set(NonCompiledSources ${Win32Sources} ${POSIXSources} ${NonWavixSources})
	endif()
else()
	set(Sources ${POSIXSources})
	set(NonCompiledSources ${WindowsSources} ${Win32Sources} ${Win64Sources})

	# Using if(CMAKE_SYSTEM_NAME STREQUAL Wavix) doesn't work because Wavix is interpreted as a
	# reference to the Wavix target in this project.
	string(COMPARE EQUAL "${CMAKE_SYSTEM_NAME}" Wavix IS_WAVIX)
	if(NOT IS_WAVIX)
		list(APPEND Sources ${NonWavixSources})
	endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND PLATFORM_PRIVATE_LIBS pthread rt)
endif()

if(NOT MSVC AND WAVM_ENABLE_RUNTIME)
	list(APPEND PLATFORM_PRIVATE_LIBS dl WAVMUnwind)

	# This can't be set in the WAVMUnwind target's public include directories without adding the
	# libunwind headers to the install, so just set it manually for the Platform component.
	set(PLATFORM_PRIVATE_INCLUDE_DIRECTORIES ${WAVM_SOURCE_DIR}/ThirdParty/libunwind/include)
endif()

WAVM_ADD_LIB_COMPONENT(Platform
	SOURCES ${Sources}
	NONCOMPILED_SOURCES ${Headers} ${NonCompiledSources}
	PRIVATE_LIBS ${PLATFORM_PRIVATE_LIBS}
	PRIVATE_INCLUDE_DIRECTORIES ${PLATFORM_PRIVATE_INCLUDE_DIRECTORIES})