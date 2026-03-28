# Detect the size and alignment of the platform's synchronization primitive types.
# This replaces the manually-maintained #ifdef blocks in Mutex.h, RWMutex.h, etc.

if(MSVC)
    set(DETECT_SYNC_SIZES_SOURCE "
#include <windows.h>
#include <cstdio>

int main() {
    printf(\"MUTEX_SIZE=%zu\\n\", sizeof(SRWLOCK));
    printf(\"MUTEX_ALIGN=%zu\\n\", alignof(SRWLOCK));
    printf(\"COND_SIZE=%zu\\n\", sizeof(CONDITION_VARIABLE));
    printf(\"COND_ALIGN=%zu\\n\", alignof(CONDITION_VARIABLE));
    printf(\"RWMUTEX_SIZE=%zu\\n\", sizeof(SRWLOCK));
    printf(\"RWMUTEX_ALIGN=%zu\\n\", alignof(SRWLOCK));
    return 0;
}
")
else()
    set(DETECT_SYNC_SIZES_SOURCE "
#include <pthread.h>
#include <cstdio>

int main() {
    printf(\"MUTEX_SIZE=%zu\\n\", sizeof(pthread_mutex_t));
    printf(\"MUTEX_ALIGN=%zu\\n\", alignof(pthread_mutex_t));
    printf(\"COND_SIZE=%zu\\n\", sizeof(pthread_cond_t));
    printf(\"COND_ALIGN=%zu\\n\", alignof(pthread_cond_t));
    printf(\"RWMUTEX_SIZE=%zu\\n\", sizeof(pthread_rwlock_t));
    printf(\"RWMUTEX_ALIGN=%zu\\n\", alignof(pthread_rwlock_t));
    return 0;
}
")
endif()

# Write the detection source to a temporary file
file(WRITE "${CMAKE_BINARY_DIR}/detect_sync_sizes.cpp" "${DETECT_SYNC_SIZES_SOURCE}")

# Try to compile and run the detection program
try_run(
    DETECT_SYNC_RUN_RESULT
    DETECT_SYNC_COMPILE_RESULT
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/detect_sync_sizes.cpp"
    RUN_OUTPUT_VARIABLE DETECT_SYNC_OUTPUT
)

if(NOT DETECT_SYNC_COMPILE_RESULT)
    message(FATAL_ERROR "Failed to compile sync primitive size detection program")
endif()

if(NOT DETECT_SYNC_RUN_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to run sync primitive size detection program")
endif()

# Parse the output
string(REGEX MATCH "MUTEX_SIZE=([0-9]+)" _ "${DETECT_SYNC_OUTPUT}")
set(WAVM_PLATFORM_MUTEX_SIZE ${CMAKE_MATCH_1})

string(REGEX MATCH "MUTEX_ALIGN=([0-9]+)" _ "${DETECT_SYNC_OUTPUT}")
set(WAVM_PLATFORM_MUTEX_ALIGN ${CMAKE_MATCH_1})

string(REGEX MATCH "COND_SIZE=([0-9]+)" _ "${DETECT_SYNC_OUTPUT}")
set(WAVM_PLATFORM_COND_SIZE ${CMAKE_MATCH_1})

string(REGEX MATCH "COND_ALIGN=([0-9]+)" _ "${DETECT_SYNC_OUTPUT}")
set(WAVM_PLATFORM_COND_ALIGN ${CMAKE_MATCH_1})

string(REGEX MATCH "RWMUTEX_SIZE=([0-9]+)" _ "${DETECT_SYNC_OUTPUT}")
set(WAVM_PLATFORM_RWMUTEX_SIZE ${CMAKE_MATCH_1})

string(REGEX MATCH "RWMUTEX_ALIGN=([0-9]+)" _ "${DETECT_SYNC_OUTPUT}")
set(WAVM_PLATFORM_RWMUTEX_ALIGN ${CMAKE_MATCH_1})

if(NOT WAVM_PLATFORM_MUTEX_SIZE OR NOT WAVM_PLATFORM_MUTEX_ALIGN)
    message(FATAL_ERROR "Failed to parse mutex sizes from: ${DETECT_SYNC_OUTPUT}")
endif()

if(NOT WAVM_PLATFORM_COND_SIZE OR NOT WAVM_PLATFORM_COND_ALIGN)
    message(FATAL_ERROR "Failed to parse condition variable sizes from: ${DETECT_SYNC_OUTPUT}")
endif()

if(NOT WAVM_PLATFORM_RWMUTEX_SIZE OR NOT WAVM_PLATFORM_RWMUTEX_ALIGN)
    message(FATAL_ERROR "Failed to parse rwmutex sizes from: ${DETECT_SYNC_OUTPUT}")
endif()

message(STATUS "Detected Mutex size: ${WAVM_PLATFORM_MUTEX_SIZE}, alignment: ${WAVM_PLATFORM_MUTEX_ALIGN}")
message(STATUS "Detected ConditionVariable size: ${WAVM_PLATFORM_COND_SIZE}, alignment: ${WAVM_PLATFORM_COND_ALIGN}")
message(STATUS "Detected RWMutex size: ${WAVM_PLATFORM_RWMUTEX_SIZE}, alignment: ${WAVM_PLATFORM_RWMUTEX_ALIGN}")
