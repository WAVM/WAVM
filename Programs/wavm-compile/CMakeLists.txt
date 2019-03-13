WAVM_ADD_EXECUTABLE(wavm-compile
	FOLDER Programs
	SOURCES wavm-compile.cpp
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse WASM Runtime)
WAVM_INSTALL_TARGET(wavm-compile)
