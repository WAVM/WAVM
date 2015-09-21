file(GLOB Sources "*.cpp")
file(GLOB Headers "*.h")

add_definitions(-DCORE_API=DLL_EXPORT)

add_library(Core SHARED ${Sources} ${Headers})