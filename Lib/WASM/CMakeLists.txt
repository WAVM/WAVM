set(Sources WASMSerialization.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/WASM/WASM.h)
include_directories(${WAVM_INCLUDE_DIR}/WASM)

WAVM_ADD_LIBRARY(WASM ${Sources} ${PublicHeaders})
set_target_properties(WASM PROPERTIES FOLDER Libraries)

add_definitions(-DWASM_API=DLL_EXPORT)
target_link_libraries(WASM Logging IR)
