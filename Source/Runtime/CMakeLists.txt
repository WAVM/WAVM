file(GLOB Sources "*.cpp")
file(GLOB Headers "*.h")
add_library(Runtime STATIC ${Sources} ${Headers})

# Find an installed build of LLVM
find_package(LLVM 3.7 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM_INCLUDE_DIRS: ${LLVM_INCLUDE_DIRS}")

# Include the LLVM headers
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Link against the LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS support core passes mcjit native)
target_link_libraries(Runtime Core AST ${LLVM_LIBS})
