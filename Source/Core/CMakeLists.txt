file(GLOB Sources "*.cpp")
file(GLOB PrivateHeaders "*.h")

file(GLOB Headers "${WAVM_INCLUDE_DIR}/Core/*.h")
include_directories(${WAVM_INCLUDE_DIR}/Core)

add_definitions(-DCORE_API=DLL_EXPORT)

add_library(Core SHARED ${Sources} ${Headers} ${PrivateHeaders})
