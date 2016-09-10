file(GLOB Sources "*.cpp")
file(GLOB PrivateHeaders "*.h")

file(GLOB Headers "${WAVM_INCLUDE_DIR}/WebAssembly/*.h")
include_directories(${WAVM_INCLUDE_DIR}/WebAssembly)

add_library(WebAssembly SHARED ${Sources} ${Headers} ${PrivateHeaders})
add_definitions(-DWEBASSEMBLY_API=DLL_EXPORT)
target_link_libraries(WebAssembly Core)
