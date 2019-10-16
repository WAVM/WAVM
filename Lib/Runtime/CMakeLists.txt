set(Sources
	Atomics.cpp
	Compartment.cpp
	Context.cpp
	Exception.cpp
	Global.cpp
	Instance.cpp
	Intrinsics.cpp
	Invoke.cpp
	Linker.cpp
	Memory.cpp
	Module.cpp
	ObjectGC.cpp
	ResourceQuota.cpp
	Runtime.cpp
	RuntimePrivate.h
	Table.cpp
	WAVMIntrinsics.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/Runtime/Intrinsics.h
	${WAVM_INCLUDE_DIR}/Runtime/Linker.h
	${WAVM_INCLUDE_DIR}/Runtime/Runtime.h)

WAVM_ADD_LIB_COMPONENT(Runtime
	SOURCES ${Sources} ${PublicHeaders}
	PUBLIC_LIB_COMPONENTS IR Platform
	PRIVATE_LIB_COMPONENTS Logging LLVMJIT RuntimeABI WASM)