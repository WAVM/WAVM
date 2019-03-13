WAVM_ADD_EXECUTABLE(wavm-disas
	FOLDER Programs
	SOURCES wavm-disas.cpp
	PRIVATE_LIB_COMPONENTS Logging IR WASTPrint WASM Platform)
WAVM_INSTALL_TARGET(wavm-disas)