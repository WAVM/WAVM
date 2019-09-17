set(Sources Emscripten.cpp
			EmscriptenABI.h
			EmscriptenPrivate.h
			EmscriptenThreads.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/Emscripten/Emscripten.h)

WAVM_ADD_LIB_COMPONENT(Emscripten
	SOURCES ${Sources} ${PublicHeaders}
	PUBLIC_LIB_COMPONENTS Runtime
	PRIVATE_LIB_COMPONENTS Logging Platform)