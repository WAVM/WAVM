if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND ENABLE_LIBFUZZER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer")
	add_definitions(-DENABLE_LIBFUZZER=1)
else()
	add_definitions(-DENABLE_LIBFUZZER=0)
endif()

add_executable(FuzzWAST FuzzWAST.cpp)
target_link_libraries(FuzzWAST Logging IR WAST)
set_target_properties(FuzzWAST PROPERTIES FOLDER Testing)

add_executable(FuzzWASM FuzzWASM.cpp)
target_link_libraries(FuzzWASM Logging IR WASM)
set_target_properties(FuzzWASM PROPERTIES FOLDER Testing)

add_executable(FuzzRuntime FuzzRuntime.cpp)
target_link_libraries(FuzzRuntime Logging IR WAST WASM Runtime)
set_target_properties(FuzzRuntime PROPERTIES FOLDER Testing)

add_executable(FuzzDisassemble FuzzDisassemble.cpp)
target_link_libraries(FuzzDisassemble Logging IR WAST WASM)
set_target_properties(FuzzDisassemble PROPERTIES FOLDER Testing)
