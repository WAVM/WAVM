set(Sources
	exceptions.wast
	fuzz_regression.wast
	llvm_bugs.wast
	simd.wast
	threads.wast
	trunc_sat.wast)
add_custom_target(WAVMTests SOURCES ${Sources})
set_target_properties(WAVMTests PROPERTIES FOLDER Testing)

set(TEST_BIN ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIGURATION}/Test)

add_test(exceptions ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/exceptions.wast)
add_test(fuzz_regression ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/fuzz_regression.wast)
add_test(llvm_bugs ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/llvm_bugs.wast)
add_test(simd ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/simd.wast)
add_test(threads ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/threads.wast)
add_test(trunc_sat ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/trunc_sat.wast)

add_subdirectory(Containers)
