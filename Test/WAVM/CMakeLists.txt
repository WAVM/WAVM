file(GLOB Sources "*.wast")
add_custom_target(WAVMTests SOURCES ${Sources})

set(TEST_BIN ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIGURATION}/Test)

add_test(WAVM_known_failures ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/WAVM_known_failures.wast)

add_test(exceptions ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/exceptions.wast)
add_test(simd ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/simd.wast)
