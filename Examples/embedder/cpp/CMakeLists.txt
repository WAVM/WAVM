cmake_minimum_required(VERSION 3.8.0)
project(embedder-test LANGUAGES C CXX)

add_executable(embedder-example embedder-example.cpp)

option(INCLUDE_WAVM_PROJECT false)
if(INCLUDE_WAVM_PROJECT)
	add_subdirectory(
		${CMAKE_CURRENT_SOURCE_DIR}/../../..
		${CMAKE_CURRENT_BINARY_DIR}/WAVM)

	target_link_libraries(embedder-example libWAVM)
else()
	find_package(WAVM REQUIRED CONFIG)

	target_link_libraries(embedder-example WAVM::libWAVM)
endif()