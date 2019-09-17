set(LMDB_COMPILE_OPTIONS)
set(LMDB_PRIVATE_DEFINITIONS)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	list(APPEND LMDB_COMPILE_OPTIONS "/wd4146") # unary minus operator applied to unsigned type, result still unsigned
	list(APPEND LMDB_COMPILE_OPTIONS "/wd4267") # conversion from 'x' to 'y', possible loss of data
	list(APPEND LMDB_COMPILE_OPTIONS "/wd4244") # conversion from 'x' to 'y', possible loss of data
	list(APPEND LMDB_COMPILE_OPTIONS "/wd4996") # 'x': was declared deprecated
	list(APPEND LMDB_COMPILE_OPTIONS "/wd4172") # returning address of local variable or temporary: 'x'

	list(APPEND LMDB_PRIVATE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS")
endif()

WAVM_ADD_THIRD_PARTY_LIBRARY(WAVMlmdb
	SOURCES lmdb.h
			mdb.c
			midl.c
			midl.h
	PUBLIC_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	PRIVATE_DEFINITIONS ${LMDB_PRIVATE_DEFINITIONS}
	PRIVATE_COMPILE_OPTIONS ${LMDB_COMPILE_OPTIONS}
)
