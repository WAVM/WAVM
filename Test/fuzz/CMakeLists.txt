if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND ENABLE_LIBFUZZER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer")
endif()

add_executable(FuzzWAST FuzzWAST.cpp wastFuzzDictionary.txt)
target_link_libraries(FuzzWAST Logging IR WASTParse)
set_target_properties(FuzzWAST PROPERTIES FOLDER Testing/Fuzzers)

add_executable(FuzzWASM FuzzWASM.cpp)
target_link_libraries(FuzzWASM Logging IR WASM)
set_target_properties(FuzzWASM PROPERTIES FOLDER Testing/Fuzzers)

add_executable(FuzzDisassemble FuzzDisassemble.cpp ModuleMatcher.h)
target_link_libraries(FuzzDisassemble Logging IR WASTParse WASTPrint WASM)
set_target_properties(FuzzDisassemble PROPERTIES FOLDER Testing/Fuzzers)

add_executable(FuzzAssemble FuzzAssemble.cpp ModuleMatcher.h)
target_link_libraries(FuzzAssemble Logging IR WASTParse WASTPrint WASM)
set_target_properties(FuzzAssemble PROPERTIES FOLDER Testing/Fuzzers)

if(ENABLE_RUNTIME)
	add_executable(FuzzCompile FuzzCompile.cpp)
	target_link_libraries(FuzzCompile Logging IR WASM Runtime)
	set_target_properties(FuzzCompile PROPERTIES FOLDER Testing/Fuzzers)

	add_executable(FuzzInstantiate FuzzInstantiate.cpp)
	target_link_libraries(FuzzInstantiate Logging IR WASM Runtime)
	set_target_properties(FuzzInstantiate PROPERTIES FOLDER Testing/Fuzzers)

	add_executable(FuzzCompileModel FuzzCompileModel.cpp)
	target_link_libraries(FuzzCompileModel Logging IR WASTPrint Runtime)
	set_target_properties(FuzzCompileModel PROPERTIES FOLDER Testing/Fuzzers)
endif()
