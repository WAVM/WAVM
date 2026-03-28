cmake_minimum_required(VERSION 3.8.0)
project(embedder-test LANGUAGES C CXX)

add_executable(embedder-example embedder-example.cpp)

option(INCLUDE_WAVM_PROJECT false)
if(INCLUDE_WAVM_PROJECT)
	add_subdirectory(
		${CMAKE_CURRENT_SOURCE_DIR}/../../..
		${CMAKE_CURRENT_BINARY_DIR}/WAVM)

	target_link_libraries(embedder-example libWAVM)
	set(WAVM_LIB_TARGET libWAVM)
else()
	find_package(WAVM REQUIRED CONFIG)

	target_link_libraries(embedder-example WAVM::libWAVM)
	set(WAVM_LIB_TARGET WAVM::libWAVM)
endif()

# On Windows, copy the WAVM DLL next to the executable.
# On Unix, set RPATH to the WAVM library directory so the binary can find libWAVM at runtime.
if(WIN32)
	add_custom_command(TARGET embedder-example POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:${WAVM_LIB_TARGET}>
			$<TARGET_FILE_DIR:embedder-example>)
else()
	target_link_options(embedder-example PRIVATE
		"-Wl,-rpath,$<TARGET_FILE_DIR:${WAVM_LIB_TARGET}>")
endif()