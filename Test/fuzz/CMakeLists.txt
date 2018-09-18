function(WAVM_ADD_FUZZER_EXECUTABLE TARGET_NAME)
	WAVM_ADD_EXECUTABLE(${TARGET_NAME} Testing/Fuzzers ${ARGN})
	if(WAVM_ENABLE_LIBFUZZER)
		target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=fuzzer")
		target_link_libraries(${TARGET_NAME} PRIVATE "-fsanitize=fuzzer")
		target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize-coverage=trace-cmp,trace-div,trace-gep")
	endif()
endfunction()

WAVM_ADD_FUZZER_EXECUTABLE(FuzzWAST FuzzWAST.cpp wastFuzzDictionary.txt)
target_link_libraries(FuzzWAST PRIVATE Logging IR WASTParse Platform)

WAVM_ADD_FUZZER_EXECUTABLE(FuzzWASM FuzzWASM.cpp)
target_link_libraries(FuzzWASM PRIVATE Logging IR WASM Platform)

WAVM_ADD_FUZZER_EXECUTABLE(FuzzDisassemble FuzzDisassemble.cpp ModuleMatcher.h)
target_link_libraries(FuzzDisassemble PRIVATE Logging IR WASTParse WASTPrint WASM Platform)

WAVM_ADD_FUZZER_EXECUTABLE(FuzzAssemble FuzzAssemble.cpp ModuleMatcher.h)
target_link_libraries(FuzzAssemble PRIVATE Logging IR WASTParse WASTPrint WASM Platform)

if(WAVM_ENABLE_RUNTIME)
	WAVM_ADD_FUZZER_EXECUTABLE(FuzzCompile FuzzCompile.cpp)
	target_link_libraries(FuzzCompile PRIVATE Logging IR WASM Runtime)

	WAVM_ADD_FUZZER_EXECUTABLE(FuzzInstantiate FuzzInstantiate.cpp)
	target_link_libraries(FuzzInstantiate PRIVATE Logging IR WASM Runtime)

	WAVM_ADD_FUZZER_EXECUTABLE(FuzzCompileModel FuzzCompileModel.cpp)
	target_link_libraries(FuzzCompileModel PRIVATE Logging IR WASTPrint Runtime Platform)
endif()
