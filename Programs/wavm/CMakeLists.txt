WAVM_ADD_EXECUTABLE(wavm
	FOLDER Programs
	SOURCES FeatureSpec.cpp
			wavm.cpp
			wavm.h
			wavm-assemble.cpp
			wavm-compile.cpp
			wavm-disassemble.cpp
			wavm-run.cpp
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse WASM Runtime Emscripten)
WAVM_INSTALL_TARGET(wavm)
