set(Sources Emscripten.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/Emscripten/Emscripten.h)

WAVM_ADD_LIBRARY(Emscripten ${Sources} ${PublicHeaders})
target_link_libraries(Emscripten PUBLIC Runtime PRIVATE Logging Platform)
