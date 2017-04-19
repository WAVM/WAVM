add_executable(Assemble Assemble.cpp CLI.h)
target_link_libraries(Assemble Core IR WAST WASM)
set_target_properties(Assemble PROPERTIES FOLDER Programs)

add_executable(Disassemble Disassemble.cpp CLI.h)
target_link_libraries(Disassemble Core IR WAST WASM)
set_target_properties(Disassemble PROPERTIES FOLDER Programs)

add_executable(Test Test.cpp CLI.h)
target_link_libraries(Test Core IR WAST Runtime)
set_target_properties(Test PROPERTIES FOLDER Programs)

add_executable(wavm wavm.cpp CLI.h)
target_link_libraries(wavm Core IR WAST WASM Runtime Emscripten)
set_target_properties(wavm PROPERTIES FOLDER Programs)
