file(GLOB Sources "*.cpp")
file(GLOB Headers "*.h")
add_library(Runtime SHARED ${Sources} ${Headers})

# Find an installed build of LLVM
find_package(LLVM 3.7 REQUIRED CONFIG)
find_package(ZLIB REQUIRED)

# Include the LLVM headers
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_definitions(-DRUNTIME_API=DLL_EXPORT)

# Link against the LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS support core passes mcjit native)
target_link_libraries(Runtime Core AST ${LLVM_LIBS})
