set(Sources
	Exception.cpp
	Intrinsics.cpp
	Linker.cpp
	LLVMEmitIR.cpp
	LLVMJIT.cpp
	LLVMJIT.h
	LLVMWin64EH.cpp
	Memory.cpp
	ModuleInstance.cpp
	ObjectGC.cpp
	Runtime.cpp
	RuntimePrivate.h
	Table.cpp
	Threads.cpp
	WAVMIntrinsics.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/Runtime/Intrinsics.h
	${WAVM_INCLUDE_DIR}/Runtime/Linker.h
	${WAVM_INCLUDE_DIR}/Runtime/Runtime.h)
include_directories(${WAVM_INCLUDE_DIR}/Runtime)

add_library(Runtime SHARED ${Sources} ${PublicHeaders})

# Find an installed build of LLVM
find_package(LLVM 6.0 REQUIRED CONFIG)

# Include the LLVM headers
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_definitions(-DRUNTIME_API=DLL_EXPORT)

set(LLVM_TARGET_ATTRIBUTES "" CACHE STRING "allows overriding LLVM target attributes (e.g. \"-avx512f\")")
string(REGEX REPLACE "\"" "\\\\\"" LLVM_TARGET_ATTRIBUTES_ESCAPED ${LLVM_TARGET_ATTRIBUTES} " ")
add_definitions("\"-DLLVM_TARGET_ATTRIBUTES=${LLVM_TARGET_ATTRIBUTES_ESCAPED}\"")

# Link against the LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS support core passes mcjit native DebugInfoDWARF)
target_link_libraries(Runtime Platform Logging IR ${LLVM_LIBS})
