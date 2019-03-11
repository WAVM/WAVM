#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

#ifdef _WIN32
#include <intrin.h>
#endif

namespace WAVM { namespace Platform {
	// The number of bytes in a cache line: assume 64 for now.
	enum
	{
		numCacheLineBytes = 64
	};

	// countLeadingZeroes returns the number of leading zeroes, or the bit width of the input if no
	// bits are set.
	inline U32 countLeadingZeroes(U32 value)
	{
#ifdef _WIN32
		// BitScanReverse returns 0 if the input is 0.
		unsigned long result;
		return _BitScanReverse(&result, value) ? (31 - result) : 32;
#else
		return value == 0 ? 32 : __builtin_clz(value);
#endif
	}

	inline U64 countLeadingZeroes(U64 value)
	{
#ifdef _WIN64
		unsigned long result;
		return _BitScanReverse64(&result, value) ? (63 - result) : 64;
#elif defined(_WIN32)
		DEBUG_TRAP();
#else
		return value == 0 ? 64 : __builtin_clzll(value);
#endif
	}

	// countTrailingZeroes returns the number of trailing zeroes, or the bit width of the input if
	// no bits are set.
	inline U32 countTrailingZeroes(U32 value)
	{
#ifdef _WIN32
		// BitScanForward returns 0 if the input is 0.
		unsigned long result;
		return _BitScanForward(&result, value) ? result : 32;
#else
		return value == 0 ? 32 : __builtin_ctz(value);
#endif
	}
	inline U64 countTrailingZeroes(U64 value)
	{
#ifdef _WIN64
		unsigned long result;
		return _BitScanForward64(&result, value) ? result : 64;
#elif defined(_WIN32)
		DEBUG_TRAP();
#else
		return value == 0 ? 64 : __builtin_ctzll(value);
#endif
	}

	inline U64 floorLogTwo(U64 value) { return value <= 1 ? 0 : 63 - countLeadingZeroes(value); }
	inline U32 floorLogTwo(U32 value) { return value <= 1 ? 0 : 31 - countLeadingZeroes(value); }
	inline U64 ceilLogTwo(U64 value)
	{
		return value <= 1 ? 0 : 63 - countLeadingZeroes(value * 2 - 1);
	}
	inline U32 ceilLogTwo(U32 value)
	{
		return value <= 1 ? 0 : 31 - countLeadingZeroes(value * 2 - 1);
	}

	inline U64 saturateToBounds(U64 value, U64 maxValue)
	{
		return U64(value + ((I64(maxValue - value) >> 63) & (maxValue - value)));
	}

	inline U32 saturateToBounds(U32 value, U32 maxValue)
	{
		return U32(value + ((I32(maxValue - value) >> 31) & (maxValue - value)));
	}

	// Byte-wise memcpy and memset: these are different from the C library versions because in the
	// event of a trap while reading/writing memory, they guarantee that every byte preceding the
	// byte that trapped will have been written.
	// On X86 they use "rep movsb" (for copy) and "rep stosb" (for set) which are microcoded on
	// Intel Ivy Bridge and later processors, and supposedly competitive with the C library
	// functions. On other architectures, it will fall back to a C loop, which is not likely to be
	// competitive with the C library function.

	inline void bytewiseMemCopy(U8* dest, const U8* source, Uptr numBytes)
	{
#ifdef _WIN32
		__movsb(dest, source, numBytes);
#elif defined(__i386__) || defined(__x86_64__)
		asm volatile("rep movsb"
					 : "=D"(dest), "=S"(source), "=c"(numBytes)
					 : "0"(dest), "1"(source), "2"(numBytes)
					 : "memory");
#else
		for(Uptr index = 0; index < numBytes; ++index) { dest[index] = source[index]; }
#endif
	}

	inline void bytewiseMemCopyReverse(U8* dest, const U8* source, Uptr numBytes)
	{
		for(Uptr index = 0; index < numBytes; ++index)
		{ dest[numBytes - index - 1] = source[numBytes - index - 1]; }
	}

	inline void bytewiseMemSet(U8* dest, U8 value, Uptr numBytes)
	{
#ifdef _WIN32
		__stosb(dest, value, numBytes);
#elif defined(__i386__) || defined(__x86_64__)
		asm volatile("rep stosb"
					 : "=D"(dest), "=a"(value), "=c"(numBytes)
					 : "0"(dest), "1"(value), "2"(numBytes)
					 : "memory");
#else
		for(Uptr index = 0; index < numBytes; ++index) { dest[index] = value; }
#endif
	}

	// Byte-wise memmove: this uses the above byte-wise memcpy, but if the source and destination
	// buffers overlap so the copy would overwrite the end of the source buffer before it is copied
	// from, then split the copy into two: the overlapping end of the source buffer is copied first,
	// so it can be overwritten by the second copy safely.

	inline void bytewiseMemMove(U8* dest, U8* source, Uptr numBytes)
	{
		if(source < dest && source + numBytes > dest)
		{ bytewiseMemCopyReverse(dest, source, numBytes); }
		else
		{
			bytewiseMemCopy(dest, source, numBytes);
		}
	}
}}
