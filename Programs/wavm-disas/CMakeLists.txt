WAVM_ADD_INSTALLED_EXECUTABLE(wavm-disas Programs wavm-disas.cpp)
target_link_libraries(wavm-disas Logging IR WASTPrint WASM)