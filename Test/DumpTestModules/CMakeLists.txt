WAVM_ADD_EXECUTABLE(DumpTestModules
	FOLDER Testing
	SOURCES DumpTestModules.cpp
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse WASTPrint Platform WASM)