add_executable(wavm-disas wavm-disas.cpp)
target_link_libraries(wavm-disas Logging IR WASTPrint WASM)
set_target_properties(wavm-disas PROPERTIES FOLDER Programs)
