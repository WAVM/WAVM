set(Sources
	wavm-c.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/wavm-c/wavm-c.h)

WAVM_ADD_LIB_COMPONENT(wavm-c
	SOURCES ${Sources}
	HEADERS ${PublicHeaders}
	NONCOMPILED_SOURCES ${WAVM_INCLUDE_DIR}/wavm-c/wasm-c-api.LICENSE
	PUBLIC_DEFINITIONS "WASM_C_API=WAVM_API")
