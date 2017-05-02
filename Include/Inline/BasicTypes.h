#pragma once
#include <cstdint>
#include <cstddef>

typedef uint8_t uint8;
typedef int8_t int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint64_t uint64;
typedef int64_t int64;
typedef float float32;
typedef double float64;

// The OSX libc defines uintptr_t to be a long where uint32/uint64 are int. This causes uintptr_t/uint64 to be treated as distinct types for e.g. overloading.
// Work around it by defining our own uintp/intp that are always int type.
template<size_t pointerSize>
struct PointerIntHelper;
template<> struct PointerIntHelper<4> { typedef int32 IntType; typedef uint32 UnsignedIntType; };
template<> struct PointerIntHelper<8> { typedef int64 IntType; typedef uint64 UnsignedIntType; };
typedef PointerIntHelper<sizeof(size_t)>::UnsignedIntType uintp;
typedef PointerIntHelper<sizeof(size_t)>::IntType intp;
