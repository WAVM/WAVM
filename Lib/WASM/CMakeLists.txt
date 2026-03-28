set(Sources WASMSerialization.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/WASM/WASM.h)

WAVM_ADD_LIB_COMPONENT(WASM
	SOURCES ${Sources}
	HEADERS ${PublicHeaders})
