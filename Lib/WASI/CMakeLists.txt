set(Sources WASI.cpp WASIDefinitions.h WASIDefinitions.LICENSE)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/WASI/WASI.h)

WAVM_ADD_LIB_COMPONENT(WASI
	SOURCES ${Sources} ${PublicHeaders}
	PUBLIC_LIB_COMPONENTS Runtime
	PRIVATE_LIB_COMPONENTS Logging Platform)