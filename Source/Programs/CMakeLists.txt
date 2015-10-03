add_executable(PrintWAST PrintWAST.cpp CLI.h)
target_link_libraries(PrintWAST Core AST WebAssembly)
set_target_properties(PrintWAST PROPERTIES FOLDER Programs)

add_executable(PrintASMJS PrintASMJS.cpp CLI.h)
target_link_libraries(PrintASMJS Core AST WebAssembly ASMJS)
set_target_properties(PrintASMJS PROPERTIES FOLDER Programs)

add_executable(Run Run.cpp CLI.h)
target_link_libraries(Run Core AST WebAssembly Runtime)
set_target_properties(Run PROPERTIES FOLDER Programs)

add_executable(Test Test.cpp CLI.h)
target_link_libraries(Test Core AST WebAssembly Runtime)
set_target_properties(Test PROPERTIES FOLDER Programs)
