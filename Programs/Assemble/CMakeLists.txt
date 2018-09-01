add_executable(Assemble Assemble.cpp)
target_link_libraries(Assemble Logging IR WASTParse WASM)
set_target_properties(Assemble PROPERTIES FOLDER Programs)
