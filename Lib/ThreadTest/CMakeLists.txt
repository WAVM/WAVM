set(Sources ThreadTest.cpp)
set(PublicHeaders ${WAVM_INCLUDE_DIR}/ThreadTest/ThreadTest.h)

WAVM_ADD_LIBRARY(ThreadTest ${Sources} ${PublicHeaders})
target_link_libraries(ThreadTest PRIVATE Logging Platform Runtime)
