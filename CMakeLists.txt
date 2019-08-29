cmake_minimum_required(VERSION 3.8.0)
project (WAVM C CXX ASM)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# Parse the VERSION file.
file(READ "VERSION" VERSION_FILE_CONTENTS)
string(REGEX MATCH "([0-9]+)\.([0-9]+)\.([0-9]+)" WAVM_VERSION ${VERSION_FILE_CONTENTS})
if(NOT WAVM_VERSION)
	message(FATAL_ERROR "Could not parse 'VERSION' file")
endif()
set(WAVM_VERSION_MAJOR ${CMAKE_MATCH_1})
set(WAVM_VERSION_MINOR ${CMAKE_MATCH_2})
set(WAVM_VERSION_PATCH ${CMAKE_MATCH_3})

# Configure CPack
set(CPACK_PACKAGE_NAME "WAVM")
set(CPACK_PACKAGE_VENDOR "Andrew Scheidecker")
set(CPACK_PACKAGE_CONTACT "andrew@scheidecker.net")
set(CPACK_PACKAGE_VERSION_MAJOR ${WAVM_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${WAVM_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${WAVM_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION "WebAssembly virtual machine")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://wavm.github.io")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "WAVM")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.15), libgcc1 (>= 1:3.3), libstdc++6 (>= 5.2), libtinfo5 (>= 6), zlib1g (>= 1:1.1.4)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://wavm.github.io")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)

if(MSVC)
	enable_language(ASM_MASM)
endif()

# WAVM configuration options

option(WAVM_ENABLE_STATIC_LINKING "use static linking instead of dynamic for the WAVM libraries" OFF)
option(WAVM_ENABLE_RELEASE_ASSERTS "enable assertions in release builds" 0)
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

# On GCC/Clang, provide an option to compile the static libraries as PIC: Position-Independent Code.
# With this option disabled, the static libraries may not be linked into shared libraries.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if(WAVM_ENABLE_STATIC_LINKING)
		option(WAVM_ENABLE_STATIC_LIBRARY_PIC "create position-independent static libraries" ON)
	endif()
endif()

if(WAVM_ENABLE_RUNTIME)
	# Find an installed build of LLVM
	find_package(LLVM REQUIRED CONFIG)

	if(LLVM_VERSION_MAJOR LESS 6)
		message(FATAL_ERROR "WAVM requires LLVM version 6.0 or newer")
	endif()

	# Convert LLVM_DEFINITIONS and LLVM_INCLUDE_DIRS from strings of space-separated elements to
	# CMake lists (strings with semicolon-separated elements).
	separate_arguments(LLVM_DEFINITIONS)
	separate_arguments(LLVM_INCLUDE_DIRS)
	
	option(WAVM_ENABLE_UNWIND "enables printing stack traces" ON)
else()
	set(WAVM_ENABLE_UNWIND OFF)
endif()

# Tell MASM to create SAFESEH-compatible object files on Win32.
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /safeseh")
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

# Generate Inline/Version.h in the build/install directories from Inline/Version.h.in in the source
configure_file(${WAVM_SOURCE_DIR}/Include/WAVM/Inline/Version.h.in
			   ${PROJECT_BINARY_DIR}/Include/WAVM/Inline/Version.h)
install(FILES ${PROJECT_BINARY_DIR}/Include/WAVM/Inline/Version.h
		DESTINATION include/WAVM/Inline)

# Install LICENSE to <install root> on Windows and <install root>/share/wavm on other systems.
install(FILES ${WAVM_SOURCE_DIR}/LICENSE
		DESTINATION $<IF:$<PLATFORM_ID:Windows>,.,share/wavm>)

# Install the Examples directory to <install root>/examples on Windows, and <install root>/share/wavm/examples on other systems.
install(DIRECTORY ${WAVM_SOURCE_DIR}/Examples/
		DESTINATION $<IF:$<PLATFORM_ID:Windows>,examples,share/wavm/examples>
		PATTERN *.txt EXCLUDE)

set(OPTIONAL_C_FLAGS)
set(OPTIONAL_CXX_FLAGS)
function(ADD_COMPILER_FLAG_IF_SUPPORTED OPTIONAL_FLAG)
	check_c_compiler_flag(${OPTIONAL_FLAG} C_HAS_FLAG)
	check_cxx_compiler_flag(${OPTIONAL_FLAG} CXX_HAS_FLAG)
	if(C_HAS_FLAG)
		list(APPEND OPTIONAL_C_FLAGS ${OPTIONAL_FLAG})
	endif()
	if(CXX_HAS_FLAG)
		list(APPEND OPTIONAL_CXX_FLAGS ${OPTIONAL_FLAG})
	endif()
endfunction()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	ADD_COMPILER_FLAG_IF_SUPPORTED("-Wnull-dereference")
	ADD_COMPILER_FLAG_IF_SUPPORTED("-Wduplicated-cond")
	ADD_COMPILER_FLAG_IF_SUPPORTED("-Wduplicated-branches")
	ADD_COMPILER_FLAG_IF_SUPPORTED("-Wlogical-op")
	ADD_COMPILER_FLAG_IF_SUPPORTED("-Wnon-virtual-dtor")
	ADD_COMPILER_FLAG_IF_SUPPORTED("-Wrestrict")
	ADD_COMPILER_FLAG_IF_SUPPORTED("-Wdouble-promotion")
endif()

# A function that sets sanitizer options on a target.
function(WAVM_SET_TARGET_SANITIZER_OPTIONS TARGET_NAME)
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
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
	endif()
endfunction()

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
	
	# Set sanitizer options.
	WAVM_SET_TARGET_SANITIZER_OPTIONS(${TARGET_NAME})

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
		target_compile_options(${TARGET_NAME} PRIVATE "/wd4204") # nonstandard extension used : non-constant aggregate initializer

		target_link_libraries(${TARGET_NAME} PRIVATE "-ignore:4199") # /DELAYLOAD:... ignored; no imports found from ...

		# error C2338:  You've instantiated std::aligned_storage<Len, Align> with an extended alignment
		# (in other words, Align > alignof(max_align_t)). Before VS 2017 15.8, the member "type" would
		# non-conformingly have an alignment of only alignof(max_align_t). VS 2017 15.8 was fixed to
		# handle this correctly, but the fix inherently changes layout and breaks binary compatibility
		# (*only* for uses of aligned_storage with extended alignments). Please define either
		# (1) _ENABLE_EXTENDED_ALIGNED_STORAGE to acknowledge that you understand this message and
		#     that you actually want a type with an extended alignment, or
		# (2) _DISABLE_EXTENDED_ALIGNED_STORAGE to silence this message and get the old non-conformant
		#     behavior.
		target_compile_definitions(${TARGET_NAME} PRIVATE "_ENABLE_EXTENDED_ALIGNED_STORAGE")

	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	
		if(WAVM_ENABLE_LTO)
			target_compile_options(${TARGET_NAME} PRIVATE "-flto")
			target_link_libraries(${TARGET_NAME} PRIVATE "-flto")
		endif()

		# Compile with all+extra warnings and fatal warnings
		target_compile_options(${TARGET_NAME} PRIVATE "-Wall")
		target_compile_options(${TARGET_NAME} PRIVATE "-Wextra")
		target_compile_options(${TARGET_NAME} PRIVATE "-Werror")
		
		# Exclude some warnings included in Wextra that we don't care about.
		target_compile_options(${TARGET_NAME} PRIVATE "-Wno-unused-parameter")
		
		# Add the optional warnings that the compiler was detected to support.
		target_compile_options(${TARGET_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:C>:${OPTIONAL_C_FLAGS}>")
		target_compile_options(${TARGET_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${OPTIONAL_CXX_FLAGS}>")
		
		# Warn if a switch doesn't handle an enum case even if it has a default label.
		target_compile_options(${TARGET_NAME} PRIVATE "-Wswitch-enum")
		target_compile_options(${TARGET_NAME} PRIVATE "-Wswitch-default")
	
		# Disable RTTI to allow linking against a build of LLVM that was compiled without it.
		target_compile_options(${TARGET_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

		get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
		if(TARGET_TYPE STREQUAL "STATIC_LIBRARY" AND WAVM_ENABLE_STATIC_LIBRARY_PIC)
			# Ensure that even static libraries are relocatable so they can be linked into a .so
			target_compile_options(${TARGET_NAME} PRIVATE "-fPIC")
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

	# Install PDB files to the same directory as the binaries.
	# Exclude static libraries as TARGET_PDB_FILE only works for linked targets.
	get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
	if(MSVC AND NOT ${TARGET_TYPE} STREQUAL "STATIC_LIBRARY")
		install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}>
				DESTINATION $<IF:$<OR:$<CONFIG:RelWithDebInfo>,$<BOOL:${WAVM_DISABLE_PER_CONFIG_RPATH}>>,bin,bin/WAVM/$<CONFIG>>
				OPTIONAL)
	endif()
endfunction()

# Older versions of CMake can't handle target_link_libraries on the monolithic WAVM library
# when invoked from a source directory other than the root directory where the WAVM target is
# defined. To get around this, accumulate the libraries this component needs to link with in an
# internal config variable, which seems to be the closest thing CMake has to a global variable.
# After processing all library components, this root directory CMakeLists.txt invokes
# target_link_libraries with the accumulated libraries.
set(WAVM_MONOLIB_PRIVATE_LIBS "" CACHE INTERNAL "" FORCE)
set(WAVM_MONOLIB_PUBLIC_LIBS "" CACHE INTERNAL "" FORCE)

# CMake also scopes the effect of set_source_files_properties to targets created by the same list
# file, so to set the header-only flag on source files in the WAVM monolib, it's necessary to
# accumulate the set of header-only files in a global variable while processing subdirectory list
# files, and then set the source file properties at the end of this list file.
set(WAVM_MONOLIB_NONCOMPILED_SOURCE_FILES "" CACHE INTERNAL "" FORCE)

function(WAVM_ADD_LIB_COMPONENT COMPONENT_NAME)
	cmake_parse_arguments(COMPONENT
		""
		""
		"SOURCES;NONCOMPILED_SOURCES;PRIVATE_LIBS;PUBLIC_LIBS;PRIVATE_LIB_COMPONENTS;PUBLIC_LIB_COMPONENTS;PRIVATE_INCLUDE_DIRECTORIES;PRIVATE_SYSTEM_INCLUDE_DIRECTORIES;PRIVATE_DEFINITIONS;PUBLIC_DEFINITIONS"
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
	
	# Directly add the component's source files to the monolithic WAVM library.
	target_sources(libWAVM PRIVATE ${COMPONENT_SOURCES_ABSOLUTE} ${CMAKE_CURRENT_LIST_FILE})

	# Add the non-compiled source files to a global list that will be flagged as "header-only".
	list(APPEND WAVM_MONOLIB_NONCOMPILED_SOURCE_FILES ${COMPONENT_NONCOMPILED_SOURCES_ABSOLUTE})
	set(WAVM_MONOLIB_NONCOMPILED_SOURCE_FILES ${WAVM_MONOLIB_NONCOMPILED_SOURCE_FILES} CACHE INTERNAL "" FORCE)

	# Add the libraries this component depends on to the global list of libraries to link the
	# monolithic WAVM library with.
	list(APPEND WAVM_MONOLIB_PRIVATE_LIBS ${COMPONENT_PRIVATE_LIBS})
	list(APPEND WAVM_MONOLIB_PUBLIC_LIBS ${COMPONENT_PUBLIC_LIBS})
	set(WAVM_MONOLIB_PRIVATE_LIBS ${WAVM_MONOLIB_PRIVATE_LIBS} CACHE INTERNAL "" FORCE)
	set(WAVM_MONOLIB_PUBLIC_LIBS ${WAVM_MONOLIB_PUBLIC_LIBS} CACHE INTERNAL "" FORCE)
	
	# Add the component's include directories and definitions.
	target_include_directories(libWAVM PRIVATE ${COMPONENT_PRIVATE_INCLUDE_DIRECTORIES})
	target_include_directories(libWAVM SYSTEM PRIVATE ${COMPONENT_PRIVATE_SYSTEM_INCLUDE_DIRECTORIES})
	target_compile_definitions(libWAVM PRIVATE ${COMPONENT_PRIVATE_DEFINITIONS})
	target_compile_definitions(libWAVM PUBLIC ${COMPONENT_PUBLIC_DEFINITIONS})

	# Set up the component's API definitions.
	string(TOUPPER ${COMPONENT_NAME} COMPONENT_C_NAME)
	string(REPLACE "-" "_" COMPONENT_C_NAME ${COMPONENT_C_NAME})
	if(NOT WAVM_ENABLE_STATIC_LINKING AND MSVC)
		target_compile_definitions(libWAVM PRIVATE "\"${COMPONENT_C_NAME}_API=__declspec(dllexport)\"")
		target_compile_definitions(libWAVM INTERFACE "\"${COMPONENT_C_NAME}_API=__declspec(dllimport)\"")
	else()
		target_compile_definitions(libWAVM PUBLIC "${COMPONENT_C_NAME}_API=")
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
	target_link_libraries(${EXE_NAME} PRIVATE libWAVM)

	if(EXE_FOLDER)
		set_target_properties(${EXE_NAME} PROPERTIES FOLDER ${EXE_FOLDER})
	endif()
endfunction()

# Create a WAVM library that will include all WAVM library components.
if(WAVM_ENABLE_STATIC_LINKING)
	add_library(libWAVM STATIC)
else()
	add_library(libWAVM SHARED)
endif()
WAVM_SET_TARGET_COMPILE_OPTIONS(libWAVM)
WAVM_INSTALL_TARGET(libWAVM)
set_target_properties(libWAVM PROPERTIES FOLDER Libraries)
set_target_properties(libWAVM PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
set_target_properties(libWAVM PROPERTIES SOVERSION ${WAVM_VERSION_MAJOR})
set_target_properties(libWAVM PROPERTIES OUTPUT_NAME "WAVM")

# Process the CMake scripts in subdirectories.
add_subdirectory(Examples)
add_subdirectory(Include/WAVM/Inline)
add_subdirectory(Lib/IR)
add_subdirectory(Lib/Logging)
add_subdirectory(Lib/NFA)
add_subdirectory(Lib/Platform)
add_subdirectory(Lib/RegExp)
add_subdirectory(Lib/VFS)
add_subdirectory(Lib/WASM)
add_subdirectory(Lib/WASTParse)
add_subdirectory(Lib/WASTPrint)
add_subdirectory(Programs/wavm)
add_subdirectory(Test)
add_subdirectory(ThirdParty/gdtoa)
add_subdirectory(ThirdParty/liblmdb)

if(WAVM_ENABLE_RUNTIME)
	add_subdirectory(Include/WAVM/RuntimeABI)
	add_subdirectory(Lib/Emscripten)
	add_subdirectory(Lib/ObjectCache)
	add_subdirectory(Lib/LLVMJIT)
	add_subdirectory(Lib/Runtime)
	add_subdirectory(Lib/ThreadTest)
	add_subdirectory(Lib/WASI)
	add_subdirectory(Lib/wavm-c)
endif()

if(WAVM_ENABLE_UNWIND)
	add_subdirectory(ThirdParty/libunwind)
endif()

# Add the library dependencies accumulated from the various library components as link dependencies
# of the monolithic WAVM library.
target_link_libraries(libWAVM PRIVATE ${WAVM_MONOLIB_PRIVATE_LIBS})
target_link_libraries(libWAVM PUBLIC ${WAVM_MONOLIB_PUBLIC_LIBS})

# Set the non-compiled source files accumulated from the various library components as header-only,
# which includes the files in IDE projects, but doesn't compile them.
target_sources(libWAVM PRIVATE ${WAVM_MONOLIB_NONCOMPILED_SOURCE_FILES})
set_source_files_properties(${WAVM_MONOLIB_NONCOMPILED_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

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
	Build/azure-build-job-template.yml
	Build/notify-discord.sh
	Build/travis-build.sh
	LICENSE
	README.md
	run-clang-format.sh
	WebAssembly.tmLanguage)
