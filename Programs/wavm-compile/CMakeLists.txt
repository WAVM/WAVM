add_executable(wavm-compile wavm-compile.cpp)
target_link_libraries(wavm-compile Logging IR WASTParse WASM Runtime)
set_target_properties(wavm-compile PROPERTIES FOLDER Programs)
install(TARGETS wavm-compile DESTINATION bin)