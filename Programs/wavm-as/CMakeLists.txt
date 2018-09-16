add_executable(wavm-as wavm-as.cpp)
target_link_libraries(wavm-as Logging IR WASTParse WASM)
set_target_properties(wavm-as PROPERTIES FOLDER Programs)
install(TARGETS wavm-as DESTINATION bin)
