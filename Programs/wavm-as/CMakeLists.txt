WAVM_ADD_INSTALLED_EXECUTABLE(wavm-as Programs wavm-as.cpp)
target_link_libraries(wavm-as Logging IR WASTParse WASM)
