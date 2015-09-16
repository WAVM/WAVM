add_executable(PrintWAST PrintWAST.cpp)
target_link_libraries(PrintWAST Core AST WebAssembly)

add_executable(PrintASMJS PrintASMJS.cpp)
target_link_libraries(PrintASMJS Core AST WebAssembly ASMJS)

add_executable(Run Run.cpp)
target_link_libraries(Run Core AST WebAssembly Runtime)

set_target_properties(PrintWAST PROPERTIES FOLDER Programs)
set_target_properties(PrintASMJS PROPERTIES FOLDER Programs)
set_target_properties(Run PROPERTIES FOLDER Programs)