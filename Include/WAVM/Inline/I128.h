#pragma once

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

#define INT128_MIN I128(U64(0), I64(0x8000000000000000ll))
#define INT128_MAX I128(UINT64_MAX, INT64_MAX)

namespace WAVM {
	struct alignas(16) I128
	{
		I128() : lowU64(0), highU64(0), overflow(false) {}
		I128(I32 value)
		{
			lowI64 = value;
			highI64 = value >= 0 ? 0 : -1;
			overflow = false;
		}
		I128(U32 value)
		{
			lowU64 = value;
			highU64 = 0;
			overflow = false;
		}
		I128(I64 value)
		{
			lowI64 = value;
			highI64 = value >= 0 ? 0 : -1;
			overflow = false;
		}
		I128(U64 value)
		{
			lowU64 = value;
			highU64 = 0;
			overflow = false;
		}
		I128(I64 inLow, I64 inHigh, bool inOverflow = false)
		: lowI64(inLow), highI64(inHigh), overflow(inOverflow)
		{
		}
		I128(U64 inLow, I64 inHigh, bool inOverflow = false)
		: lowU64(inLow), highI64(inHigh), overflow(inOverflow)
		{
		}
		I128(U64 inLow, U64 inHigh, bool inOverflow = false)
		: lowU64(inLow), highU64(inHigh), overflow(inOverflow)
		{
		}

		// Conversion to smaller integer types.
		explicit operator U32() const;
		explicit operator I32() const;
		explicit operator U64() const;
		explicit operator I64() const;

		// Conversion to FP types.
		explicit operator F32() const;
		explicit operator F64() const;

		// Overflow handling
		bool getOverflow() const { return overflow; }
		friend I128 ignoreOverflow(I128 i) { return I128(i.lowU64, i.highU64, i.overflow); }
		friend I128 assumeNoOverflow(I128 i)
		{
			errorUnless(!i.overflow);
			return i;
		}

		// Unary operators
		friend I128 operator+(I128 a);
		friend I128 operator-(I128 a);

		friend I128 operator~(I128 a);

		// Binary operators
		friend I128 operator+(I128 a, I128 b);
		friend I128 operator-(I128 a, I128 b);
		friend I128 operator*(I128 a, I128 b);
		friend I128 operator/(I128 a, I128 b);
		friend I128 operator%(I128 a, I128 b);

		friend I128 operator>>(I128 a, I128 b);
		friend I128 operator<<(I128 a, I128 b);

		friend I128 operator^(I128 a, I128 b);
		friend I128 operator&(I128 a, I128 b);
		friend I128 operator|(I128 a, I128 b);

		// Comparison operators
		friend bool operator>(I128 a, I128 b);
		friend bool operator>=(I128 a, I128 b);
		friend bool operator<(I128 a, I128 b);
		friend bool operator<=(I128 a, I128 b);
		friend bool operator==(I128 a, I128 b);
		friend bool operator!=(I128 a, I128 b);

		// Assignment operators

		I128& operator=(I128 copyee);

		I128& operator+=(I128 b) { return *this = *this + b; }
		I128& operator-=(I128 b) { return *this = *this - b; }
		I128& operator*=(I128 b) { return *this = *this * b; }
		I128& operator>>=(I128 b) { return *this = *this >> b; }
		I128& operator<<=(I128 b) { return *this = *this << b; }
		I128& operator^=(I128 b) { return *this = *this ^ b; }
		I128& operator&=(I128 b) { return *this = *this & b; }
		I128& operator|=(I128 b) { return *this = *this | b; }
		I128& operator/=(I128 b) { return *this = *this / b; }
		I128& operator%=(I128 b) { return *this = *this % b; }

	private:
		union
		{
			U64 lowU64;
			I64 lowI64;
		};
		union
		{
			U64 highU64;
			I64 highI64;
		};
		bool overflow;

		static I128 udivmod(I128 a, I128 b, I128& outRemainder);
	};
}

#include "WAVM/Inline/I128Impl.h"
