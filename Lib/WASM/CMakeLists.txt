set(Sources WASMSerialization.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/WASM/WASM.h)

WAVM_ADD_LIBRARY(WASM ${Sources} ${PublicHeaders})
target_link_libraries(WASM PUBLIC Logging PRIVATE Platform IR)
