cmake_minimum_required(VERSION 2.7.0)
project (WAVM)

enable_testing()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Put executables in the bin dir, and libraries in the lib dir.
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Allow includes relative to the source path.
include_directories(Source)

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
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
	endif()
endif()

option(WAVM_TIMER_OUTPUT "controls printing the timings of some operations to stdout" OFF)
if(WAVM_TIMER_OUTPUT)
	add_definitions("-DWAVM_TIMER_OUTPUT=1")
endif()

add_subdirectory(Source/Core)
add_subdirectory(Source/AST)
add_subdirectory(Source/WebAssembly)
add_subdirectory(Source/Runtime)
add_subdirectory(Source/Programs)
add_subdirectory(Source/ThirdParty)

add_subdirectory(Test/spec)