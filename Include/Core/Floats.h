#pragma once

#include "Core.h"

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

	// Prints a floating point value to a string, using the WebAssembly syntax for text floats.
	template<typename FloatComponents,typename Float,uint64 maxExponent,uintp numSignificandHexits>
	std::string asString(Float f)
	{
		FloatComponents components;
		components.value = f;
	
		auto sign = std::string(components.bits.sign ? "-" : "+");

		if(components.bits.exponent == maxExponent)
		{
			// Handle infinity.
			if(components.bits.significand == 0) { return sign + "infinity"; }
			else
			{
				// Handle NaN.
				char significandString[numSignificandHexits + 1];
				for(uintp hexitIndex = 0;hexitIndex < numSignificandHexits;++hexitIndex)
				{
					auto hexitValue = char((components.bits.significand >> ((numSignificandHexits - hexitIndex - 1) * 4)) & 0xf);
					significandString[hexitIndex] = hexitValue >= 10 ? ('a' + hexitValue - 10) : ('0' + hexitValue);
				}
				significandString[numSignificandHexits] = 0;
				return sign + "nan:0x" + significandString + "";
			}
		}
		else
		{
			// If it's not infinite or NaN, just use the STL decimal float printing.
			char buffer[32];
			auto numChars = std::sprintf(buffer,"%.13a",f);
			if(unsigned(numChars) + 1 > sizeof(buffer))
			{
				Core::error("not enough space in Float::asString buffer");
			}
			return buffer;
		}
	}

	inline std::string asString(float32 f32) { return asString<F32Components,float32,0xff,6>(f32); }
	inline std::string asString(float64 f64) { return asString<F64Components,float64,0x7ff,13>(f64); }
}
