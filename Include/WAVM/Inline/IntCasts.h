#pragma once

#include <cinttypes>
#include <type_traits>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"

namespace WAVM {

	// narrow: runtime-checked lossless integer conversion.
	// The integer value must be representable in the target type.
	// Compile-time rejects conversions that are statically guaranteed lossless (use implicit
	// conversion for those).
	template<typename To, typename From> To narrow(const char* context, From value)
	{
		static_assert(std::is_integral_v<From> && std::is_integral_v<To>);
		static_assert(
			!((std::is_signed_v<From> == std::is_signed_v<To> && sizeof(To) >= sizeof(From))
			  || (std::is_unsigned_v<From> && std::is_signed_v<To> && sizeof(To) > sizeof(From))),
			"narrow is for conversions that are not statically guaranteed lossless; "
			"this conversion is always lossless and doesn't need an explicit cast");
		To result = To(value);
		if(From(result) != value)
		{
			if constexpr(std::is_signed_v<From>)
			{
				Errors::fatalfWithCallStack("narrow failed (%s): %" PRId64 " does not fit in %s%zu",
											context,
											I64(value),
											std::is_signed_v<To> ? "I" : "U",
											sizeof(To) * 8);
			}
			else
			{
				Errors::fatalfWithCallStack("narrow failed (%s): %" PRIu64 " does not fit in %s%zu",
											context,
											U64(value),
											std::is_signed_v<To> ? "I" : "U",
											sizeof(To) * 8);
			}
		}
		return result;
	}

	// wrap: modular integer conversion.
	// The integer value is mapped to the target type's range via modular arithmetic.
	template<typename To, typename From> constexpr To wrap(From value)
	{
		static_assert(std::is_integral_v<From> && std::is_integral_v<To>);
		return To(value);
	}
}
