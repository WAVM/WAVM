set(Sources
	WASI.cpp
	WASIArgsEnvs.cpp
	WASIClocks.cpp
	WASIDiagnostics.cpp
	WASIFile.cpp
	WASIPrivate.h)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/WASI/WASI.h
	${WAVM_INCLUDE_DIR}/WASI/WASIABI.h
	${WAVM_INCLUDE_DIR}/WASI/WASIABI.LICENSE)

WAVM_ADD_LIB_COMPONENT(WASI
	SOURCES ${Sources} ${PublicHeaders}
	PUBLIC_LIB_COMPONENTS Runtime
	PRIVATE_LIB_COMPONENTS Logging Platform)