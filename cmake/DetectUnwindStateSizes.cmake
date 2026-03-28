# Detect the size and alignment of the platform's unwind state structures.
# This is needed to create a platform-independent UnwindState struct that can
# hold the platform-specific unwind context and cursor.

if(MSVC)
    # On Windows, we use CONTEXT and related structures
    set(DETECT_UNWIND_SIZES_SOURCE "
#include <windows.h>
#include <cstdint>
#include <cstdio>

struct UnwindStateImpl {
    CONTEXT context;
    DWORD64 imageBase;
    void* runtimeFunction;  // RUNTIME_FUNCTION*
    intptr_t ipAdjustment;
    bool valid;
};

int main() {
    printf(\"STATE_SIZE=%zu\\n\", sizeof(UnwindStateImpl));
    printf(\"STATE_ALIGN=%zu\\n\", alignof(UnwindStateImpl));
    return 0;
}
")
else()
    # On POSIX, we use libunwind's unw_context_t and unw_cursor_t
    if(WAVM_BUILD_WAVMUNWIND)
        set(DETECT_UNWIND_INCLUDE_DIR "${WAVM_SOURCE_DIR}/ThirdParty/libunwind/include")
    else()
        set(DETECT_UNWIND_INCLUDE_DIR "")
    endif()

    set(DETECT_UNWIND_SIZES_SOURCE "
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <cstdint>
#include <cstdio>

struct UnwindStateImpl {
    unw_context_t context;
    unw_cursor_t cursor;
    intptr_t ipAdjustment;
    bool valid;
};

int main() {
    printf(\"STATE_SIZE=%zu\\n\", sizeof(UnwindStateImpl));
    printf(\"STATE_ALIGN=%zu\\n\", alignof(UnwindStateImpl));
    return 0;
}
")
endif()

# Write the detection source to a temporary file
file(WRITE "${CMAKE_BINARY_DIR}/detect_unwind_sizes.cpp" "${DETECT_UNWIND_SIZES_SOURCE}")

# Try to compile and run the detection program
if(MSVC)
    try_run(
        DETECT_UNWIND_RUN_RESULT
        DETECT_UNWIND_COMPILE_RESULT
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_BINARY_DIR}/detect_unwind_sizes.cpp"
        RUN_OUTPUT_VARIABLE DETECT_UNWIND_OUTPUT
    )
else()
    if(DETECT_UNWIND_INCLUDE_DIR)
        try_run(
            DETECT_UNWIND_RUN_RESULT
            DETECT_UNWIND_COMPILE_RESULT
            "${CMAKE_BINARY_DIR}"
            "${CMAKE_BINARY_DIR}/detect_unwind_sizes.cpp"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${DETECT_UNWIND_INCLUDE_DIR}"
            RUN_OUTPUT_VARIABLE DETECT_UNWIND_OUTPUT
        )
    else()
        try_run(
            DETECT_UNWIND_RUN_RESULT
            DETECT_UNWIND_COMPILE_RESULT
            "${CMAKE_BINARY_DIR}"
            "${CMAKE_BINARY_DIR}/detect_unwind_sizes.cpp"
            RUN_OUTPUT_VARIABLE DETECT_UNWIND_OUTPUT
        )
    endif()
endif()

if(NOT DETECT_UNWIND_COMPILE_RESULT)
    message(FATAL_ERROR "Failed to compile unwind state size detection program")
endif()

if(NOT DETECT_UNWIND_RUN_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to run unwind state size detection program")
endif()

# Parse the output
string(REGEX MATCH "STATE_SIZE=([0-9]+)" _ "${DETECT_UNWIND_OUTPUT}")
set(WAVM_PLATFORM_UNWIND_STATE_SIZE ${CMAKE_MATCH_1})

string(REGEX MATCH "STATE_ALIGN=([0-9]+)" _ "${DETECT_UNWIND_OUTPUT}")
set(WAVM_PLATFORM_UNWIND_STATE_ALIGN ${CMAKE_MATCH_1})

if(NOT WAVM_PLATFORM_UNWIND_STATE_SIZE OR NOT WAVM_PLATFORM_UNWIND_STATE_ALIGN)
    message(FATAL_ERROR "Failed to parse unwind state sizes from: ${DETECT_UNWIND_OUTPUT}")
endif()

message(STATUS "Detected UnwindState size: ${WAVM_PLATFORM_UNWIND_STATE_SIZE}, alignment: ${WAVM_PLATFORM_UNWIND_STATE_ALIGN}")
