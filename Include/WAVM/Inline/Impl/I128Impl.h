// IWYU pragma: private, include "WAVM/Inline/I128.h"
// You should only include this file indirectly by including I128.h.
#pragma once
#ifndef WAVM_INLINE_IMPL_I128IMPL_H
#define WAVM_INLINE_IMPL_I128IMPL_H

// I128 Encoding Scheme
// ====================
//
// Storage:
//   Two 64-bit words accessed via unions: lowU64/lowI64 and highU64/highI64.
//
// Value interpretation:
//   The encoding is two's complement over 128 bits, with the mathematical value:
//     value = (highI64 * 2^64) + lowU64
//
//   The sign is determined by bit 127 (the MSB of highI64). When highI64 < 0, the
//   number is negative.
//
// Range:
//   Standard 128-bit two's complement would have range [-2^127, 2^127 - 1], but this
//   implementation reserves the most negative bit pattern as NaN, giving a symmetric
//   range: [-(2^127 - 1), +(2^127 - 1)].
//
// Special values:
//   NaN: high = 0x8000000000000000, low = 0x0000000000000000  (bit pattern for -2^127)
//   min: high = 0x8000000000000000, low = 0x0000000000000001  (value: -(2^127 - 1))
//   max: high = 0x7FFFFFFFFFFFFFFF, low = 0xFFFFFFFFFFFFFFFF  (value: +(2^127 - 1))
//
// Negation preserves NaN:
//   -NaN = ~(0x8000000000000000:0) + 1 = (0x7FFFFFFFFFFFFFFF:0xFFFFFFFFFFFFFFFF) + 1
//        = (0x8000000000000000:0) = NaN
//
// Comparison ordering:
//   The high words are compared as signed (highI64) to correctly order positive vs
//   negative numbers. When high words are equal, the low words must be compared as
//   unsigned (lowU64) because they represent an unsigned offset within a "quadrant"
//   of the number space. For example:
//     - I128(U64_MAX) = (high=0, low=0xFFFFFFFFFFFFFFFF) should be > I128(0)
//     - Signed low comparison would incorrectly give: -1 > 0 = false
//     - Unsigned low comparison correctly gives: 0xFFFFFFFFFFFFFFFF > 0 = true

#include "WAVM/Inline/I128.h"

#include <cstdint>
#include <limits> // IWYU pragma: keep
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/FloatComponents.h"
#include "WAVM/Platform/Intrinsic.h"

namespace WAVM {
	inline I128::operator U8() const
	{
		WAVM_ASSERT(!isNaN(*this) && !highI64 && lowU64 <= UINT8_MAX);
		return U8(lowU64);
	}

	inline I128::operator I8() const
	{
		WAVM_ASSERT(!isNaN(*this) && highI64 <= 0 && highI64 >= -1
					&& !(highI64 == -1 && lowI64 < INT8_MIN)
					&& !(highI64 == 0 && lowI64 > INT8_MAX));
		return I8(lowI64);
	}

	inline I128::operator U16() const
	{
		WAVM_ASSERT(!isNaN(*this) && !highI64 && lowU64 <= UINT16_MAX);
		return U16(lowU64);
	}

	inline I128::operator I16() const
	{
		WAVM_ASSERT(!isNaN(*this) && highI64 <= 0 && highI64 >= -1
					&& !(highI64 == -1 && lowI64 < INT16_MIN)
					&& !(highI64 == 0 && lowI64 > INT16_MAX));
		return I16(lowI64);
	}

	inline I128::operator U32() const
	{
		WAVM_ASSERT(!isNaN(*this) && !highI64 && lowU64 <= UINT32_MAX);
		return U32(lowU64);
	}

	inline I128::operator I32() const
	{
		WAVM_ASSERT(!isNaN(*this) && highI64 <= 0 && highI64 >= -1
					&& !(highI64 == -1 && lowI64 < INT32_MIN)
					&& !(highI64 == 0 && lowI64 > INT32_MAX));
		return I32(lowI64);
	}

	inline I128::operator U64() const
	{
		WAVM_ASSERT(!isNaN(*this) && !highI64);
		return lowU64;
	}

	inline I128::operator I64() const
	{
		WAVM_ASSERT(!isNaN(*this) && highI64 <= 0 && highI64 >= -1
					&& !(highI64 == -1 && lowI64 >= 0) && !(highI64 == 0 && lowI64 < 0));
		return lowI64;
	}

	inline I128::operator F32() const { return asFloat<F32>(); }
	inline I128::operator F64() const { return asFloat<F64>(); }

	template<typename Float> Float I128::asFloat() const
	{
		using Components = FloatComponents<Float>;
		using Bits = typename Components::Bits;

		Components components;

		if(isNaN(*this))
		{
			components.bits.sign = 0;
			components.bits.exponent = Components::maxExponentBits;
			components.bits.significand = Components::canonicalSignificand;
			return components.value;
		}

		// Determine the sign, and compute the absolute value of the I128.
		components.bits.sign = *this < 0 ? 1 : 0;
		I128 absoluteI128 = components.bits.sign ? -*this : *this;

		// Count the number of leading zeroes in the absolute I128.
		U64 leadingZeroes = countLeadingZeroes(absoluteI128.highU64);
		if(leadingZeroes == 64)
		{
			leadingZeroes += countLeadingZeroes(absoluteI128.lowU64);
			if(leadingZeroes == 128)
			{
				components.bits.exponent = 0;
				components.bits.significand = 0;
				return components.value;
			}
		}
		// leadingZeroes is in the range 0<=leadingZeroes<128.

		// Calculate the float exponent from the number of leading zeroes.
		const U64 exponent = 127 - leadingZeroes;
		components.bits.exponent = Bits(exponent + Components::exponentBias);

		// Shift the most-significant set bit to be the MSB in a U64.
		// This is normalization, not arithmetic, so we shift inline without overflow checking.
		U64 significand64;
		if(leadingZeroes == 0) { significand64 = absoluteI128.highU64; }
		else if(leadingZeroes < 64)
		{
			significand64 = (absoluteI128.highU64 << leadingZeroes)
							| (absoluteI128.lowU64 >> (64 - leadingZeroes));
		}
		else if(leadingZeroes == 64) { significand64 = absoluteI128.lowU64; }
		else
		{
			significand64 = absoluteI128.lowU64 << (leadingZeroes - 64);
		}

		// Shift the most-significant set bit down and take the significand bits below it.
		// The shift amount is 64 - (numSignificandBits + 1) to account for the hidden bit.
		constexpr Uptr shift = 63 - Components::numSignificandBits;
		Bits significand = Bits(significand64 >> shift);

		// Round-to-nearest-even: examine the truncated bits
		U64 truncatedBits = significand64 & ((U64(1) << shift) - 1);
		U64 halfPoint = U64(1) << (shift - 1);

		bool roundUp = truncatedBits > halfPoint;
		if(truncatedBits == halfPoint)
		{
			// Tie: round to even (round up if LSB of significand is 1)
			roundUp = (significand & 1) != 0;
		}

		if(roundUp)
		{
			significand++;
			// Check for overflow: significand has (numSignificandBits + 1) bits including hidden
			// bit. If it overflows to (1 << (numSignificandBits + 1)), that's 2.0 * 2^exp = 1.0 *
			// 2^(exp+1).
			if(significand == (Bits(1) << (Components::numSignificandBits + 1)))
			{
				significand = 0;
				components.bits.exponent++;
			}
		}

		components.bits.significand = significand;

		return components.value;
	}

	inline I128 operator+(I128 a) { return a; }

	inline I128 operator-(I128 a)
	{
		// This doesn't need to explicitly check for NaN, because negation preserves NaN.
		I128 result;
		result.highU64 = ~a.highU64;
		result.lowU64 = ~a.lowU64;
		result.highU64 += addAndCheckOverflow(result.lowU64, 1, &result.lowU64) ? 1 : 0;
		return result;
	}

	inline I128 abs(I128 a)
	{
		// This doesn't need to explicitly check for NaN, because negation preserves NaN.
		I128 result;
		I64 sign = a.highI64 >> 63;
		result.lowU64 = a.lowU64 ^ sign;
		result.highU64 = (a.highU64 ^ sign) + ((result.lowU64 - U64(sign) < result.lowU64) ? 1 : 0);
		result.lowU64 -= U64(sign);
		return result;
	}

	inline I128 operator~(I128 a) { return isNaN(a) ? a : I128(~a.lowU64, ~a.highI64); }

	inline bool addAndCheckOverflow(I128 a, I128 b, I128* out)
	{
		if(isNaN(a) || isNaN(b))
		{
			*out = I128::nan();
			return false;
		}

		bool overflowed = false;
		if(addAndCheckOverflow(a.highI64, b.highI64, &out->highI64)) { overflowed = true; }

		if(addAndCheckOverflow(a.lowU64, b.lowU64, &out->lowU64))
		{
			if(addAndCheckOverflow(out->highI64, 1, &out->highI64)) { overflowed = true; }
		}
		return overflowed;
	}

	inline I128 addmod127(I128 a, I128 b)
	{
		I128 result;
		if(addAndCheckOverflow(a, b, &result)) { result = -result; }
		return result;
	}

	inline I128 operator+(I128 a, I128 b)
	{
		I128 result;
		if(addAndCheckOverflow(a, b, &result)) { result = I128::nan(); }
		return result;
	}

	inline I128 operator-(I128 a, I128 b) { return a + -b; }

	inline bool mulAndCheckOverflow(I128 a, I128 b, I128* out)
	{
		if(isNaN(a) || isNaN(b))
		{
			*out = I128::nan();
			return false;
		}

		// Remove the sign from the operands.
		const I64 sign = (a.highI64 >> 63) ^ (b.highI64 >> 63);
		a = abs(a);
		b = abs(b);

		// Calculate a matrix of 64-bit products of all the 32-bit components of the operands.
		const U64 a0 = a.lowU64 & 0xffffffff;
		const U64 a1 = a.lowU64 >> 32;
		const U64 a2 = a.highU64 & 0xffffffff;
		const U64 a3 = a.highU64 >> 32;
		const U64 b0 = b.lowU64 & 0xffffffff;
		const U64 b1 = b.lowU64 >> 32;
		const U64 b2 = b.highU64 & 0xffffffff;
		const U64 b3 = b.highU64 >> 32;
		const U64 m00 = a0 * b0;
		const U64 m01 = a0 * b1;
		const U64 m02 = a0 * b2;
		const U64 m03 = a0 * b3;
		const U64 m10 = a1 * b0;
		const U64 m11 = a1 * b1;
		const U64 m12 = a1 * b2;
		const U64 m13 = a1 * b3;
		const U64 m20 = a2 * b0;
		const U64 m21 = a2 * b1;
		const U64 m22 = a2 * b2;
		const U64 m23 = a2 * b3;
		const U64 m30 = a3 * b0;
		const U64 m31 = a3 * b1;
		const U64 m32 = a3 * b2;
		const U64 m33 = a3 * b3;

		// Add all the 64-bit products together, checking for overflow.
		bool overflowed = false;
		out->lowU64 = m00;
		out->highU64 = U64(addAndCheckOverflow(out->lowU64, m01 << 32, &out->lowU64));
		out->highU64 += U64(addAndCheckOverflow(out->lowU64, m10 << 32, &out->lowU64));
		out->highU64 += (m01 >> 32) + (m10 >> 32);
		overflowed = addAndCheckOverflow(out->highU64, m02, &out->highU64) || overflowed;
		overflowed = addAndCheckOverflow(out->highU64, m11, &out->highU64) || overflowed;
		overflowed = addAndCheckOverflow(out->highU64, m20, &out->highU64) || overflowed;
		overflowed = addAndCheckOverflow(out->highU64, m03 << 32, &out->highU64) || overflowed;
		overflowed = addAndCheckOverflow(out->highU64, m12 << 32, &out->highU64) || overflowed;
		overflowed = addAndCheckOverflow(out->highU64, m21 << 32, &out->highU64) || overflowed;
		overflowed = addAndCheckOverflow(out->highU64, m30 << 32, &out->highU64) || overflowed;

		// Check if the product overflowed the 63 non-sign bits in highU64.
		overflowed = out->highI64 < 0 || overflowed;
		out->highU64 &= ~(U64(1) << 63);

		// Check if the product overflowed the I128 completely.
		overflowed = m13 || m22 || m31 || m23 || m32 || m33 || overflowed;

		// Restore the correct sign for the product.
		if(sign) { *out = -*out; }

		return overflowed;
	}

	inline I128 mulmod127(I128 a, I128 b)
	{
		I128 result;
		mulAndCheckOverflow(a, b, &result);
		return result;
	}

	inline I128 operator*(I128 a, I128 b)
	{
		I128 result;
		return mulAndCheckOverflow(a, b, &result) ? I128::nan() : result;
	}

	inline I128 operator/(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b) || b == 0) { return I128::nan(); }

		I128 remainder;
		bool negative = (a.highI64 < 0) != (b.highI64 < 0);
		a = abs(a);
		b = abs(b);
		I128 quotient = I128::udivmod(a, b, remainder);
		return negative ? -quotient : quotient;
	}

	inline I128 operator%(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b)) { return I128::nan(); }

		if(b == 0) { return I128::nan(); }

		I128 s = b.highI64 >> 63; // s = b < 0 ? -1 : 0
		b = (b ^ s) - s;          // negate if s == -1
		s = a.highI64 >> 63;      // s = a < 0 ? -1 : 0
		a = (a ^ s) - s;          // negate if s == -1
		I128 remainder;
		I128::udivmod(a, b, remainder);
		return (remainder ^ s) - s; // negate if s == -1
	}

	inline I128 operator>>(I128 a, I128 b)
	{
		if(b == 0) { return a; }
		if(isNaN(a) || isNaN(b)) { return I128::nan(); }

		I128 result;
		U64 b64 = b.lowU64;

		b64 &= 127;
		if(b64 == 0) { result = a; }
		else if(b64 & 64)
		{
			// 64 <= b < 128
			result.highI64 = a.highI64 >> 63;
			result.lowI64 = a.highI64 >> (b64 - 64);
		}
		else
		{
			// 0 <= |b| < 64
			result.highI64 = a.highI64 >> b64;
			result.lowU64 = (U64(a.highI64) << (64 - b64)) | (a.lowU64 >> b64);
		}
		return result;
	}

	inline I128 operator<<(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b)) { return I128::nan(); }

		// Shifts by negative amounts or amounts >= 127 overflow (except shifting 0)
		if(b.highI64 != 0 || b.lowU64 >= 127) { return (a == 0) ? a : I128::nan(); }

		U64 b64 = b.lowU64;
		if(b64 == 0) { return a; }

		I128 result;
		if(b64 & 64)
		{
			// 64 <= b < 127
			result.lowU64 = 0;
			result.highI64 = a.lowU64 << (b64 - 64);
		}
		else
		{
			// 0 < b < 64
			result.lowU64 = a.lowU64 << b64;
			result.highU64 = (a.highU64 << b64) | (a.lowU64 >> (64 - b64));
		}

		// Overflow if information was lost: shifting back should give original value
		if((result >> b) != a) { return I128::nan(); }

		return result;
	}

	inline I128 operator^(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b)) { return I128::nan(); }

		return I128(a.lowU64 ^ b.lowU64, a.highI64 ^ b.highI64);
	}

	inline I128 operator&(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b)) { return I128::nan(); }

		return I128(a.lowU64 & b.lowU64, a.highI64 & b.highI64);
	}

	inline I128 operator|(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b)) { return I128::nan(); }

		return I128(a.lowU64 | b.lowU64, a.highI64 | b.highI64);
	}

	inline bool operator>(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b)) { return false; }
		return a.highI64 > b.highI64 || (a.highI64 == b.highI64 && a.lowU64 > b.lowU64);
	}

	inline bool operator>=(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b)) { return isNaN(a) && isNaN(b); }
		return a.highI64 > b.highI64 || (a.highI64 == b.highI64 && a.lowU64 >= b.lowU64);
	}

	inline bool operator<(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b)) { return false; }
		return a.highI64 < b.highI64 || (a.highI64 == b.highI64 && a.lowU64 < b.lowU64);
	}

	inline bool operator<=(I128 a, I128 b)
	{
		if(isNaN(a) || isNaN(b)) { return isNaN(a) && isNaN(b); }
		return a.highI64 < b.highI64 || (a.highI64 == b.highI64 && a.lowU64 <= b.lowU64);
	}

	inline bool operator==(I128 a, I128 b)
	{
		// NaN == NaN is true (unlike IEEE floats)
		return a.highI64 == b.highI64 && a.lowU64 == b.lowU64;
	}

	inline bool operator!=(I128 a, I128 b)
	{
		return a.highI64 != b.highI64 || a.lowU64 != b.lowU64;
	}

	inline I128 I128::udivmod(I128 n, I128 d, I128& outRemainder)
	{
		if(isNaN(n) || isNaN(d)) { return I128::nan(); }

		I128 q;
		I128 r;
		U64 sr;
		// special cases, X is unknown, K != 0
		if(n.highU64 == 0)
		{
			if(d.highU64 == 0)
			{
				// 0 X
				// ---
				// 0 X
				outRemainder = n.lowU64 % d.lowU64;
				return n.lowU64 / d.lowU64;
			}
			// 0 X
			// ---
			// K X
			return {0, n.lowU64};
		}
		// n.highU64 != 0
		if(d.lowU64 == 0)
		{
			// Error if the divisor is zero.
			WAVM_ERROR_UNLESS(d.highU64 != 0);

			// d.highU64 != 0
			if(n.lowU64 == 0)
			{
				// K 0
				// ---
				// K 0
				r.highU64 = n.highU64 % d.highU64;
				r.lowU64 = 0;
				outRemainder = r;
				return n.highU64 / d.highU64;
			}
			// K K
			// ---
			// K 0
			if((d.highU64 & (d.highU64 - 1)) == 0) /* if d is a power of 2 */
			{
				r.lowU64 = n.lowU64;
				r.highU64 = n.highU64 & (d.highU64 - 1);
				outRemainder = r;
				return n.highU64 >> countTrailingZeroes(d.highU64);
			}
			// K K
			// ---
			// K 0
			sr = countLeadingZeroes(d.highU64) - countLeadingZeroes(n.highU64);
			// 0 <= sr <= 64 - 2 or sr large
			if(sr > 64 - 2)
			{
				outRemainder = n;
				return 0;
			}
			++sr;
			// 1 <= sr <= 64 - 1
			// q = n << (128 - sr);
			q.lowU64 = 0;
			q.highU64 = n.lowU64 << (64 - sr);
			// r = n >> sr;
			r.highU64 = n.highU64 >> sr;
			r.lowU64 = (n.highU64 << (64 - sr)) | (n.lowU64 >> sr);
		}
		else /* d.lowU64 != 0 */
		{
			if(d.highU64 == 0)
			{
				// K X
				// ---
				// 0 K
				if((d.lowU64 & (d.lowU64 - 1)) == 0) /* if d is a power of 2 */
				{
					r = n.lowU64 & (d.lowU64 - 1);
					if(d.lowU64 == 1)
					{
						outRemainder = r;
						return n;
					}
					sr = countTrailingZeroes(d.lowU64);
					q.highU64 = n.highU64 >> sr;
					q.lowU64 = (n.highU64 << (64 - sr)) | (n.lowU64 >> sr);
					outRemainder = r;
					return q;
				}
				// K X
				// ---
				// 0 K
				sr = 1 + 64 + countLeadingZeroes(d.lowU64) - countLeadingZeroes(n.highU64);
				// 2 <= sr <= 128 - 1
				// q = n << (128 - sr);
				// r = n >> sr;
				if(sr == 64)
				{
					q.lowU64 = 0;
					q.highU64 = n.lowU64;
					r.highU64 = 0;
					r.lowU64 = n.highU64;
				}
				else if(sr < 64) /* 2 <= sr <= 64 - 1 */
				{
					q.lowU64 = 0;
					q.highU64 = n.lowU64 << (64 - sr);
					r.highU64 = n.highU64 >> sr;
					r.lowU64 = (n.highU64 << (64 - sr)) | (n.lowU64 >> sr);
				}
				else /* 64 + 1 <= sr <= 128 - 1 */
				{
					q.lowU64 = n.lowU64 << (128 - sr);
					q.highU64 = (n.highU64 << (128 - sr)) | (n.lowU64 >> (sr - 64));
					r.highU64 = 0;
					r.lowU64 = n.highU64 >> (sr - 64);
				}
			}
			else
			{
				// K X
				// ---
				// K K
				sr = countLeadingZeroes(d.highU64) - countLeadingZeroes(n.highU64);
				// 0 <= sr <= 64 - 1 or sr large
				if(sr > 64 - 1)
				{
					outRemainder = n;
					return 0;
				}
				++sr;
				// 1 <= sr <= 64
				// q = n << (128 - sr);
				// r = n >> sr;
				q.lowU64 = 0;
				if(sr == 64)
				{
					q.highU64 = n.lowU64;
					r.highU64 = 0;
					r.lowU64 = n.highU64;
				}
				else
				{
					r.highU64 = n.highU64 >> sr;
					r.lowU64 = (n.highU64 << (64 - sr)) | (n.lowU64 >> sr);
					q.highU64 = n.lowU64 << (64 - sr);
				}
			}
		}
		// Not a special case
		// q and r are initialized with:
		// q = n << (128 - sr);
		// r = n >> sr;
		// 1 <= sr <= 128 - 1
		U64 carry = 0;
		for(; sr > 0; --sr)
		{
			// r:q = ((r:q)  << 1) | carry
			r.highU64 = (r.highU64 << 1) | (r.lowU64 >> (64 - 1));
			r.lowU64 = (r.lowU64 << 1) | (q.highU64 >> (64 - 1));
			q.highU64 = (q.highU64 << 1) | (q.lowU64 >> (64 - 1));
			q.lowU64 = (q.lowU64 << 1) | carry;
			// carry = 0;
			// if (r >= d)
			// {
			//     r -= d;
			//      carry = 1;
			// }
			const I128 s = (d - r - 1).highI64 >> 63;
			carry = s.lowU64 & 1;
			r -= d & s;
		}
		q = (q << 1) | carry;
		outRemainder = r;
		return q;
	}

	inline ToCharsResult toChars(char* outBegin, char* outEnd, I128 value)
	{
		if(outBegin >= outEnd) { return {outEnd, true}; }

		char* ptr = outBegin;
		if(isNaN(value))
		{
			if(outEnd - ptr < 1) { return {ptr, true}; }
			*ptr++ = 'N';
			if(outEnd - ptr < 1) { return {ptr, true}; }
			*ptr++ = 'a';
			if(outEnd - ptr < 1) { return {ptr, true}; }
			*ptr++ = 'N';
			return {ptr, false};
		}

		const bool negative = value < 0;
		if(negative)
		{
			if(ptr >= outEnd) { return {outEnd, true}; }
			*ptr++ = '-';
			value = -value;
		}

		I128 zero(0);
		if(value == zero)
		{
			if(ptr >= outEnd) { return {outEnd, true}; }
			*ptr++ = '0';
			return {ptr, false};
		}

		// Collect digits in reverse order
		char* digitStart = ptr;
		while(value != zero)
		{
			if(ptr >= outEnd) { return {outEnd, true}; }
			I128 digit = value % I128(10);
			*ptr++ = '0' + char(U64(digit));
			value = value / I128(10);
		}

		// Reverse the digits
		char* digitEnd = ptr - 1;
		while(digitStart < digitEnd)
		{
			char tmp = *digitStart;
			*digitStart = *digitEnd;
			*digitEnd = tmp;
			++digitStart;
			--digitEnd;
		}

		return {ptr, false};
	}

	template<typename Int, typename> Int clampedCast(I128 value)
	{
		WAVM_ASSERT(!isNaN(value));
		if(value < std::numeric_limits<Int>::min()) { return std::numeric_limits<Int>::min(); }
		if(value > std::numeric_limits<Int>::max()) { return std::numeric_limits<Int>::max(); }
		return Int(value);
	}
}

#endif // WAVM_INLINE_IMPL_I128IMPL_H
