add_executable(PrintWAST PrintWAST.cpp CLI.h)
target_link_libraries(PrintWAST Core AST WebAssembly Runtime)
set_target_properties(PrintWAST PROPERTIES FOLDER Programs)

add_executable(Test Test.cpp CLI.h)
target_link_libraries(Test Core AST WebAssembly Runtime)
set_target_properties(Test PROPERTIES FOLDER Programs)

add_executable(wavm wavm.cpp CLI.h)
target_link_libraries(wavm Core AST WebAssembly Runtime)
set_target_properties(wavm PROPERTIES FOLDER Programs)
