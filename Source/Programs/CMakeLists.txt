add_executable(Assemble Assemble.cpp CLI.h)
target_link_libraries(Assemble Core WAST WebAssembly)
set_target_properties(Assemble PROPERTIES FOLDER Programs)

add_executable(Disassemble Disassemble.cpp CLI.h)
target_link_libraries(Disassemble Core WAST WebAssembly)
set_target_properties(Disassemble PROPERTIES FOLDER Programs)

add_executable(Test Test.cpp CLI.h)
target_link_libraries(Test Core WAST WebAssembly Runtime)
set_target_properties(Test PROPERTIES FOLDER Programs)

add_executable(wavm wavm.cpp CLI.h)
target_link_libraries(wavm Core WAST WebAssembly Runtime Emscripten)
set_target_properties(wavm PROPERTIES FOLDER Programs)
