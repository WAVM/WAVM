WAVM_ADD_INSTALLED_EXECUTABLE(wavm-run Programs wavm-run.cpp)
target_link_libraries(wavm-run PRIVATE Logging IR WASTParse WASM Runtime Emscripten ThreadTest)
