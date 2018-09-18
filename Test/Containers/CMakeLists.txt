WAVM_ADD_EXECUTABLE(HashSetTest Testing HashSetTest.cpp)
target_link_libraries(HashSetTest PRIVATE Platform Logging)
add_test(NAME HashSetTest COMMAND $<TARGET_FILE:HashSetTest>)

WAVM_ADD_EXECUTABLE(HashMapTest Testing HashMapTest.cpp)
target_link_libraries(HashMapTest PRIVATE Platform Logging)
add_test(NAME HashMapTest COMMAND $<TARGET_FILE:HashMapTest>)