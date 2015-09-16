file(GLOB Sources "*.cpp")
file(GLOB Headers "*.h")
add_library(WebAssembly STATIC ${Sources} ${Headers})
