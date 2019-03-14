function(WAVM_ADD_FUZZER_EXECUTABLE TARGET_NAME)
	WAVM_ADD_EXECUTABLE(${TARGET_NAME}
		FOLDER Testing/Fuzzers
		${ARGN})
	if(WAVM_ENABLE_LIBFUZZER)
		target_link_libraries(${TARGET_NAME} PRIVATE "-fsanitize=fuzzer")
	endif()
endfunction()

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-wast
	SOURCES fuzz-wast.cpp wastFuzzDictionary.txt
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse Platform)

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-wasm
	SOURCES fuzz-wasm.cpp
	PRIVATE_LIB_COMPONENTS Logging IR WASM Platform)

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-disassemble
	SOURCES fuzz-disassemble.cpp ModuleMatcher.h
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse WASTPrint WASM Platform)

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-assemble
	SOURCES fuzz-assemble.cpp ModuleMatcher.h
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse WASTPrint WASM Platform)

if(WAVM_ENABLE_RUNTIME)
	WAVM_ADD_FUZZER_EXECUTABLE(fuzz-compile
		SOURCES fuzz-compile.cpp
		PRIVATE_LIB_COMPONENTS Logging IR WASM Runtime)

	WAVM_ADD_FUZZER_EXECUTABLE(fuzz-instantiate
		SOURCES fuzz-instantiate.cpp
		PRIVATE_LIB_COMPONENTS Logging IR WASM Runtime)

	WAVM_ADD_FUZZER_EXECUTABLE(fuzz-compile-model
		SOURCES fuzz-compile-model.cpp
		PRIVATE_LIB_COMPONENTS Logging IR WASTPrint Runtime Platform)
endif()
