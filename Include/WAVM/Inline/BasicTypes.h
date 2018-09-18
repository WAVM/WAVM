#pragma once
#include <inttypes.h>
#include <cstddef>
#include <cstdint>

typedef uint8_t U8;
typedef int8_t I8;
typedef uint16_t U16;
typedef int16_t I16;
typedef uint32_t U32;
typedef int32_t I32;
typedef uint64_t U64;
typedef int64_t I64;
typedef float F32;
typedef double F64;

// The OSX libc defines uintptr_t to be a long where U32/U64 are int. This causes uintptr_t/uint64
// to be treated as distinct types for e.g. overloading. Work around it by defining our own
// Uptr/Iptr that are always int type.
template<size_t pointerSize> struct PointerIntHelper;
template<> struct PointerIntHelper<4>
{
	typedef I32 IntType;
	typedef U32 UnsignedIntType;
};
template<> struct PointerIntHelper<8>
{
	typedef I64 IntType;
	typedef U64 UnsignedIntType;
};
typedef PointerIntHelper<sizeof(size_t)>::UnsignedIntType Uptr;
typedef PointerIntHelper<sizeof(size_t)>::IntType Iptr;

// Redefine the PRI*PTR macros to match the type of Uptr.
#if defined(__APPLE__)
#undef PRIuPTR
#undef PRIxPTR
#if __SIZEOF_POINTER__ == 8
#define PRIuPTR PRIu64
#define PRIxPTR PRIx64
#elif __SIZEOF_POINTER__ == 4
#define PRIuPTR PRIu32
#define PRIxPTR PRIx32
#endif
#endif

union alignas(16) V128
{
	U8 u8[16];
	I8 i8[16];

	U16 u16[8];
	I16 i16[8];

	U32 u32[4];
	I32 i32[4];
	F32 f32[4];

	U64 u64[2];
	I64 i64[2];
	F64 f64[2];
};

inline bool operator==(const V128& left, const V128& right)
{
	return left.i64[0] == right.i64[0] && left.i64[1] == right.i64[1];
}