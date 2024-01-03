#pragma once

#include <cstddef>
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace WAVM::Utils {
	/*
	Copied from fast_io
	*/
	inline void secure_clear(void* data, ::std::size_t size) noexcept
	{
#if defined(_MSC_VER) && !defined(__clang__)
		/*
		Referenced from Microsoft's winnt.h.
		forceinline for this is nonsense
		*/

#if defined(_M_AMD64)
		__stosb(__builtin_bit_cast(unsigned char*, data), 0, size);
#else
		volatile char* vptr = (volatile char*)data;
		while(size)
		{
#if !defined(_M_CEE) && (defined(_M_ARM) || defined(_M_ARM64))
			__iso_volatile_store8(vptr, 0);
#else
			*vptr = 0;
#endif
			++vptr;
			--size;
		}
#endif // _M_AMD64
#else
		/*
		https://github.com/bminor/glibc/blob/master/string/explicit_bzero.c
		Referenced from glibc
		*/
		__builtin_memset(data, 0, size);
		__asm__ __volatile__("" ::"r"(data) : "memory");
#endif
	}

}
