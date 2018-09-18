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
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set_source_files_properties(Windows.asm PROPERTIES HEADER_FILE_ONLY TRUE)
	endif()
	set_source_files_properties(${POSIXSources} PROPERTIES HEADER_FILE_ONLY TRUE)
	enable_language(ASM_MASM)
else()
	set(Sources ${POSIXSources})

	# Using if(CMAKE_SYSTEM_NAME STREQUAL Wavix) doesn't work because Wavix is interpreted as a
	# reference to the Wavix target in this project.
	string(COMPARE EQUAL "${CMAKE_SYSTEM_NAME}" Wavix IS_WAVIX)
	if(IS_WAVIX)
		set_source_files_properties(POSIX.S PROPERTIES HEADER_FILE_ONLY TRUE)
	endif()
endif()

WAVM_ADD_LIBRARY(Platform ${Sources} ${PublicHeaders})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(Platform PRIVATE pthread rt)
endif()

if(NOT MSVC AND WAVM_ENABLE_RUNTIME)
	target_link_libraries(Platform PRIVATE dl WAVMUnwind)
endif()
