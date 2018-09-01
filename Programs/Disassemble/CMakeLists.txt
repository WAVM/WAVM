add_executable(Disassemble Disassemble.cpp)
target_link_libraries(Disassemble Logging IR WASTPrint WASM)
set_target_properties(Disassemble PROPERTIES FOLDER Programs)
