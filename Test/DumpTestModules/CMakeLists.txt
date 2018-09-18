WAVM_ADD_EXECUTABLE(DumpTestModules Testing DumpTestModules.cpp)
target_link_libraries(DumpTestModules PRIVATE Logging IR WASTParse WASTPrint Platform WASM)
