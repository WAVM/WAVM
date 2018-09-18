add_executable(wavm-run wavm-run.cpp)
target_link_libraries(wavm-run Logging IR WASTParse WASM Runtime Emscripten ThreadTest)
set_target_properties(wavm-run PROPERTIES FOLDER Programs)
install(TARGETS wavm-run DESTINATION bin)