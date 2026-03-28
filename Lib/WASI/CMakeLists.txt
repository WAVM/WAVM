set(Sources
	WASI.cpp
	WASIArgsEnvs.cpp
	WASIClocks.cpp
	WASIDiagnostics.cpp
	WASIFile.cpp)
set(PrivateHeaders
	WASIPrivate.h)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/WASI/WASI.h
	${WAVM_INCLUDE_DIR}/WASI/WASIABI.h)

WAVM_ADD_LIB_COMPONENT(WASI
	SOURCES ${Sources}
	HEADERS ${PublicHeaders} ${PrivateHeaders}
	NONCOMPILED_SOURCES ${WAVM_INCLUDE_DIR}/WASI/WASIABI.LICENSE)
