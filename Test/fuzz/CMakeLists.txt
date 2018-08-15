if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND ENABLE_LIBFUZZER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer")
	add_definitions(-DENABLE_LIBFUZZER=1)
else()
	add_definitions(-DENABLE_LIBFUZZER=0)
endif()

add_executable(fuzz-parser fuzz-parser.cpp)
target_link_libraries(fuzz-parser Logging IR WAST WASM)
set_target_properties(fuzz-parser PROPERTIES FOLDER Testing)

add_executable(fuzz-runtime fuzz-runtime.cpp)
target_link_libraries(fuzz-runtime Logging IR WAST WASM Runtime)
set_target_properties(fuzz-runtime PROPERTIES FOLDER Testing)
