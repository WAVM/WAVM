set(Sources
	EmitContext.h
	EmitConvert.cpp
	EmitCore.cpp
	EmitExceptions.cpp
	EmitFunction.cpp
	EmitFunctionContext.h
	EmitMem.cpp
	EmitModule.cpp
	EmitModuleContext.h
	EmitNumeric.cpp
	EmitTable.cpp
	EmitVar.cpp
	EmitWorkarounds.h
	LLVMCompile.cpp
	LLVMJIT.cpp
	LLVMJITPrivate.h
	LLVMModule.cpp
	Thunk.cpp
	Win64EH.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/LLVMJIT/LLVMJIT.h)

# Find an installed build of LLVM
find_package(LLVM REQUIRED CONFIG)

if(LLVM_VERSION_MAJOR LESS 6)
	message(FATAL_ERROR "WAVM requires LLVM version 6.0 or newer")
endif()

# Include the LLVM headers
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(LLVM_TARGET_ATTRIBUTES "" CACHE STRING "allows overriding LLVM target attributes (e.g. \"-avx512f\")")
string(REGEX REPLACE "\"" "\\\\\"" LLVM_TARGET_ATTRIBUTES_ESCAPED ${LLVM_TARGET_ATTRIBUTES} " ")
add_definitions("\"-DLLVM_TARGET_ATTRIBUTES=${LLVM_TARGET_ATTRIBUTES_ESCAPED}\"")

# Find the LLVM libraries to link with.
llvm_map_components_to_libnames(LLVM_LIBS support core passes orcjit RuntimeDyld native DebugInfoDWARF)

# Create the LLVMJIT target.
WAVM_ADD_LIB_COMPONENT(LLVMJIT
	SOURCES ${Sources} ${PublicHeaders}
	PUBLIC_LIB_COMPONENTS IR
	PRIVATE_LIB_COMPONENTS Logging Platform
	PRIVATE_LIBS ${LLVM_LIBS})

# This adds the LLVM RPATH to the installed libLLVMJIT.so
set_target_properties(LLVMJIT PROPERTIES INSTALL_RPATH_USE_LINK_PATH true)
