set(Sources WASMSerialization.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/WASM/WASM.h)
include_directories(${WAVM_INCLUDE_DIR}/WASM)

add_library(WASM SHARED ${Sources} ${PublicHeaders})
add_definitions(-DWASM_API=DLL_EXPORT)
target_link_libraries(WASM Logging IR)
