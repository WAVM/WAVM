#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"

#define SUPPRESS_UNUSED(variable) (void)(variable);

#ifdef _WIN32
#define FORCEINLINE __forceinline
#define FORCENOINLINE __declspec(noinline)
#define PACKED_STRUCT(definition)                                                                  \
	__pragma(pack(push, 1)) definition;                                                            \
	__pragma(pack(pop))
#elif defined(__GNUC__)
#define FORCEINLINE inline __attribute__((always_inline))
#define FORCENOINLINE __attribute__((noinline))
#define PACKED_STRUCT(definition) definition __attribute__((packed));
#else
#define FORCEINLINE
#define FORCENOINLINE
#define PACKED_STRUCT(definition) definition
#endif

#if defined(__GNUC__)
#define NO_ASAN __attribute__((no_sanitize_address))
#define RETURNS_TWICE __attribute__((returns_twice))
#define VALIDATE_AS_PRINTF(formatStringIndex, firstFormatArgIndex)                                 \
	__attribute__((format(printf, formatStringIndex, firstFormatArgIndex)))
#define UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#define NO_ASAN
#define RETURNS_TWICE
#define VALIDATE_AS_PRINTF(formatStringIndex, firstFormatArgIndex)
#define UNLIKELY(condition) (condition)
#endif

#if defined(__clang__) && WAVM_ENABLE_UBSAN
#define NO_UBSAN __attribute__((no_sanitize("undefined")))
#elif defined(__GNUC__) && WAVM_ENABLE_UBSAN
#define NO_UBSAN __attribute__((no_sanitize_undefined))
#else
#define NO_UBSAN
#endif

#ifdef _WIN32
#define DEBUG_TRAP() __debugbreak()
#elif !defined(__GNUC__) || WAVM_ENABLE_LIBFUZZER || !(defined(__i386__) || defined(__x86_64__))
// Use abort() instead of int3 when fuzzing, since
// libfuzzer doesn't handle the breakpoint trap.
#define DEBUG_TRAP() abort()
#else
#define DEBUG_TRAP() __asm__ __volatile__("int3")
#endif

// Define WAVM_DEBUG to 0 or 1 depending on whether it's a debug build.
#if defined(NDEBUG)
#define WAVM_DEBUG 0
#else
#define WAVM_DEBUG 1
#endif

#ifdef _MSC_VER
#define SCOPED_DISABLE_SECURE_CRT_WARNINGS(code)                                                   \
	__pragma(warning(push)) __pragma(warning(disable : 4996)) code __pragma(warning(pop))
#else
#define SCOPED_DISABLE_SECURE_CRT_WARNINGS(code) code
#endif