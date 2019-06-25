// IWYU pragma: private, include "Inline/I128.h"
// You should only include this file indirectly by including HashMap.h.
#pragma once

#include "WAVM/Inline/FloatComponents.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Platform/Intrinsic.h"

namespace WAVM {
	inline I128::operator U32() const
	{
		errorUnless(!overflow);
		errorUnless(!highI64 && lowU64 <= UINT32_MAX);
		return U32(lowU64);
	}

	inline I128::operator I32() const
	{
		errorUnless(!overflow);
		if(highI64 == 0)
		{
			errorUnless(lowU64 <= INT32_MAX);
			return I32(lowI64);
		}
		else
		{
			errorUnless(highI64 == -1 && lowI64 >= INT32_MIN);
			return I32(lowI64);
		}
	}

	inline I128::operator U64() const
	{
		errorUnless(!overflow);
		errorUnless(!highU64);
		return lowU64;
	}

	inline I128::operator I64() const
	{
		errorUnless(!overflow);
		if(highI64 == 0)
		{
			errorUnless(lowU64 <= INT64_MAX);
			return lowI64;
		}
		else
		{
			errorUnless(highI64 == -1);
			return lowI64;
		}
	}

	inline I128::operator F32() const { return F32(F64(*this)); }

	inline I128::operator F64() const
	{
		errorUnless(!overflow);

		FloatComponents<F64> f64Components;

		// Determine the sign, and compute the absolute value of the I128.
		f64Components.bits.sign = *this < 0 ? 1 : 0;
		I128 absoluteI128 = f64Components.bits.sign ? -*this : *this;

		// Count the number of leading zeroes in the absolute I128.
		U64 leadingZeroes = Platform::countLeadingZeroes(absoluteI128.highU64);
		if(leadingZeroes == 64)
		{
			leadingZeroes += Platform::countLeadingZeroes(absoluteI128.lowU64);
			if(leadingZeroes == 128)
			{
				f64Components.bits.exponent = 0;
				f64Components.bits.significand = 0;
				return f64Components.value;
			}
		}
		// leadingZeroes is in the range 0<=leadingZeroes<128.

		// Calculate the F64 exponent from the number of leading zeroes.
		const U64 exponent = 127 - leadingZeroes;
		f64Components.bits.exponent = exponent + FloatComponents<F64>::exponentBias;

		// Shift the most-significant set bit to be the MSB in a U64.
		const U64 significand64 = (absoluteI128 << leadingZeroes).highU64;

		// Shift the most-significant set bit down to the 53rd bit, and take the 52 bits below it as
		// the significand.
		f64Components.bits.significand = significand64 >> 11;

		return f64Components.value;
	}

	inline I128 operator+(I128 a) { return a; }

	inline I128 operator-(I128 a)
	{
		I128 result;
		result.highU64 = ~a.highU64;
		result.lowU64 = ~a.lowU64;
		result += 1;
		return result;
	}

	inline I128 operator~(I128 a)
	{
		I128 result;
		result.highU64 = ~a.highU64;
		result.lowU64 = ~a.lowU64;
		return result;
	}

	inline I128 operator+(I128 a, I128 b)
	{
		I128 result;
		result.lowU64 = a.lowU64 + b.lowU64;
		result.highU64 = a.highU64 + b.highU64;
		result.overflow = a.overflow || b.overflow;
		if(result.lowU64 < a.lowU64)
		{
			if(result.highI64 == INT64_MAX) { result.overflow = true; }
			++result.highI64;
		}
		return result;
	}

	inline I128 operator-(I128 a, I128 b)
	{
		b = -b;
		return a + b;
	}

	inline I128 operator*(I128 a, I128 b)
	{
		I128 result;

		const U64 m00 = (a.lowU64 & 0xffffffff) * (b.lowU64 & 0xffffffff);
		const U64 m01 = (a.lowU64 & 0xffffffff) * (b.lowU64 >> 32);
		const U64 m10 = (a.lowU64 >> 32) * (b.lowU64 & 0xffffffff);
		const U64 m11 = (a.lowU64 >> 32) * (b.lowU64 >> 32);

		const U64 t1 = (m00 >> 32) + m10;
		const U64 t2 = (t1 & 0xffffffff) + m01;
		const U64 t3 = m11 + (t1 >> 32) + (t2 >> 32);

		result.lowU64 = (m00 & 0xffffffff) + (t2 << 32);
		result.highI64 = t3 + a.highI64 * b.lowU64 + a.lowU64 * b.highI64;

		result.overflow = a.overflow || b.overflow;
		if(!result.overflow)
		{
			if(a == INT128_MIN)
			{
				if(b != 0 && b != 1) { result.overflow = true; }
			}
			else if(b == INT128_MIN)
			{
				if(a != 0 && a != 1) { result.overflow = true; }
			}
			else
			{
				I128 sa = a >> 127;
				I128 abs_a = (a ^ sa) - sa;
				I128 sb = b >> 127;
				I128 abs_b = (b ^ sb) - sb;
				if(abs_a >= 2 && abs_b >= 2
				   && (sa == sb ? (abs_a > INT128_MAX / abs_b) : (abs_a > INT128_MIN / -abs_b)))
				{ result.overflow = true; }
			}
		}

		return result;
	}

	inline I128 operator/(I128 a, I128 b)
	{
		I128 remainder;
		I128 s_a = a >> 127;                                 // s_a = a < 0 ? -1 : 0
		I128 s_b = b >> 127;                                 // s_b = b < 0 ? -1 : 0
		a = (a ^ s_a) - s_a;                                 // negate if s_a == -1
		b = (b ^ s_b) - s_b;                                 // negate if s_b == -1
		s_a ^= s_b;                                          // sign of quotient
		return (I128::udivmod(a, b, remainder) ^ s_a) - s_a; // negate if s_a == -1
	}

	inline I128 operator%(I128 a, I128 b)
	{
		I128 s = b >> 127; // s = b < 0 ? -1 : 0
		b = (b ^ s) - s;   // negate if s == -1
		s = a >> 127;      // s = a < 0 ? -1 : 0
		a = (a ^ s) - s;   // negate if s == -1
		I128 remainder;
		I128::udivmod(a, b, remainder);
		return (remainder ^ s) - s; // negate if s == -1
	}

	inline I128 operator>>(I128 a, I128 b)
	{
		I128 result;
		result.overflow = a.overflow || b.overflow;

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
			result.lowU64 = (a.highI64 << (64 - b64)) | (a.lowU64 >> b64);
		}
		return result;
	}

	inline I128 operator<<(I128 a, I128 b)
	{
		I128 result;
		result.overflow = a.overflow || b.overflow;

		U64 b64 = b.lowU64;

		b64 &= 127;
		if(b64 == 0) { result = a; }
		else if(b64 & 64)
		{
			// 64 <= b < 128
			result.lowU64 = 0;
			result.highI64 = a.lowU64 << (b64 - 64);
		}
		else
		{
			// 0 <= |b| < 64
			result.lowU64 = a.lowU64 << b64;
			result.highU64 = (a.highU64 << b64) | (a.lowU64 >> (64 - b64));
		}
		return result;
	}

	inline I128 operator^(I128 a, I128 b)
	{
		return I128(a.lowU64 ^ b.lowU64,
					a.highI64 ^ b.highI64,
					(a.overflow && !b.overflow) || (!a.overflow && b.overflow));
	}

	inline I128 operator&(I128 a, I128 b)
	{
		return I128(a.lowU64 & b.lowU64, a.highI64 & b.highI64, a.overflow && b.overflow);
	}

	inline I128 operator|(I128 a, I128 b)
	{
		return I128(a.lowU64 | b.lowU64, a.highI64 | b.highI64, a.overflow || b.overflow);
	}

	inline bool operator>(I128 a, I128 b)
	{
		errorUnless(!a.overflow && !b.overflow);
		return a.highI64 > b.highI64 || (a.highI64 == b.highI64 && a.lowI64 > b.lowI64);
	}

	inline bool operator>=(I128 a, I128 b)
	{
		errorUnless(!a.overflow && !b.overflow);
		return a.highI64 > b.highI64 || (a.highI64 == b.highI64 && a.lowI64 >= b.lowI64);
	}

	inline bool operator<(I128 a, I128 b)
	{
		errorUnless(!a.overflow && !b.overflow);
		return a.highI64 < b.highI64 || (a.highI64 == b.highI64 && a.lowI64 < b.lowI64);
	}

	inline bool operator<=(I128 a, I128 b)
	{
		errorUnless(!a.overflow && !b.overflow);
		return a.highI64 < b.highI64 || (a.highI64 == b.highI64 && a.lowI64 <= b.lowI64);
	}

	inline bool operator==(I128 a, I128 b)
	{
		errorUnless(!a.overflow && !b.overflow);
		return a.highI64 == b.highI64 && a.lowU64 == b.lowU64;
	}

	inline bool operator!=(I128 a, I128 b)
	{
		errorUnless(!a.overflow && !b.overflow);
		return a.highI64 != b.highI64 || a.lowU64 == b.lowU64;
	}

	inline I128& I128::operator=(I128 copyee)
	{
		lowU64 = copyee.lowU64;
		highU64 = copyee.highU64;
		overflow = copyee.overflow;
		return *this;
	}

	inline I128 I128::udivmod(I128 a, I128 b, I128& outRemainder)
	{
		I128 n = a;
		I128 d = b;
		I128 q;
		I128 r;
		U64 sr;
		r.overflow = n.overflow || d.overflow;
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
			errorUnless(d.highU64 != 0);

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
				return n.highU64 >> Platform::countTrailingZeroes(d.highU64);
			}
			// K K
			// ---
			// K 0
			sr = Platform::countLeadingZeroes(d.highU64) - Platform::countLeadingZeroes(n.highU64);
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
					sr = Platform::countTrailingZeroes(d.lowU64);
					q.highU64 = n.highU64 >> sr;
					q.lowU64 = (n.highU64 << (64 - sr)) | (n.lowU64 >> sr);
					outRemainder = r;
					return q;
				}
				// K X
				// ---
				// 0 K
				sr = 1 + 64 + Platform::countLeadingZeroes(d.lowU64)
					 - Platform::countLeadingZeroes(n.highU64);
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
				sr = Platform::countLeadingZeroes(d.highU64)
					 - Platform::countLeadingZeroes(n.highU64);
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
			const I128 s = (d - r - 1) >> 127;
			carry = s.lowU64 & 1;
			r -= d & s;
		}
		q = (q << 1) | carry;
		outRemainder = r;
		return q;
	}
}
