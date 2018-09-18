WAVM_ADD_INSTALLED_EXECUTABLE(wavm-compile Programs wavm-compile.cpp)
target_link_libraries(wavm-compile PRIVATE Logging IR WASTParse WASM Runtime)
