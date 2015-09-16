file(GLOB Sources "*.cpp")
file(GLOB Headers "*.h")
add_library(Core STATIC ${Sources} ${Headers})