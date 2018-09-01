add_executable(HashSetTest HashSetTest.cpp)
target_link_libraries(HashSetTest Platform Logging)
set_target_properties(HashSetTest PROPERTIES FOLDER Testing)
add_test(HashSetTest ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIGURATION}/HashSetTest)

add_executable(HashMapTest HashMapTest.cpp)
target_link_libraries(HashMapTest Platform Logging)
set_target_properties(HashMapTest PROPERTIES FOLDER Testing)
add_test(HashMapTest ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIGURATION}/HashMapTest)