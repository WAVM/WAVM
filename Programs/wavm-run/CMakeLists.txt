WAVM_ADD_EXECUTABLE(wavm-run
	FOLDER Programs
	SOURCES wavm-run.cpp
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse WASM Runtime Emscripten)
WAVM_INSTALL_TARGET(wavm-run)
