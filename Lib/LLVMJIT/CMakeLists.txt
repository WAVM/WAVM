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

# Find the LLVM libraries to link with.
llvm_map_components_to_libnames(LLVM_LIBS
	support
	core
	passes
	orcjit
	RuntimeDyld
	DebugInfoDWARF
	AllTargetsAsmParsers
	${LLVM_TARGETS_TO_BUILD})

# Create the LLVMJIT target.
WAVM_ADD_LIB_COMPONENT(LLVMJIT
	SOURCES ${Sources} ${PublicHeaders}
	PUBLIC_LIB_COMPONENTS IR RuntimeABI
	PRIVATE_LIB_COMPONENTS Logging Platform
	PRIVATE_LIBS ${LLVM_LIBS}
	PRIVATE_SYSTEM_INCLUDE_DIRECTORIES ${LLVM_INCLUDE_DIRS}
	PRIVATE_DEFINITIONS ${LLVM_DEFINITIONS})