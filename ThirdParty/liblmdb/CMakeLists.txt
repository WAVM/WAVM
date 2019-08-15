add_library(lmdb STATIC
	lmdb.h
	mdb.c
	midl.c
	midl.h
	)
	
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(lmdb PRIVATE "/wd4146") # unary minus operator applied to unsigned type, result still unsigned
	target_compile_options(lmdb PRIVATE "/wd4267") # conversion from 'x' to 'y', possible loss of data
	target_compile_options(lmdb PRIVATE "/wd4244") # conversion from 'x' to 'y', possible loss of data
	target_compile_options(lmdb PRIVATE "/wd4996") # 'x': was declared deprecated
	target_compile_options(lmdb PRIVATE "/wd4172") # returning address of local variable or temporary: 'x'

	target_compile_definitions(lmdb PRIVATE "_CRT_SECURE_NO_WARNINGS")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# Compile as a static library, but use -fPIC to make sure it can be linked into a shared object.
	target_compile_options(lmdb PRIVATE "-fPIC")
endif()

target_include_directories(lmdb PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# When using static linking for the WAVM libraries, the lmdb library needs to be
# installed. Otherwise, it will just be linked into the WAVM .so/.dylib files.
if(WAVM_ENABLE_STATIC_LINKING)
	install(TARGETS lmdb DESTINATION lib/WAVM EXPORT WAVMInstallTargets)
endif()

WAVM_SET_TARGET_SANITIZER_OPTIONS(lmdb)

set_target_properties(lmdb PROPERTIES FOLDER "Third party")
