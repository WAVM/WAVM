#pragma once

#include "WAVM/Inline/BasicTypes.h"

namespace WAVM {
	template<typename Float> struct FloatComponents;

	// The components of a 64-bit float
	template<> struct FloatComponents<F64>
	{
		typedef U64 Bits;
		typedef F64 Float;

		enum Constants : I64
		{
			maxSignificand = 0xfffffffffffff,
			numSignificandBits = 52,
			numSignificandHexits = 13,
			canonicalSignificand = 0x8000000000000ull,

			denormalExponent = -1023,
			minNormalExponent = -1022,
			maxNormalExponent = 1023,
			exponentBias = 1023,
			maxExponentBits = 0x7ff,
		};

		union
		{
			struct
			{
				U64 significand : 52;
				U64 exponent : 11;
				U64 sign : 1;
			} bits;
			Float value;
			Bits bitcastInt;
		};
	};

	// The components of a 32-bit float.
	template<> struct FloatComponents<F32>
	{
		typedef U32 Bits;
		typedef F32 Float;

		enum Constants : I32
		{
			maxSignificand = 0x7fffff,
			numSignificandBits = 23,
			numSignificandHexits = 6,
			canonicalSignificand = 0x400000,

			denormalExponent = -127,
			minNormalExponent = -126,
			maxNormalExponent = 127,
			exponentBias = 127,
			maxExponentBits = 0xff,
		};

		union
		{
			struct
			{
				U32 significand : 23;
				U32 exponent : 8;
				U32 sign : 1;
			} bits;
			Float value;
			Bits bitcastInt;
		};
	};
}
