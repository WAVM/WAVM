#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include "WAVM/Platform/Defines.h"

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
#if defined(__APPLE__) || defined(__WAVIX__)
#if __SIZEOF_POINTER__ == 8
#define WAVM_PRIuPTR PRIu64
#define WAVM_PRIxPTR PRIx64
typedef U64 Uptr;
typedef I64 Iptr;
#elif __SIZEOF_POINTER__ == 4
#define WAVM_PRIuPTR PRIu32
#define WAVM_PRIxPTR PRIx32
typedef U32 Uptr;
typedef I32 Iptr;
#endif
#else
#define WAVM_PRIuPTR PRIuPTR
#define WAVM_PRIxPTR PRIxPTR
typedef uintptr_t Uptr;
typedef intptr_t Iptr;
#endif

WAVM_ALIGNED_STRUCT(
	16,
	union V128 {
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
	});
