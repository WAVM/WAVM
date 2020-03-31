include(CheckSymbolExists)

set(POSIXSources
	POSIX/ClockPOSIX.cpp
	POSIX/DiagnosticsPOSIX.cpp
	POSIX/ErrorPOSIX.cpp
	POSIX/EventPOSIX.cpp
	POSIX/SignalPOSIX.cpp
	POSIX/FilePOSIX.cpp
	POSIX/MemoryPOSIX.cpp
	POSIX/MutexPOSIX.cpp
	POSIX/RandomPOSIX.cpp
	POSIX/RWMutexPOSIX.cpp
	POSIX/ThreadPOSIX.cpp
	POSIX/POSIXPrivate.h)

set(POSIXSourcesX86_64  POSIX/POSIX-X86_64.S)
set(POSIXSourcesAArch64 POSIX/POSIX-AArch64.S)

set(WindowsSources
	Windows/ClockWindows.cpp
	Windows/DiagnosticsWindows.cpp
	Windows/ErrorWindows.cpp
	Windows/EventWindows.cpp
	Windows/SignalWindows.cpp
	Windows/FileWindows.cpp
	Windows/MemoryWindows.cpp
	Windows/MutexWindows.cpp
	Windows/RandomWindows.cpp
	Windows/RWMutexWindows.cpp
	Windows/ThreadWindows.cpp
	Windows/WindowsPrivate.h)

set(Win32Sources Windows/Win32.asm)
set(Win64Sources Windows/Win64.asm)

set(Headers
	${WAVM_INCLUDE_DIR}/Platform/Clock.h
	${WAVM_INCLUDE_DIR}/Platform/Defines.h
	${WAVM_INCLUDE_DIR}/Platform/Diagnostics.h
	${WAVM_INCLUDE_DIR}/Platform/Error.h
	${WAVM_INCLUDE_DIR}/Platform/Event.h
	${WAVM_INCLUDE_DIR}/Platform/Signal.h
	${WAVM_INCLUDE_DIR}/Platform/File.h
	${WAVM_INCLUDE_DIR}/Platform/Intrinsic.h
	${WAVM_INCLUDE_DIR}/Platform/Memory.h
	${WAVM_INCLUDE_DIR}/Platform/Mutex.h
	${WAVM_INCLUDE_DIR}/Platform/RWMutex.h
	${WAVM_INCLUDE_DIR}/Platform/Thread.h)

if(MSVC)
	list(APPEND Headers ${POSIXSources})
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(Sources ${WindowsSources} ${Win32Sources})
		set(NonCompiledSources ${Win64Sources} ${POSIXSources} ${POSIXSourcesX86_64} ${POSIXSourcesAArch64})
	else()
		set(Sources ${WindowsSources} ${Win64Sources})
		set(NonCompiledSources ${Win32Sources} ${POSIXSources} ${POSIXSourcesX86_64} ${POSIXSourcesAArch64})
	endif()
else()
	# Prevent check_symbol_exists from using API that is not supported for a given deployment target.
	check_c_compiler_flag("-Werror=unguarded-availability-new" C_HAS_WERROR_UNGUARDED_AVAILABILITY_NEW)
	if(C_HAS_WERROR_UNGUARDED_AVAILABILITY_NEW)
		set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror=unguarded-availability-new")
	endif()

	# futimens/utimensat isn't available on MacOS until 10.13.
	check_symbol_exists(futimens sys/stat.h HAS_FUTIMENS)
	if(HAS_FUTIMENS)
		list(APPEND PLATFORM_PRIVATE_DEFINITIONS "HAS_FUTIMENS")
	endif()
	check_symbol_exists(utimensat sys/stat.h HAS_UTIMENSAT)
	if(HAS_FUTIMENS)
		list(APPEND PLATFORM_PRIVATE_DEFINITIONS "HAS_UTIMENSAT")
	endif()

	if(WAVM_ENABLE_ASAN)
		# Check whether __sanitizer_print_memory_profile is defined in sanitizer/common_interface_defs.h
		set(CMAKE_REQUIRED_FLAGS_SAVED ${CMAKE_REQUIRED_FLAGS})
		set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fsanitize=address")
		check_c_source_compiles(
			"#include <sanitizer/common_interface_defs.h>\n\
			void main() { __sanitizer_print_memory_profile(100); }"
			HAS_SANITIZER_PRINT_MEMORY_PROFILE_1)
		check_c_source_compiles(
			"#include <sanitizer/common_interface_defs.h>\n\
			void main() { __sanitizer_print_memory_profile(100, 20); }"
			HAS_SANITIZER_PRINT_MEMORY_PROFILE_2)
		set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVED})

		if(HAS_SANITIZER_PRINT_MEMORY_PROFILE_2)
			list(APPEND PLATFORM_PRIVATE_DEFINITIONS "HAS_SANITIZER_PRINT_MEMORY_PROFILE_2")
		elseif(HAS_SANITIZER_PRINT_MEMORY_PROFILE_1)
			list(APPEND PLATFORM_PRIVATE_DEFINITIONS "HAS_SANITIZER_PRINT_MEMORY_PROFILE_1")
		endif()
	endif()

	set(Sources ${POSIXSources})
	set(NonCompiledSources ${WindowsSources} ${Win32Sources} ${Win64Sources})

	if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
		list(APPEND Sources ${POSIXSourcesX86_64})
	elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
		list(APPEND Sources ${POSIXSourcesAArch64})
	endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND PLATFORM_PRIVATE_LIBS rt)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	list(APPEND PLATFORM_PRIVATE_LIBS "Bcrypt.lib")
endif()

if(NOT MSVC)
	list(APPEND PLATFORM_PRIVATE_LIBS dl)
endif()

if(NOT MSVC AND WAVM_ENABLE_UNWIND)
	list(APPEND PLATFORM_PRIVATE_LIBS WAVMUnwind)
	
	# This can't be set in the WAVMUnwind target's public include directories without adding the
	# libunwind headers to the install, so just set it manually for the Platform component.
	set(PLATFORM_PRIVATE_INCLUDE_DIRECTORIES ${WAVM_SOURCE_DIR}/ThirdParty/libunwind/include)
endif()

WAVM_ADD_LIB_COMPONENT(Platform
	SOURCES ${Sources}
	NONCOMPILED_SOURCES ${Headers} ${NonCompiledSources}
	PRIVATE_LIBS ${PLATFORM_PRIVATE_LIBS}
	PRIVATE_INCLUDE_DIRECTORIES ${PLATFORM_PRIVATE_INCLUDE_DIRECTORIES}
	PRIVATE_DEFINITIONS ${PLATFORM_PRIVATE_DEFINITIONS})
