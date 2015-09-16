file(GLOB Sources "*.cpp")
file(GLOB Headers "*.h")
add_library(WebAssembly STATIC ${Sources} ${Headers})
target_link_libraries(WebAssembly PUBLIC Core AST)