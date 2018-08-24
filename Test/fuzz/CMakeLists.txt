if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND ENABLE_LIBFUZZER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer")
endif()

add_executable(FuzzWAST FuzzWAST.cpp)
target_link_libraries(FuzzWAST Logging IR WAST)
set_target_properties(FuzzWAST PROPERTIES FOLDER Testing/Fuzzers)

add_executable(FuzzWASM FuzzWASM.cpp)
target_link_libraries(FuzzWASM Logging IR WASM)
set_target_properties(FuzzWASM PROPERTIES FOLDER Testing/Fuzzers)

add_executable(FuzzRuntime FuzzRuntime.cpp StubResolver.h)
target_link_libraries(FuzzRuntime Logging IR WAST WASM Runtime)
set_target_properties(FuzzRuntime PROPERTIES FOLDER Testing/Fuzzers)

add_executable(FuzzRuntimeModel FuzzRuntimeModel.cpp StubResolver.h)
target_link_libraries(FuzzRuntimeModel Logging IR WAST WASM Runtime)
set_target_properties(FuzzRuntimeModel PROPERTIES FOLDER Testing/Fuzzers)

add_executable(FuzzDisassemble FuzzDisassemble.cpp)
target_link_libraries(FuzzDisassemble Logging IR WAST WASM)
set_target_properties(FuzzDisassemble PROPERTIES FOLDER Testing/Fuzzers)
