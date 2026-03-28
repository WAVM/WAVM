# Convert LLVM_DEFINITIONS from a string of space-separated elements to a CMake list.
# (find_package(LLVM) is called in the root CMakeLists.txt so its targets are globally visible)
separate_arguments(LLVM_DEFINITIONS)

# LLVM on Windows includes definitions to disable secure CRT warnings. Remove them since
# they're not necessary just to include LLVM and suppress useful warnings in WAVM code.
list(REMOVE_ITEM LLVM_DEFINITIONS "-D_CRT_SECURE_NO_WARNINGS")
list(REMOVE_ITEM LLVM_DEFINITIONS "-D_CRT_SECURE_NO_DEPRECATE")
list(REMOVE_ITEM LLVM_DEFINITIONS "-D_CRT_NONSTDC_NO_DEPRECATE")
list(REMOVE_ITEM LLVM_DEFINITIONS "-D_CRT_NONSTDC_NO_WARNINGS")
list(REMOVE_ITEM LLVM_DEFINITIONS "-D_SCL_SECURE_NO_DEPRECATE")
list(REMOVE_ITEM LLVM_DEFINITIONS "-D_SCL_SECURE_NO_WARNINGS")

# Find the LLVM libraries to link with.
llvm_map_components_to_libnames(LLVM_LIBS
	support
	core
	passes
	orcjit
	DebugInfoDWARF
	AllTargetsAsmParsers
	${LLVM_TARGETS_TO_BUILD})

set(Sources
	Disassembler.cpp
	EmitConvert.cpp
	EmitCore.cpp
	EmitExceptions.cpp
	EmitFunction.cpp
	EmitMem.cpp
	EmitModule.cpp
	EmitNumeric.cpp
	EmitTable.cpp
	EmitVar.cpp
	GDBRegistration.cpp
	LLVMCompile.cpp
	LLVMJIT.cpp
	LLVMModule.cpp
	Thunk.cpp)
set(PrivateHeaders
	EmitContext.h
	EmitFunctionContext.h
	EmitModuleContext.h
	LLVMJITPrivate.h)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/LLVMJIT/LLVMJIT.h)

# Create the LLVMJIT component.
WAVM_ADD_LIB_COMPONENT(LLVMJIT
	SOURCES ${Sources}
	HEADERS ${PublicHeaders} ${PrivateHeaders}
	PRIVATE_LIBS ${LLVM_LIBS}
	PRIVATE_SYSTEM_INCLUDE_DIRECTORIES ${LLVM_INCLUDE_DIRS}
	PRIVATE_DEFINITIONS ${LLVM_DEFINITIONS})
