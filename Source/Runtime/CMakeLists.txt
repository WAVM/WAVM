file(GLOB Sources "*.cpp")
file(GLOB PrivateHeaders "*.h")

file(GLOB Headers "${WAVM_INCLUDE_DIR}/Runtime/*.h")
include_directories(${WAVM_INCLUDE_DIR}/Runtime)

add_library(Runtime SHARED ${Sources} ${Headers} ${PrivateHeaders})

# Find an installed build of LLVM
find_package(LLVM 3.8 REQUIRED CONFIG)

# Workaround an apparent problem in the Homebrew LLVM 3.8 bottle.
string(REGEX REPLACE "\@LLVM_DEFINITIONS\@" "-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS" LLVM_DEFINITIONS ${LLVM_DEFINITIONS})

# Include the LLVM headers
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_definitions(-DRUNTIME_API=DLL_EXPORT)

# Link against the LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS support core passes mcjit native)
target_link_libraries(Runtime Core WebAssembly ${LLVM_LIBS})
