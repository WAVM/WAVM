cmake_minimum_required(VERSION 3.8.0)
project (WAVM C CXX ASM)

if(MSVC)
	enable_language(ASM_MASM)
endif()

# WAVM configuration options

option(WAVM_ENABLE_STATIC_LINKING "use static linking instead of dynamic for the WAVM libraries" OFF)
option(WAVM_ENABLE_RELEASE_ASSERTS "enable assertions in release builds" 0)
option(WAVM_METRICS_OUTPUT "controls printing the timings of some operations to stdout" OFF)
option(WAVM_ENABLE_LTO "use link-time optimization" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# The sanitizers are only available when compiling with Clang and GCC.
	option(WAVM_ENABLE_ASAN "enable ASAN" OFF)
	option(WAVM_ENABLE_UBSAN "enable UBSAN" OFF)
	option(WAVM_ENABLE_TSAN "enable TSAN" OFF)
else()
	set(WAVM_ENABLE_ASAN OFF)
	set(WAVM_ENABLE_UBSAN OFF)
	set(WAVM_ENABLE_TSAN OFF)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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

if(WAVM_ENABLE_RUNTIME)
	# Find an installed build of LLVM
	find_package(LLVM REQUIRED CONFIG)

	if(LLVM_VERSION_MAJOR LESS 6)
		message(FATAL_ERROR "WAVM requires LLVM version 6.0 or newer")
	endif()
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

# This is a hack around the fact that CMake doesn't allow generator expressions in INSTALL_RPATH.
# This means that platforms that need an RPATH but have multi-configuration generators (i.e. Xcode)
# can't install the shared libraries into a configuration-dependent directory, so only one
# configuration may be installed at a time.
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
	set(WAVM_DISABLE_PER_CONFIG_RPATH 1)
elseif(CMAKE_BUILD_TYPE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_BUILD_TYPE}")
	set(WAVM_DISABLE_PER_CONFIG_RPATH 0)
else()
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
	set(WAVM_DISABLE_PER_CONFIG_RPATH 1)
endif()

# If MSVC LTO is enabled, remove the /INCREMENTAL option from the link flags to avoid link warnings.
function(REMOVE_INCREMENTAL_FLAG INOUT_FLAGS)
	set(INOUT_FLAGS ${OUT_FLAGS_LOCAL} PARENT_SCOPE)
endfunction()
if(MSVC AND WAVM_ENABLE_LTO)
	string(REGEX REPLACE "[-/]INCREMENTAL" "/INCREMENTAL:NO " CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})
	string(REGEX REPLACE "[-/]INCREMENTAL" "/INCREMENTAL:NO " CMAKE_SHARED_LINKER_FLAGS_DEBUG ${CMAKE_SHARED_LINKER_FLAGS_DEBUG})
	string(REGEX REPLACE "[-/]INCREMENTAL" "/INCREMENTAL:NO " CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
	string(REGEX REPLACE "[-/]INCREMENTAL" "/INCREMENTAL:NO " CMAKE_EXE_LINKER_FLAGS_DEBUG ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
endif()

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
function(WAVM_SET_TARGET_COMPILE_OPTIONS TARGET_NAME)
	# Add the WAVM public include directory.
	target_include_directories(
		${TARGET_NAME}
		PUBLIC $<INSTALL_INTERFACE:include>
			   $<BUILD_INTERFACE:${WAVM_SOURCE_DIR}/Include>
			   $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/Include>
	)

	# Target C++11.
	target_compile_features(${TARGET_NAME} PUBLIC cxx_std_11)

	if(MSVC)
		if(WAVM_ENABLE_LTO)
			target_compile_options(${TARGET_NAME} PRIVATE "/GL")

			set_target_properties(${TARGET_NAME} PROPERTIES STATIC_LIBRARY_FLAGS "/LTCG")
			set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "/LTCG")
		endif()

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

		target_link_libraries(${TARGET_NAME} PRIVATE "-ignore:4199") # /DELAYLOAD:... ignored; no imports found from ...

	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	
		if(WAVM_ENABLE_LTO)
			target_compile_options(${TARGET_NAME} PRIVATE "-flto")
			target_link_libraries(${TARGET_NAME} PRIVATE "-flto")
		endif()

		# Compile with all warnings and fatal warnings
		target_compile_options(${TARGET_NAME} PRIVATE "-Wall")
		target_compile_options(${TARGET_NAME} PRIVATE "-Werror")
	
		# Disable RTTI to allow linking against a build of LLVM that was compiled without it.
		target_compile_options(${TARGET_NAME} PRIVATE "-fno-rtti")

		# Ensure that even static libraries are relocatable so they can be linked into a .so
		target_compile_options(${TARGET_NAME} PRIVATE "-fPIC")

		if(WAVM_ENABLE_ASAN)
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=address")
			target_link_libraries(${TARGET_NAME} PUBLIC "-fsanitize=address")
		endif()

		# Optionally enable the undefined-behavior sanitizer.
		if(WAVM_ENABLE_UBSAN)
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=undefined")
			target_link_libraries(${TARGET_NAME} PUBLIC -fsanitize=undefined)
			target_compile_options(${TARGET_NAME} PRIVATE "-fno-sanitize-recover=undefined")
		endif()

		# Optionally enable the thread sanitizer.
		if(WAVM_ENABLE_TSAN)
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=thread")
			target_link_libraries(${TARGET_NAME} PUBLIC -fsanitize=thread)
		endif()
		
		# Optionally enable Clang's libfuzzer.
		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WAVM_ENABLE_LIBFUZZER)
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=fuzzer")
			target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize-coverage=trace-cmp,trace-div,trace-gep")
		endif()

		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			# Link with the static sanitizer runtimes instead of the shared sanitize runtimes on GCC.
			# This matches the default behavior for Clang.
			if(WAVM_ENABLE_ASAN)
				target_link_libraries(${TARGET_NAME} PUBLIC "-static-libasan")
			endif()
			if(WAVM_ENABLE_UBSAN)
				target_link_libraries(${TARGET_NAME} PUBLIC "-static-libubsan")
			endif()
			if(WAVM_ENABLE_TSAN)
				target_link_libraries(${TARGET_NAME} PUBLIC "-static-libtsan")
			endif()
		endif()

		# Don't eliminate frame pointers: this makes thread forking work robustly if one of the
		# sanitizers requires a frame pointer, and makes ASAN's stack trace on malloc much better
		# without using the slow libunwind path.
		target_compile_options(${TARGET_NAME} PRIVATE "-fno-omit-frame-pointer")
	endif()
endfunction()

function(WAVM_INSTALL_TARGET TARGET_NAME)
	install(TARGETS ${TARGET_NAME}
			EXPORT WAVMInstallTargets
			LIBRARY DESTINATION $<IF:$<OR:$<CONFIG:RelWithDebInfo>,$<BOOL:${WAVM_DISABLE_PER_CONFIG_RPATH}>>,lib,lib/WAVM/$<CONFIG>>
			ARCHIVE DESTINATION $<IF:$<OR:$<CONFIG:RelWithDebInfo>,$<BOOL:${WAVM_DISABLE_PER_CONFIG_RPATH}>>,lib,lib/WAVM/$<CONFIG>>
			# Install RelWithDebInfo binaries to bin, and all others to bin/WAVM/config
			RUNTIME DESTINATION $<IF:$<OR:$<CONFIG:RelWithDebInfo>,$<BOOL:${WAVM_DISABLE_PER_CONFIG_RPATH}>>,bin,bin/WAVM/$<CONFIG>>)
endfunction()

# Older versions of CMake can't handle target_link_libraries on the monolithic WAVM library
# when invoked from a source directory other than the root directory where the WAVM target is
# defined. To get around this, accumulate the libraries this component needs to link with in an
# internal config variable, which seems to be the closest thing CMake has to a global variable.
# After processing all library components, this root directory CMakeLists.txt invokes
# target_link_libraries with the accumulated libraries.
set(WAVM_MONOLIB_PRIVATE_LIBS ${WAVM_MONOLIB_PRIVATE_LIBS} CACHE INTERNAL "" FORCE)
set(WAVM_MONOLIB_PUBLIC_LIBS ${WAVM_MONOLIB_PUBLIC_LIBS} CACHE INTERNAL "" FORCE)

function(WAVM_ADD_LIB_COMPONENT COMPONENT_NAME)
	cmake_parse_arguments(COMPONENT
		""
		""
		"SOURCES;NONCOMPILED_SOURCES;PRIVATE_LIBS;PUBLIC_LIBS;PRIVATE_LIB_COMPONENTS;PUBLIC_LIB_COMPONENTS;PRIVATE_INCLUDE_DIRECTORIES;PRIVATE_DEFINITIONS"
		${ARGN})

	# Translate the relative source and header file paths to absolute paths.
	# Older versions of CMake will use the source directory where a target is defined as the root
	# for relative paths in the target's sources, which breaks when adding the source files to The
	# monolithic WAVM library defined in the root directory.
	foreach(COMPONENT_SOURCE ${COMPONENT_SOURCES})
		get_filename_component(COMPONENT_SOURCE_ABSOLUTE ${COMPONENT_SOURCE} ABSOLUTE)
		list(APPEND COMPONENT_SOURCES_ABSOLUTE ${COMPONENT_SOURCE_ABSOLUTE})
	endforeach()
	foreach(COMPONENT_NONCOMPILED_SOURCE ${COMPONENT_NONCOMPILED_SOURCES})
		get_filename_component(COMPONENT_NONCOMPILED_SOURCE_ABSOLUTE ${COMPONENT_NONCOMPILED_SOURCE} ABSOLUTE)
		list(APPEND COMPONENT_NONCOMPILED_SOURCES_ABSOLUTE ${COMPONENT_NONCOMPILED_SOURCE_ABSOLUTE})			
	endforeach()
	
	# Create a custom target to hold all the source files. Without this, CMake won't include The
	# CMakeLists.txt that called WAVM_ADD_LIB_COMPONENT in the MSVC solution.
	add_custom_target(${COMPONENT_NAME} SOURCES ${COMPONENT_SOURCES} ${COMPONENT_NONCOMPILED_SOURCES_ABSOLUTE})
	set_target_properties(${COMPONENT_NAME} PROPERTIES FOLDER Libraries)

	# Directly add the component's source files to the monolithic WAVM library.
	target_sources(WAVM PRIVATE ${COMPONENT_SOURCES_ABSOLUTE})

	# Add the libraries this component depends on to the global list of libraries to link the
	# monolithic WAVM library with.
	list(APPEND WAVM_MONOLIB_PRIVATE_LIBS ${COMPONENT_PRIVATE_LIBS})
	list(APPEND WAVM_MONOLIB_PUBLIC_LIBS ${COMPONENT_PUBLIC_LIBS})
	set(WAVM_MONOLIB_PRIVATE_LIBS ${WAVM_MONOLIB_PRIVATE_LIBS} CACHE INTERNAL "" FORCE)
	set(WAVM_MONOLIB_PUBLIC_LIBS ${WAVM_MONOLIB_PUBLIC_LIBS} CACHE INTERNAL "" FORCE)
	
	# Add the component's include directories and definitions.
	target_include_directories(WAVM PRIVATE ${COMPONENT_PRIVATE_INCLUDE_DIRECTORIES})
	target_compile_definitions(WAVM PRIVATE ${COMPONENT_PRIVATE_DEFINITIONS})

	# Set up the component's API definitions.
	string(TOUPPER ${COMPONENT_NAME} COMPONENT_C_NAME)
	string(REPLACE "-" "_" COMPONENT_C_NAME ${COMPONENT_C_NAME})
	if(NOT WAVM_ENABLE_STATIC_LINKING AND MSVC)
		target_compile_definitions(WAVM PRIVATE "\"${COMPONENT_C_NAME}_API=__declspec(dllexport)\"")
		target_compile_definitions(WAVM INTERFACE "\"${COMPONENT_C_NAME}_API=__declspec(dllimport)\"")
	else()
		target_compile_definitions(WAVM PUBLIC "${COMPONENT_C_NAME}_API=")
	endif()
endfunction()

function(WAVM_ADD_EXECUTABLE EXE_NAME)
	cmake_parse_arguments(EXE
		""
		"FOLDER"
		"SOURCES;PRIVATE_LIBS;PUBLIC_LIBS;PRIVATE_LIB_COMPONENTS;PUBLIC_LIB_COMPONENTS"
		${ARGN})

	add_executable(${EXE_NAME} ${EXE_SOURCES})
	WAVM_SET_TARGET_COMPILE_OPTIONS(${EXE_NAME})

	# Add the executable's link libraries.
	target_link_libraries(${EXE_NAME} PRIVATE ${EXE_PRIVATE_LIBS})
	target_link_libraries(${EXE_NAME} PUBLIC ${EXE_PUBLIC_LIBS})

	# Ignore the PRIVATE_LIB_COMPONENTS and PUBLIC_LIB_COMPONENTS, and just link the executable with
	# the monolithic WAVM library.
	target_link_libraries(${EXE_NAME} PRIVATE WAVM)

	if(EXE_FOLDER)
		set_target_properties(${EXE_NAME} PROPERTIES FOLDER ${EXE_FOLDER})
	endif()
endfunction()

# Create a WAVM library that will include all WAVM library components.
if(WAVM_ENABLE_STATIC_LINKING)
	add_library(WAVM STATIC)
else()
	add_library(WAVM SHARED)
endif()
WAVM_SET_TARGET_COMPILE_OPTIONS(WAVM)
WAVM_INSTALL_TARGET(WAVM)
set_target_properties(WAVM PROPERTIES FOLDER Libraries)
set_target_properties(WAVM PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

# Process the CMake scripts in subdirectories.
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

# Add the library dependencies accumulated from the various library components as link dependencies
# of the monolithic WAVM library.
target_link_libraries(WAVM PRIVATE ${WAVM_MONOLIB_PRIVATE_LIBS})
target_link_libraries(WAVM PUBLIC ${WAVM_MONOLIB_PUBLIC_LIBS})

# Create a CMake package in <build>/lib/cmake/WAVM containing the WAVM library targets.
export(
	EXPORT WAVMInstallTargets
	FILE ${CMAKE_CURRENT_BINARY_DIR}/lib/cmake/WAVM/WAVMConfig.cmake
	NAMESPACE WAVM::)

# Create a CMake package in <install root>/lib/cmake/WAVM containing the WAVM library targets.
install(
	EXPORT WAVMInstallTargets
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
