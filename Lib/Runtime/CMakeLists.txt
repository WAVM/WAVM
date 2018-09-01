set(Sources
	Atomics.cpp
	Compartment.cpp
	Context.cpp
	Exception.cpp
	Global.cpp
	Intrinsics.cpp
	Invoke.cpp
	Linker.cpp
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
	${WAVM_INCLUDE_DIR}/Runtime/Runtime.h
	${WAVM_INCLUDE_DIR}/Runtime/RuntimeData.h)

WAVM_ADD_LIBRARY(Runtime ${Sources} ${PublicHeaders})
set_target_properties(Runtime PROPERTIES FOLDER Libraries)

add_definitions(-DRUNTIME_API=DLL_EXPORT)

# Link against the LLVM libraries
target_link_libraries(Runtime Platform Logging IR LLVMJIT)