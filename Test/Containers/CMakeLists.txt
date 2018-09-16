add_executable(HashSetTest HashSetTest.cpp)
target_link_libraries(HashSetTest Platform Logging)
set_target_properties(HashSetTest PROPERTIES FOLDER Testing)
add_test(NAME HashSetTest COMMAND $<TARGET_FILE:HashSetTest>)

add_executable(HashMapTest HashMapTest.cpp)
target_link_libraries(HashMapTest Platform Logging)
set_target_properties(HashMapTest PROPERTIES FOLDER Testing)
add_test(NAME HashMapTest COMMAND $<TARGET_FILE:HashMapTest>)