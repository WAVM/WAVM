#pragma once

#include "Core.h"
#include <inttypes.h>

namespace Floats
{	
	// The components of a 64-bit float
	union F64Components
	{
		struct
		{
			uint64 significand : 52;
			uint64 exponent : 11;
			uint64 sign : 1;
		} bits;
		float64 value;
		uint64 bitcastInt;
	};

	// The components of a 32-bit float.
	union F32Components
	{
		struct
		{
			uint32 significand : 23;
			uint32 exponent : 8;
			uint32 sign : 1;
		} bits;
		float32 value;
		uint32 bitcastInt;
	};

	template<typename FloatComponents,typename Float,uint64 maxExponent>
	std::string asString(Float f)
	{
		FloatComponents components;
		components.value = f;
	
		auto sign = std::string(components.bits.sign ? "-" : "+");

		if(components.bits.exponent == maxExponent)
		{
			if(components.bits.significand == 0) { return sign + "infinity"; }
			else
			{
				char significandString[32];
				auto numChars = std::sprintf(significandString,"%" PRIx64,(uint64)components.bits.significand);
				if(unsigned(numChars) + 1 > sizeof(significandString)) { abort(); }
				return sign + "nan(0x" + significandString + ")";
			}
		}
		else
		{
			char buffer[32];
			auto numChars = std::sprintf(buffer,"%.13a",f);
			if(unsigned(numChars) + 1 > sizeof(buffer))
			{
				abort();
			}
			return buffer;
		}
	}

	inline std::string asString(float32 f32) { return asString<F32Components,float32,0xff>(f32); }
	inline std::string asString(float64 f64) { return asString<F64Components,float64,0x7ff>(f64); }
}
