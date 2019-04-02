WAVM_ADD_EXECUTABLE(wavm-run-wasi
	FOLDER Programs
	SOURCES wavm-run-wasi.cpp
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse WASM Runtime WASI)
WAVM_INSTALL_TARGET(wavm-run-wasi)
