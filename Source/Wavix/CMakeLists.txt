add_executable(wavix file.cpp memory.cpp process.cpp wavix.cpp wavix.h)
target_link_libraries(wavix Logging IR WASM Runtime)
set_target_properties(wavix PROPERTIES FOLDER Programs)
