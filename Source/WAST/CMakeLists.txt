file(GLOB Sources "*.cpp")
file(GLOB Headers "*.h")

file(GLOB Headers "${WAVM_INCLUDE_DIR}/WAST/*.h")
include_directories(${WAVM_INCLUDE_DIR}/WAST)

add_library(WAST SHARED ${Sources} ${Headers})
add_definitions(-DWAST_API=DLL_EXPORT)
target_link_libraries(WAST Core WebAssembly)
