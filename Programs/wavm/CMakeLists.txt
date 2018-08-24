add_executable(wavm wavm.cpp)
target_link_libraries(wavm Logging IR WAST WASM Runtime Emscripten ThreadTest)
set_target_properties(wavm PROPERTIES FOLDER Programs)
