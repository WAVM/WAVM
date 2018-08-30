set(Sources
	Atomics.cpp
	Exception.cpp
	Intrinsics.cpp
	Linker.cpp
	LLVMEmitContext.h
	LLVMEmitConvert.cpp
	LLVMEmitCore.cpp
	LLVMEmitExceptions.cpp
	LLVMEmitFunction.cpp
	LLVMEmitFunctionContext.h
	LLVMEmitMem.cpp
	LLVMEmitModule.cpp
	LLVMEmitModuleContext.h
	LLVMEmitNumeric.cpp
	LLVMEmitVar.cpp
	LLVMEmitWorkarounds.h
	LLVMJIT.cpp
	LLVMJIT.h
	LLVMJITThunk.cpp
	LLVMJITUnit.cpp
	LLVMPreInclude.h
	LLVMPostInclude.h
	LLVMWin64EH.cpp
	Memory.cpp
	Module.cpp
	ObjectGC.cpp
	Runtime.cpp
	RuntimePrivate.h
	Table.cpp
	WAVMIntrinsics.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/Runtime/Intrinsics.h
	${WAVM_INCLUDE_DIR}/Runtime/Linker.h
	${WAVM_INCLUDE_DIR}/Runtime/Runtime.h)
include_directories(${WAVM_INCLUDE_DIR}/Runtime)

WAVM_ADD_LIBRARY(Runtime ${Sources} ${PublicHeaders})
set_target_properties(Runtime PROPERTIES FOLDER Libraries)

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
llvm_map_components_to_libnames(LLVM_LIBS support core passes orcjit native DebugInfoDWARF)
target_link_libraries(Runtime Platform Logging IR ${LLVM_LIBS})