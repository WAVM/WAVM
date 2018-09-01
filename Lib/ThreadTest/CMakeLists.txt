set(Sources ThreadTest.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/ThreadTest/ThreadTest.h)

WAVM_ADD_LIBRARY(ThreadTest ${Sources} ${PublicHeaders})
set_target_properties(ThreadTest PROPERTIES FOLDER Testing)

add_definitions(-DTHREADTEST_API=DLL_EXPORT)

target_link_libraries(ThreadTest Logging Platform Runtime)
