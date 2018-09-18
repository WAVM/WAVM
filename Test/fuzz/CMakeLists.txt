function(WAVM_ADD_FUZZER_EXECUTABLE TARGET_NAME)
	WAVM_ADD_EXECUTABLE(${TARGET_NAME} Testing/Fuzzers ${ARGN})
	if(WAVM_ENABLE_LIBFUZZER)
		target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=fuzzer")
	endif()
endfunction()

WAVM_ADD_FUZZER_EXECUTABLE(FuzzWAST FuzzWAST.cpp wastFuzzDictionary.txt)
target_link_libraries(FuzzWAST Logging IR WASTParse)

WAVM_ADD_FUZZER_EXECUTABLE(FuzzWASM FuzzWASM.cpp)
target_link_libraries(FuzzWASM Logging IR WASM)

WAVM_ADD_FUZZER_EXECUTABLE(FuzzDisassemble FuzzDisassemble.cpp ModuleMatcher.h)
target_link_libraries(FuzzDisassemble Logging IR WASTParse WASTPrint WASM)

WAVM_ADD_FUZZER_EXECUTABLE(FuzzAssemble FuzzAssemble.cpp ModuleMatcher.h)
target_link_libraries(FuzzAssemble Logging IR WASTParse WASTPrint WASM)

if(WAVM_ENABLE_RUNTIME)
	WAVM_ADD_FUZZER_EXECUTABLE(FuzzCompile FuzzCompile.cpp)
	target_link_libraries(FuzzCompile Logging IR WASM Runtime)

	WAVM_ADD_FUZZER_EXECUTABLE(FuzzInstantiate FuzzInstantiate.cpp)
	target_link_libraries(FuzzInstantiate Logging IR WASM Runtime)

	WAVM_ADD_FUZZER_EXECUTABLE(FuzzCompileModel FuzzCompileModel.cpp)
	target_link_libraries(FuzzCompileModel Logging IR WASTPrint Runtime)
endif()
