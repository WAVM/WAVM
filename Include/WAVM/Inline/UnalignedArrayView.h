#pragma once

#include <cstring>
#include <type_traits>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM {

	// View over a potentially misaligned array of trivially-copyable elements.
	// Uses memcpy to read elements, avoiding undefined behavior from misaligned access.
	template<typename T> struct UnalignedArrayView
	{
		static_assert(std::is_trivially_copyable<T>::value,
					  "UnalignedArrayView requires a trivially copyable type");
		const U8* base;
		Uptr count;

		T operator[](Uptr index) const
		{
			WAVM_ASSERT(index < count);
			T value;
			memcpy(&value, base + index * sizeof(T), sizeof(T));
			return value;
		}
	};

}
