WAVM_ADD_INSTALLED_EXECUTABLE(wavm-disas Programs wavm-disas.cpp)
target_link_libraries(wavm-disas PRIVATE Logging IR WASTPrint WASM Platform)