cmake_minimum_required(VERSION 3.8.0)
project (WAVM C CXX ASM)


# WAVM configuration options

option(WAVM_ENABLE_STATIC_LINKING "use static linking instead of dynamic for the WAVM libraries" OFF)
option(WAVM_ENABLE_RELEASE_ASSERTS "enable assertions in release builds" 0)
option(WAVM_METRICS_OUTPUT "controls printing the timings of some operations to stdout" OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# The sanitizers are only available when compiling with Clang and GCC.
	option(WAVM_ENABLE_ASAN "enable ASAN" OFF)
	option(WAVM_ENABLE_UBSAN "enable UBSAN" OFF)
	option(WAVM_ENABLE_TSAN "enable TSAN" OFF)
else()
	set(WAVM_ENABLE_ASAN OFF)
	set(WAVM_ENABLE_UBSAN OFF)
	set(WAVM_ENABLE_TSAN OFF)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# libfuzzer is only available when compiling with Clang.
	option(WAVM_ENABLE_LIBFUZZER "compile WAVM for use by clang/LLVM's libfuzzer" OFF)
else()
	set(WAVM_ENABLE_LIBFUZZER OFF)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	# Disable the runtime on 32-bit platforms.
	set(WAVM_ENABLE_RUNTIME OFF)
else()
	# Allow disabling the runtime on 64-bit platforms.
	option(WAVM_ENABLE_RUNTIME "enables the runtime components of WAVM" ON)
endif()



# Bind some variables to useful paths.
set(WAVM_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(WAVM_INCLUDE_DIR ${WAVM_SOURCE_DIR}/Include/WAVM)

# If no build type is specified, default to RelWithDebInfo
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "The type of build (Debug, Release, RelWithDebInfo, or MinSizeRel" FORCE)
endif()

# Enable MAXOSX_RPATH by default
cmake_policy(SET CMP0042 NEW)

# Enable cmake's testing infrastructure
enable_testing()

# Enable folders when generating Visual Studio solutions
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Put executables/DLLs in the <build>/bin directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Put libraries/SOs in the <build|install>/lib/WAVM directory.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/WAVM)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/WAVM")


# Install Include/WAVM to <install root>/include/WAVM
install(DIRECTORY ${WAVM_SOURCE_DIR}/Include/WAVM
		DESTINATION include
		PATTERN *.txt EXCLUDE
		PATTERN *.h.in EXCLUDE)

# Generate Inline/Config.h in the build/install directories from Inline/Config.h.in in the source
configure_file(${WAVM_SOURCE_DIR}/Include/WAVM/Inline/Config.h.in
			   ${PROJECT_BINARY_DIR}/Include/WAVM/Inline/Config.h)
install(FILES ${PROJECT_BINARY_DIR}/Include/WAVM/Inline/Config.h
		DESTINATION include/WAVM/Inline)


# A function that sets compile options that are common to all WAVM targets.
function(WAVM_SET_TARGET_OPTIONS TARGET_NAME)
	# Add the WAVM public include directory.
	target_include_directories(
		${TARGET_NAME}
		INTERFACE $<INSTALL_INTERFACE:include>
		PRIVATE $<BUILD_INTERFACE:${WAVM_SOURCE_DIR}/Include>
				${PROJECT_BINARY_DIR}/Include
	)

	# Target C++11.
	target_compile_features(${TARGET_NAME} PUBLIC cxx_std_11)

	if(MSVC)
		# Compile files in parallel.
		target_compile_options(${TARGET_NAME} PRIVATE "/MP")

		# Compile with all warnings, and fatal warnings.
		target_compile_options(${TARGET_NAME} PRIVATE "/W4")
		target_compile_options(${TARGET_NAME} PRIVATE "/WX")

		# Disable warnings
		target_compile_options(${TARGET_NAME} PRIVATE "/wd4127") # conditional expression is constant
		target_compile_options(${TARGET_NAME} PRIVATE "/wd4100") # unreferenced formal parameter
		target_compile_options(${TARGET_NAME} PRIVATE "/wd4512") # assignment operator could not be generated
		target_compile_options(${TARGET_NAME} PRIVATE "/wd4141") # 'inline': used more than once
		target_compile_options(${TARGET_NAME} PRIVATE "/wd4310") # cast truncates constant value
		target_compile_options(${TARGET_NAME} PRIVATE "/wd4324") # structure was padded due to alignment specifier
		target_compile_options(${TARGET_NAME} PRIVATE "/wd4146") # unary minus operator applied to unsigned type, result still unsigned
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		# Compile with all warnings and fatal warnings
		target_compile_options(${TARGET_NAME} PRIVATE "-Wall")
		target_compile_options(${TARGET_NAME} PRIVATE "-Werror")
	
		# Disable RTTI to allow linking against a build of LLVM that was compiled without it.
		target_compile_options(${TARGET_NAME} PRIVATE "-fno-rtti")

		# Ensure that even static libraries are relocatable so they can be linked into a .so
		target_compile_options(${TARGET_NAME} PRIVATE "-fPIC")

		if(WAVM_ENABLE_ASAN)
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=address")
			if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
				target_compile_options(${TARGET_NAME} PRIVATE "-mllvm -asan-stack=0")
				target_compile_options(${TARGET_NAME} PRIVATE "-Wno-unused-command-line-argument")
			endif()
		endif()

		# Optionally enable the undefined-behavior sanitizer.
		if(WAVM_ENABLE_UBSAN)
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=undefined")
			target_compile_options(${TARGET_NAME} PRIVATE "-fno-sanitize-recover=undefined")
		endif()

		# Optionally enable the thread sanitizer.
		if(WAVM_ENABLE_TSAN)
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=thread")
		endif()

		# Make sure to omit the frame pointer in order to allow stuff like thread forking to work.
		target_compile_options(${TARGET_NAME} PRIVATE "-fomit-frame-pointer")
	endif()
endfunction()

function(WAVM_ADD_LIBRARY TARGET_NAME)
	if(WAVM_ENABLE_STATIC_LINKING)
		add_library(${TARGET_NAME} STATIC ${ARGN})
		install(TARGETS ${TARGET_NAME} DESTINATION lib/WAVM EXPORT WAVMLibraries)
	else()
		add_library(${TARGET_NAME} SHARED ${ARGN})
		if(WIN32)
			install(TARGETS ${TARGET_NAME} DESTINATION bin EXPORT WAVMLibraries)
		else()
			install(TARGETS ${TARGET_NAME} DESTINATION lib/WAVM EXPORT WAVMLibraries)
		endif()
	endif()

	WAVM_SET_TARGET_OPTIONS(${TARGET_NAME})
	
	# Set up the library's API definitions.
	string(TOUPPER ${TARGET_NAME} TARGET_NAME_UPPERCASE)
	if(NOT WAVM_ENABLE_STATIC_LINKING AND MSVC)
		target_compile_definitions(${TARGET_NAME} PRIVATE "\"${TARGET_NAME_UPPERCASE}_API=__declspec(dllexport)\"")
		target_compile_definitions(${TARGET_NAME} INTERFACE "\"${TARGET_NAME_UPPERCASE}_API=__declspec(dllimport)\"")
	else()
		target_compile_definitions(${TARGET_NAME} PUBLIC "${TARGET_NAME_UPPERCASE}_API=")
	endif()

	# Enable libfuzzer for libraries.
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		if(WAVM_ENABLE_LIBFUZZER)
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=fuzzer-no-link")
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize-coverage=trace-cmp,trace-div,trace-gep")
		endif()
	endif()
	
	set_target_properties(${TARGET_NAME} PROPERTIES FOLDER Libraries)
endfunction()

function(WAVM_ADD_EXECUTABLE TARGET_NAME FOLDER_NAME)
	add_executable("${TARGET_NAME}" ${ARGN})
	set_target_properties(${TARGET_NAME} PROPERTIES FOLDER ${FOLDER_NAME})
	WAVM_SET_TARGET_OPTIONS(${TARGET_NAME})
endfunction()

function(WAVM_ADD_INSTALLED_EXECUTABLE TARGET_NAME FOLDER_NAME)
	WAVM_ADD_EXECUTABLE(${TARGET_NAME} ${FOLDER_NAME} ${ARGN})
	install(TARGETS ${TARGET_NAME} DESTINATION bin)
endfunction()

add_subdirectory(Examples)
add_subdirectory(Include/WAVM/Inline)
add_subdirectory(Lib/IR)
add_subdirectory(Lib/Logging)
add_subdirectory(Lib/NFA)
add_subdirectory(Lib/Platform)
add_subdirectory(Lib/RegExp)
add_subdirectory(Lib/WASM)
add_subdirectory(Lib/WASTParse)
add_subdirectory(Lib/WASTPrint)
add_subdirectory(Programs/wavm-as)
add_subdirectory(Programs/wavm-disas)
add_subdirectory(Test)
add_subdirectory(ThirdParty/dtoa)

if(WAVM_ENABLE_RUNTIME)
	add_subdirectory(Lib/Emscripten)
	add_subdirectory(Lib/LLVMJIT)
	add_subdirectory(Lib/Runtime)
	add_subdirectory(Lib/ThreadTest)
	add_subdirectory(Programs/wavm-compile)
	add_subdirectory(Programs/wavm-run)
	add_subdirectory(ThirdParty/libunwind)
endif()

# Create a CMake package in <install root>/lib/cmake/WAVM containing the WAVM library targets.
install(
	EXPORT WAVMLibraries
	FILE WAVMConfig.cmake
	DESTINATION lib/cmake/WAVM
	NAMESPACE WAVM::)

# Create a dummy target to hold various files in the project root
add_custom_target(RootFiles SOURCES
	.clang-format
	.travis.yml
	azure-pipelines.yml
	Build/azure-windows-build-job-template.yml
	Build/notify-discord.sh
	Build/travis-build.sh
	LICENSE.md
	README.md
	run-clang-format.sh
	WebAssembly.tmLanguage)
