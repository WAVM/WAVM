file(GLOB Sources "*.cpp")
file(GLOB Headers "*.h")
add_library(AST STATIC ${Sources} ${Headers})
