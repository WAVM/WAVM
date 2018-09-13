cmake_minimum_required(VERSION 2.7.0)
project (WAVM C CXX ASM)

set(WAVM_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(WAVM_LIB_DIR ${WAVM_SOURCE_DIR}/Lib)
set(WAVM_INCLUDE_DIR ${WAVM_SOURCE_DIR}/Include)

# If no build type is specified, default to RelWithDebInfo
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "The type of build (Debug, Release, RelWithDebInfo, or MinSizeRel" FORCE)
endif()

# Enable MAXOSX_RPATH by default
cmake_policy(SET CMP0042 NEW)

enable_testing()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Put executables in the bin dir, and libraries in the lib dir.
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Allow includes relative to the source path.
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

	# Compile files in parallel.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

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
	# disable warning: structure was padded due to alignment specifier
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4324")
	# disable warning: unary minus operator applied to unsigned type, result still unsigned
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4146")

	add_definitions(-DENABLE_UBSAN=0)
else()
	# Use C++11
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

	# Compile with all warnings and fatal warnings
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

	# Disable zero-length printf format string warning.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-zero-length")

	# Disable RTTI
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
	
	# Ensure that even static libraries are relocatable so they can be linked into a .so
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

	# Enable asan by default in debug builds.
	if(CMAKE_BUILD_TYPE MATCHES "Debug")
		option(ENABLE_ASAN "enable ASAN" ON)
	else()
		option(ENABLE_ASAN "enable ASAN" OFF)
	endif()
	if(ENABLE_ASAN)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
		if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mllvm -asan-stack=0")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-command-line-argument")
		endif()
	endif()

	# Optionally enable the undefined-behavior sanitizer.
	option(ENABLE_UBSAN "enable UBSAN" OFF)
	if(ENABLE_UBSAN)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-sanitize-recover=undefined")
		add_definitions(-DENABLE_UBSAN=1)
	else()
		add_definitions(-DENABLE_UBSAN=0)
	endif()

	# Optionally enable the thread sanitizer.
	option(ENABLE_TSAN "enable TSAN" OFF)
	if(ENABLE_TSAN)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fomit-frame-pointer")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	option(ENABLE_LIBFUZZER "compile WAVM for use by clang/LLVM's libfuzzer")
	if(ENABLE_LIBFUZZER)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer-no-link")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-coverage=trace-cmp,trace-div,trace-gep")
	endif()
	add_definitions(-DENABLE_LIBFUZZER=${ENABLE_LIBFUZZER})
else()
	add_definitions(-DENABLE_LIBFUZZER=0)
endif()

option(ENABLE_RELEASE_ASSERTS "enable assertions in release builds" 0)
add_definitions(-DENABLE_RELEASE_ASSERTS=${ENABLE_RELEASE_ASSERTS})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DWAVM_DEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DWAVM_DEBUG=0")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DWAVM_DEBUG=0")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DWAVM_DEBUG=0")

option(WAVM_METRICS_OUTPUT "controls printing the timings of some operations to stdout" OFF)
if(WAVM_METRICS_OUTPUT)
	add_definitions("-DWAVM_METRICS_OUTPUT=1")
else()
	add_definitions("-DWAVM_METRICS_OUTPUT=0")
endif()

option(ENABLE_RUNTIME "enables the runtime components of WAVM" ON)

option(ENABLE_STATIC_LINKING "use static linking instead of dynamic for the WAVM libraries" ON)

if(NOT ENABLE_STATIC_LINKING AND MSVC)
	add_definitions("-DDLL_EXPORT=__declspec(dllexport)")
	add_definitions("-DDLL_IMPORT=__declspec(dllimport)")
else()
	add_definitions(-DDLL_EXPORT=)
	add_definitions(-DDLL_IMPORT=)
endif()

function(WAVM_ADD_LIBRARY TARGET_NAME)
	if(ENABLE_STATIC_LINKING)
		add_library(${TARGET_NAME} STATIC ${ARGN})
	else()
		add_library(${TARGET_NAME} SHARED ${ARGN})
	endif()
endfunction(WAVM_ADD_LIBRARY)

add_subdirectory(Examples)
add_subdirectory(Include/Inline)
add_subdirectory(Lib/IR)
add_subdirectory(Lib/Logging)
add_subdirectory(Lib/NFA)
add_subdirectory(Lib/Platform)
add_subdirectory(Lib/RegExp)
add_subdirectory(Lib/WASM)
add_subdirectory(Lib/WASTParse)
add_subdirectory(Lib/WASTPrint)
add_subdirectory(Programs/Assemble)
add_subdirectory(Programs/Disassemble)
add_subdirectory(Test)
add_subdirectory(ThirdParty/dtoa)
add_subdirectory(ThirdParty/xxhash)
add_subdirectory(ThirdParty/libunwind)

if(ENABLE_RUNTIME)
	add_subdirectory(Lib/Emscripten)
	add_subdirectory(Lib/LLVMJIT)
	add_subdirectory(Lib/Runtime)
	add_subdirectory(Lib/ThreadTest)
	add_subdirectory(Programs/wavix)
	add_subdirectory(Programs/wavm)
	add_subdirectory(Programs/wavm-compile)
endif()

# Create a dummy target to hold various files in the project root
add_custom_target(RootFiles SOURCES
	.clang-format
	.travis.yml
	azure-pipelines.yml
	azure-windows-build-job-template.yml
	LICENSE.md
	README.md
	run-clang-format.sh
	travis-build.sh
	WebAssembly.tmLanguage)
