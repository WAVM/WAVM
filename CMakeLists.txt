cmake_minimum_required(VERSION 2.7.0)
project (WAVM)

enable_testing()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Put executables in the bin dir, and libraries in the lib dir.
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Allow includes relative to the source path.
set(WAVM_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/Include)
include_directories(${WAVM_INCLUDE_DIR})

if(MSVC)
	# Taken out of the VC projects to avoid the compiler complaining about POSIX code
	add_definitions(-D_SCL_SECURE_NO_WARNINGS)
	add_definitions(-D_SCL_SECURE_NO_DEPRECATE)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
	add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
	add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
	add_definitions(-D__STDC_CONSTANT_MACROS)
	add_definitions(-D__STDC_FORMAT_MACROS)
	add_definitions(-D__STDC_LIMIT_MACROS)

	# Compile with all warnings and fatal warnings
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()

	# disable warning: conditional expression is constant
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4127")
	# disable warning: unreferenced formal parameter
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100")
	# disable warning: assignment operator could not be generated
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4512")
	# disable warning: 'typedef ': ignored on left of '' when no variable is declared
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4091")
	# disable warning: 'inline': used more than once
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4141")
	# disable warning: cast truncates constant value
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4310")
	# disable warning: unreachable code
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4702")
	# disable warning: The result of the unary '&' operator may be unaligned
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4366")
else()
	# Use C++11
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

	# Compile with all warnings and fatal warnings
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

	# Disable RTTI
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
	
	# Ensure that even static libraries are relocatable so they can be linked into a .so
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

	# Enable asan in debug builds.
	if(CMAKE_BUILD_TYPE MATCHES "Debug")
		option(ENABLE_ASAN "enable ASAN" ON)
		add_definitions("-D_DEBUG")
	else()
		option(ENABLE_ASAN "enable ASAN" OFF)
	endif()
	if(ENABLE_ASAN)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
	endif()
endif()

option(WAVM_METRICS_OUTPUT "controls printing the timings of some operations to stdout" OFF)
if(WAVM_METRICS_OUTPUT)
	add_definitions("-DWAVM_METRICS_OUTPUT=1")
else()
	add_definitions("-DWAVM_METRICS_OUTPUT=0")
endif()

option(PRETEND_32BIT_ADDRESS_SPACE "forces 64-bit WAVM to reserve address-space as it would in a 32-bit process" OFF)
if(PRETEND_32BIT_ADDRESS_SPACE)
	add_definitions("-DPRETEND_32BIT_ADDRESS_SPACE=1")
else()
	add_definitions("-DPRETEND_32BIT_ADDRESS_SPACE=0")
endif()

option(ENABLE_SIMD_PROTOTYPE "enables the prototype implementation of the proposed WebAssembly SIMD extension" ON)
if(ENABLE_SIMD_PROTOTYPE)
	add_definitions("-DENABLE_SIMD_PROTOTYPE=1")
else()
	add_definitions("-DENABLE_SIMD_PROTOTYPE=0")
endif()

add_subdirectory(Include/Inline)

add_subdirectory(Source/Core)
add_subdirectory(Source/IR)
add_subdirectory(Source/WASM)
add_subdirectory(Source/WAST)
add_subdirectory(Source/Runtime)
add_subdirectory(Source/Emscripten)
add_subdirectory(Source/Programs)

add_subdirectory(Test/spec)