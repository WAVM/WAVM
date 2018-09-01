set(Sources Emscripten.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/Emscripten/Emscripten.h)

WAVM_ADD_LIBRARY(Emscripten ${Sources} ${PublicHeaders})
set_target_properties(Emscripten PROPERTIES FOLDER Libraries)

add_definitions(-DEMSCRIPTEN_API=DLL_EXPORT)

target_link_libraries(Emscripten Logging Platform Runtime)
