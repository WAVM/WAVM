file(GLOB Sources "*.cpp")
file(GLOB Headers "*.h")
add_library(ASMJS STATIC ${Sources} ${Headers})