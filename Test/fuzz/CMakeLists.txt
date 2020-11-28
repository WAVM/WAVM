function(WAVM_ADD_FUZZER_EXECUTABLE TARGET_NAME)
	WAVM_ADD_EXECUTABLE(${TARGET_NAME}
		FOLDER Testing/Fuzzers
		${ARGN})
	if(WAVM_ENABLE_LIBFUZZER)
		target_link_libraries(${TARGET_NAME} PRIVATE "-fsanitize=fuzzer")
	endif()
endfunction()

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-disassemble
	SOURCES fuzz-disassemble.cpp ModuleMatcher.h FuzzTargetCommonMain.h
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse WASTPrint WASM Platform)

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-assemble
	SOURCES fuzz-assemble.cpp ModuleMatcher.h FuzzTargetCommonMain.h
	PRIVATE_LIB_COMPONENTS Logging IR WASTParse WASTPrint WASM Platform)
	
WAVM_ADD_EXECUTABLE(translate-compile-model-corpus
	SOURCES translate-compile-model-corpus.cpp
	PRIVATE_LIB_COMPONENTS Logging IR WASM
	FOLDER Testing/Fuzzers)

if(WAVM_ENABLE_RUNTIME)
	WAVM_ADD_FUZZER_EXECUTABLE(fuzz-instantiate
		SOURCES fuzz-instantiate.cpp FuzzTargetCommonMain.h
		PRIVATE_LIB_COMPONENTS Logging IR WASM Runtime)

	WAVM_ADD_FUZZER_EXECUTABLE(fuzz-compile-model
		SOURCES fuzz-compile-model.cpp FuzzTargetCommonMain.h
		PRIVATE_LIB_COMPONENTS Logging IR WASTPrint LLVMJIT Platform)
endif()
