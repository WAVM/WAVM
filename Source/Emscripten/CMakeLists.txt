file(GLOB Sources "*.cpp")
file(GLOB PrivateHeaders "*.h")

file(GLOB Headers "${WAVM_INCLUDE_DIR}/Emscripten/*.h")
include_directories(${WAVM_INCLUDE_DIR}/Emscripten)

add_library(Emscripten SHARED ${Sources} ${Headers} ${PrivateHeaders})

add_definitions(-DEMSCRIPTEN_API=DLL_EXPORT)

target_link_libraries(Emscripten Core WebAssembly Runtime)
