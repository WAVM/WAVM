function(WAVM_ADD_FUZZER_EXECUTABLE TARGET_NAME)
	WAVM_ADD_EXECUTABLE(${TARGET_NAME} Testing/Fuzzers ${ARGN})
	if(WAVM_ENABLE_LIBFUZZER)
		target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize=fuzzer")
		target_link_libraries(${TARGET_NAME} PRIVATE "-fsanitize=fuzzer")
		target_compile_options(${TARGET_NAME} PRIVATE "-fsanitize-coverage=trace-cmp,trace-div,trace-gep")
	endif()
endfunction()

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-wast fuzz-wast.cpp wastFuzzDictionary.txt)
target_link_libraries(fuzz-wast PRIVATE Logging IR WASTParse Platform)

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-wasm fuzz-wasm.cpp)
target_link_libraries(fuzz-wasm PRIVATE Logging IR WASM Platform)

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-disassemble fuzz-disassemble.cpp ModuleMatcher.h)
target_link_libraries(fuzz-disassemble PRIVATE Logging IR WASTParse WASTPrint WASM Platform)

WAVM_ADD_FUZZER_EXECUTABLE(fuzz-assemble fuzz-assemble.cpp ModuleMatcher.h)
target_link_libraries(fuzz-assemble PRIVATE Logging IR WASTParse WASTPrint WASM Platform)

if(WAVM_ENABLE_RUNTIME)
	WAVM_ADD_FUZZER_EXECUTABLE(fuzz-compile fuzz-compile.cpp)
	target_link_libraries(fuzz-compile PRIVATE Logging IR WASM Runtime)

	WAVM_ADD_FUZZER_EXECUTABLE(fuzz-instantiate fuzz-instantiate.cpp)
	target_link_libraries(fuzz-instantiate PRIVATE Logging IR WASM Runtime)

	WAVM_ADD_FUZZER_EXECUTABLE(fuzz-compile-model fuzz-compile-model.cpp)
	target_link_libraries(fuzz-compile-model PRIVATE Logging IR WASTPrint Runtime Platform)
endif()
