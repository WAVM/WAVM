#include <cstdint>
#include "TestUtils.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Inline/Timing.h"
#include "wavm-test.h"

using namespace WAVM;
using namespace WAVM::Testing;

struct RandomStream
{
	RandomStream(I128 inSeed) : seed(inSeed) {}

	I128 get()
	{
		seed = addmod127(mulmod127(seed, U64(6364136223846793005)), U64(1442695040888963407));
		return seed;
	}

private:
	I128 seed;
};

// I128-specific check macros using the generic check() function

#define CHECK_NAN(expr)                                                                            \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](I128 v) { return isNaN(v); },                                                           \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_NAN(" #expr ")",                                                                    \
		"  actual: %s\n",                                                                          \
		expr)

#define CHECK_NOT_NAN(expr)                                                                        \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](I128 v) { return !isNaN(v); },                                                          \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_NOT_NAN(" #expr ")",                                                                \
		"  actual: %s\n",                                                                          \
		expr)

#define CHECK_EQ_OR_NAN(actual, expected)                                                          \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](I128 a, I128 e) { return isNaN(a) || isNaN(e) || a == e; },                             \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_EQ_OR_NAN(" #actual ", " #expected ")",                                             \
		"  actual:   %s\n  expected: %s\n",                                                        \
		actual,                                                                                    \
		expected)

static void testNaNHandling(TEST_STATE_PARAM)
{
	// NaN propagation through arithmetic
	CHECK_NAN(I128::nan());
	CHECK_NAN(I128::nan() + I128(1));
	CHECK_NAN(I128(1) + I128::nan());
	CHECK_NAN(I128::nan() - I128(1));
	CHECK_NAN(I128(1) - I128::nan());
	CHECK_NAN(I128::nan() * I128(2));
	CHECK_NAN(I128(2) * I128::nan());
	CHECK_NAN(I128::nan() / I128(2));
	CHECK_NAN(I128(2) / I128::nan());
	CHECK_NAN(I128::nan() % I128(2));
	CHECK_NAN(I128(2) % I128::nan());

	// NaN propagation through shifts
	CHECK_NAN(I128::nan() >> I128(1));
	CHECK_NAN(I128(1) >> I128::nan());
	CHECK_NAN(I128::nan() << I128(1));
	CHECK_NAN(I128(1) << I128::nan());

	// NaN propagation through bitwise ops
	CHECK_NAN(I128::nan() & I128(1));
	CHECK_NAN(I128(1) & I128::nan());
	CHECK_NAN(I128::nan() | I128(1));
	CHECK_NAN(I128(1) | I128::nan());
	CHECK_NAN(I128::nan() ^ I128(1));
	CHECK_NAN(I128(1) ^ I128::nan());
	CHECK_NAN(~I128::nan());

	// NaN equality: NaN == NaN is true (unlike IEEE floats)
	CHECK_TRUE(I128::nan() == I128::nan());
	CHECK_FALSE(I128::nan() != I128::nan());
	CHECK_FALSE(I128::nan() == I128(0));
	CHECK_NE(I128::nan(), I128(0));

	// Ordered comparisons with NaN: NaN is unordered, so < and > return false
	CHECK_NOT_LT(I128::nan(), I128(0));
	CHECK_NOT_LT(I128(0), I128::nan());
	CHECK_NOT_GT(I128::nan(), I128(0));
	CHECK_NOT_GT(I128(0), I128::nan());
	CHECK_NOT_LT(I128::nan(), I128::nan());
	CHECK_NOT_GT(I128::nan(), I128::nan());

	// But NaN >= NaN and NaN <= NaN are true (since NaN == NaN)
	CHECK_GE(I128::nan(), I128::nan());
	CHECK_LE(I128::nan(), I128::nan());
	CHECK_NOT_LE(I128::nan(), I128(0));
	CHECK_NOT_LE(I128(0), I128::nan());
	CHECK_NOT_GE(I128::nan(), I128(0));
	CHECK_NOT_GE(I128(0), I128::nan());

	// NaN flush functions
	CHECK_EQ(flushNaNToZero(I128::nan()), I128(0));
	CHECK_EQ(flushNaNToMax(I128::nan()), I128::max());
	CHECK_EQ(flushNaNToMin(I128::nan()), I128::min());
	CHECK_EQ(flushNaNToZero(I128(42)), I128(42));
	CHECK_EQ(flushNaNToMax(I128(42)), I128(42));
	CHECK_EQ(flushNaNToMin(I128(42)), I128(42));
}

static void testOverflow(TEST_STATE_PARAM)
{
	// Addition overflow produces NaN
	CHECK_NAN(I128::max() + I128(1));
	CHECK_NAN(I128::min() - I128(1));

	// Multiplication overflow produces NaN
	CHECK_NAN(I128::max() * I128(2));
	CHECK_NAN(I128::min() * I128(2));

	// Multiplication at boundaries that doesn't overflow
	CHECK_EQ(I128::min() * I128(1), I128::min());
	CHECK_EQ(I128(1) * I128::min(), I128::min());
	CHECK_EQ(I128::max() * I128(-1), I128::min());
	CHECK_EQ(I128(-1) * I128::max(), I128::min());
	CHECK_EQ(I128::min() * I128(-1), I128::max());
	CHECK_EQ(I128(-1) * I128::min(), I128::max());

	// addAndCheckOverflow
	I128 result;
	CHECK_TRUE(addAndCheckOverflow(I128::max(), I128(1), &result) == true);
	CHECK_TRUE(addAndCheckOverflow(I128(100), I128(200), &result) == false);
	CHECK_EQ(result, I128(300));

	// mulAndCheckOverflow: overflow cases
	CHECK_TRUE(mulAndCheckOverflow(I128::max(), I128(2), &result) == true);

	// mulAndCheckOverflow: non-overflow cases
	CHECK_TRUE(mulAndCheckOverflow(I128(100), I128(200), &result) == false);
	CHECK_EQ(result, I128(20000));

	CHECK_FALSE(mulAndCheckOverflow(I128::min(), I128(1), &result));
	CHECK_EQ(result, I128::min());

	CHECK_FALSE(mulAndCheckOverflow(I128::max(), I128(-1), &result));
	CHECK_EQ(result, I128::min());

	CHECK_FALSE(mulAndCheckOverflow(I128::min(), I128(-1), &result));
	CHECK_EQ(result, I128::max());

	// mulmod127/addmod127 don't produce NaN on overflow - they wrap
	CHECK_NOT_NAN(addmod127(I128::max(), I128(100)));
	CHECK_NOT_NAN(mulmod127(I128::max(), I128(2)));
	CHECK_EQ(mulmod127(I128::min(), I128(1)), I128::min());
	CHECK_EQ(mulmod127(I128::max(), I128(-1)), I128::min());
}

static void testDivision(TEST_STATE_PARAM)
{
	// Division by zero produces NaN
	CHECK_NAN(I128(100) / I128(0));
	CHECK_NAN(I128(100) % I128(0));
	CHECK_NAN(I128(0) / I128(0));

	// Basic division
	CHECK_EQ(I128(100) / I128(10), I128(10));
	CHECK_EQ(I128(100) % I128(10), I128(0));
	CHECK_EQ(I128(100) / I128(3), I128(33));
	CHECK_EQ(I128(100) % I128(3), I128(1));

	// Division of negative numbers
	CHECK_EQ(I128(-100) / I128(3), I128(-33));
	CHECK_EQ(I128(-100) % I128(3), I128(-1));
	CHECK_EQ(I128(100) / I128(-3), I128(-33));
	CHECK_EQ(I128(100) % I128(-3), I128(1));
	CHECK_EQ(I128(-100) / I128(-3), I128(33));
	CHECK_EQ(I128(-100) % I128(-3), I128(-1));

	// Division with large values
	CHECK_EQ(I128::max() / I128::max(), I128(1));
	CHECK_EQ(I128::max() % I128::max(), I128(0));
	CHECK_EQ(I128::max() / I128(1), I128::max());
	CHECK_EQ(I128::min() / I128(1), I128::min());
	CHECK_EQ(I128::min() / I128(-1), I128::max());
	CHECK_EQ(I128::max() / I128(-1), I128::min());

	// Modulo with boundary values
	CHECK_EQ(I128::min() % I128(1), I128(0));
	CHECK_EQ(I128::min() % I128(-1), I128(0));
	CHECK_EQ(I128::min() % I128(2), I128(-1));
	CHECK_EQ(I128::min() % I128(-2), I128(-1));
	CHECK_EQ(I128::max() % I128(1), I128(0));
	CHECK_EQ(I128::max() % I128(-1), I128(0));

	// Division where quotient is 0
	CHECK_EQ(I128(5) / I128(10), I128(0));
	CHECK_EQ(I128(5) % I128(10), I128(5));
}

static void testShifts(TEST_STATE_PARAM)
{
	// Right shift
	CHECK_EQ(I128(0x100) >> I128(4), I128(0x10));
	CHECK_EQ(I128(0x100) >> I128(0), I128(0x100));
	CHECK_EQ(I128(-128) >> I128(1), I128(-64)); // arithmetic shift preserves sign

	// Left shift
	CHECK_EQ(I128(1) << I128(0), I128(1));
	CHECK_EQ(I128(1) << I128(4), I128(16));
	CHECK_EQ(I128(1) << I128(63), I128(U64(1) << 63));

	// Shift crossing word boundary
	I128 shifted = I128(1) << I128(64);
	CHECK_EQ(shifted >> I128(64), I128(1));

	// 1 << 64 should be 2^64, which fits in I128
	I128 twoTo64 = I128(1) << I128(64);
	CHECK_GT(twoTo64, I128(U64(0xFFFFFFFFFFFFFFFF))); // 2^64 > 2^64-1

	// 1 << 126 should be 2^126, which fits in I128 (max is ~2^127)
	I128 twoTo126 = I128(1) << I128(126);
	CHECK_GT(twoTo126, I128(0));

	// Shift by 127 should overflow (result would be 2^127 which is > max)
	CHECK_NAN(I128(1) << I128(127));

	// 2^63 << 64 = 2^127 > max, should overflow
	CHECK_NAN(I128(U64(1) << 63) << I128(64));

	// (2^64-1) << 64 is much larger than max, should overflow
	CHECK_NAN(I128(U64(0xFFFFFFFFFFFFFFFF)) << I128(64));
}

static void testBitwise(TEST_STATE_PARAM)
{
	I128 a(U64(0x123456789ABCDEF0));
	I128 allOnes(-1);

	// XOR properties
	CHECK_EQ(a ^ a, I128(0));
	CHECK_EQ(a ^ I128(0), a);
	CHECK_EQ(a ^ allOnes, ~a);

	// AND properties
	CHECK_EQ(a & a, a);
	CHECK_EQ(a & I128(0), I128(0));
	CHECK_EQ(a & allOnes, a);

	// OR properties
	CHECK_EQ(a | a, a);
	CHECK_EQ(a | I128(0), a);
	CHECK_EQ(a | allOnes, allOnes);

	// NOT
	CHECK_EQ(~~a, a);
	CHECK_EQ(~I128(0), allOnes);
}

static void testComparisons(TEST_STATE_PARAM)
{
	// Basic comparisons
	CHECK_GT(I128(100), I128(50));
	CHECK_NOT_GT(I128(50), I128(100));
	CHECK_NOT_GT(I128(100), I128(100));

	CHECK_GE(I128(100), I128(50));
	CHECK_GE(I128(100), I128(100));
	CHECK_NOT_GE(I128(50), I128(100));

	CHECK_LT(I128(50), I128(100));
	CHECK_NOT_LT(I128(100), I128(50));
	CHECK_NOT_LT(I128(100), I128(100));

	CHECK_LE(I128(50), I128(100));
	CHECK_LE(I128(100), I128(100));
	CHECK_NOT_LE(I128(100), I128(50));

	CHECK_EQ(I128(100), I128(100));
	CHECK_FALSE(I128(100) == I128(50));

	CHECK_NE(I128(100), I128(50));
	CHECK_FALSE(I128(100) != I128(100));

	// Negative number comparisons
	CHECK_LT(I128(-100), I128(100));
	CHECK_LT(I128(-100), I128(-50));
	CHECK_GT(I128(-50), I128(-100));
	CHECK_LT(I128(-100), I128(0));
	CHECK_GT(I128(0), I128(-100));

	// Boundary comparisons
	CHECK_GT(I128::max(), I128::min());
	CHECK_LT(I128::min(), I128(0));
	CHECK_GT(I128::max(), I128(0));
	CHECK_GT(I128(0), I128::min());
	CHECK_LT(I128(0), I128::max());

	// Verify that the low word is compared as unsigned, not signed.
	// When high words are equal and non-negative, values with low word bit 63 set should
	// compare greater than values without it set.
	CHECK_GT(I128(U64(0x8000000000000000)), I128(0));
	CHECK_LT(I128(0), I128(U64(0x8000000000000000)));
	CHECK_GE(I128(U64(0x8000000000000000)), I128(0));
	CHECK_LE(I128(0), I128(U64(0x8000000000000000)));
	CHECK_GT(I128(U64(0xFFFFFFFFFFFFFFFF)), I128(0));
	CHECK_LT(I128(0), I128(U64(0xFFFFFFFFFFFFFFFF)));
	CHECK_GT(I128(U64(0xFFFFFFFFFFFFFFFF)), I128(U64(0x8000000000000000)));
	CHECK_LT(I128(U64(0x8000000000000000)), I128(U64(0xFFFFFFFFFFFFFFFF)));
	CHECK_GT(I128(U64(0xFFFFFFFFFFFFFFFF)), I128(U64(0x7FFFFFFFFFFFFFFF)));
	CHECK_LT(I128(U64(0x7FFFFFFFFFFFFFFF)), I128(U64(0xFFFFFFFFFFFFFFFF)));
	CHECK_GT(I128(U64(0x8000000000000000)), I128(U64(0x7FFFFFFFFFFFFFFF)));
	CHECK_LT(I128(U64(0x7FFFFFFFFFFFFFFF)), I128(U64(0x8000000000000000)));
	CHECK_EQ(I128(U64(0x8000000000000000)), I128(U64(0x8000000000000000)));
	CHECK_EQ(I128(U64(0xFFFFFFFFFFFFFFFF)), I128(U64(0xFFFFFFFFFFFFFFFF)));
}

static void testUnaryOps(TEST_STATE_PARAM)
{
	// Unary plus
	CHECK_EQ(+I128(42), I128(42));
	CHECK_EQ(+I128(-42), I128(-42));
	CHECK_EQ(+I128(0), I128(0));

	// Negation
	CHECK_EQ(-I128(42), I128(-42));
	CHECK_EQ(-I128(-42), I128(42));
	CHECK_EQ(-I128(0), I128(0));

	// abs()
	CHECK_EQ(abs(I128(42)), I128(42));
	CHECK_EQ(abs(I128(-42)), I128(42));
	CHECK_EQ(abs(I128(0)), I128(0));
	CHECK_EQ(abs(I128::max()), I128::max());
}

static void testConstructorsAndConversions(TEST_STATE_PARAM)
{
	// Constructors from signed types sign-extend
	CHECK_EQ(I128(I32(-1)), I128(-1));
	CHECK_EQ(I128(I64(-1)), I128(-1));

	// Constructors from unsigned types zero-extend
	CHECK_EQ(I128(U32(0xFFFFFFFF)), (I128(1) << 32) - 1);
	CHECK_EQ(I128(U64(0xFFFFFFFFFFFFFFFF)), (I128(1) << 64) - 1);

	// Unsigned values are positive
	CHECK_GT(I128(U32(0xFFFFFFFF)), I128(0));
}

static void testFloatConversions(TEST_STATE_PARAM)
{
	// F64 has 53-bit mantissa, F32 has 24-bit mantissa

	// Zero
	CHECK_EQ(F64(I128(0)), 0.0);
	CHECK_EQ(F32(I128(0)), 0.0f);

	// Small positive integers (exact representation)
	CHECK_EQ(F64(I128(1)), 1.0);
	CHECK_EQ(F64(I128(2)), 2.0);
	CHECK_EQ(F64(I128(100)), 100.0);
	CHECK_EQ(F64(I128(1000000)), 1000000.0);
	CHECK_EQ(F32(I128(1)), 1.0f);
	CHECK_EQ(F32(I128(100)), 100.0f);

	// Small negative integers (exact representation)
	CHECK_EQ(F64(I128(-1)), -1.0);
	CHECK_EQ(F64(I128(-2)), -2.0);
	CHECK_EQ(F64(I128(-100)), -100.0);
	CHECK_EQ(F64(I128(-1000000)), -1000000.0);
	CHECK_EQ(F32(I128(-1)), -1.0f);
	CHECK_EQ(F32(I128(-100)), -100.0f);

	// Powers of 2 (exact representation)
	// These test different code paths based on leadingZeroes
	CHECK_EQ(F64(I128(1) << I128(10)), 1024.0);                 // 2^10
	CHECK_EQ(F64(I128(1) << I128(20)), 1048576.0);              // 2^20
	CHECK_EQ(F64(I128(1) << I128(30)), 1073741824.0);           // 2^30
	CHECK_EQ(F64(I128(1) << I128(52)), 4503599627370496.0);     // 2^52
	CHECK_EQ(F64(I128(1) << I128(53)), 9007199254740992.0);     // 2^53
	CHECK_EQ(F64(I128(1) << I128(62)), 4611686018427387904.0);  // 2^62
	CHECK_EQ(F64(I128(1) << I128(63)), 9223372036854775808.0);  // 2^63
	CHECK_EQ(F64(I128(1) << I128(64)), 18446744073709551616.0); // 2^64 (crosses word boundary)
	CHECK_EQ(F64(I128(1) << I128(100)), 1267650600228229401496703205376.0); // 2^100

	// Negative powers of 2
	CHECK_EQ(F64(-(I128(1) << I128(10))), -1024.0);
	CHECK_EQ(F64(-(I128(1) << I128(63))), -9223372036854775808.0);
	CHECK_EQ(F64(-(I128(1) << I128(64))), -18446744073709551616.0);

	// F32 powers of 2
	CHECK_EQ(F32(I128(1) << I128(10)), 1024.0f);
	CHECK_EQ(F32(I128(1) << I128(23)), 8388608.0f);  // 2^23
	CHECK_EQ(F32(I128(1) << I128(24)), 16777216.0f); // 2^24
	CHECK_EQ(F32(I128(1) << I128(64)), 18446744073709551616.0f);

	// Large values (will lose precision but should be approximately correct)
	// max is approximately 1.7e38
	F64 maxF64 = F64(I128::max());
	CHECK_GT(maxF64, 1.7e38);
	CHECK_LT(maxF64, 1.8e38);

	F64 minF64 = F64(I128::min());
	CHECK_LT(minF64, -1.7e38);
	CHECK_GT(minF64, -1.8e38);

	// Verify max and min are negatives of each other (symmetric range)
	CHECK_EQ(maxF64, -minF64);

	// F32 large values
	F32 maxF32 = F32(I128::max());
	CHECK_GT(maxF32, 1.7e38f);
	CHECK_LT(maxF32, 1.8e38f);

	// NaN converts to float NaN
	F64 nanF64 = F64(I128::nan());
	CHECK_TRUE(nanF64 != nanF64); // NaN != NaN

	F32 nanF32 = F32(I128::nan());
	CHECK_TRUE(nanF32 != nanF32); // NaN != NaN

	// Values that test different branches in asFloat():
	// leadingZeroes == 0: value with MSB set in highU64 (large positive)
	I128 largeVal = I128(1) << I128(126);
	CHECK_GT(F64(largeVal), 0.0);

	// leadingZeroes < 64: value with some bits in highU64
	I128 mediumVal = I128(1) << I128(70);
	CHECK_EQ(F64(mediumVal), 1180591620717411303424.0); // 2^70

	// leadingZeroes == 64: value exactly filling lowU64
	I128 lowFullVal = I128(U64(0x8000000000000000)); // 2^63
	CHECK_EQ(F64(lowFullVal), 9223372036854775808.0);

	// leadingZeroes > 64: small value only in lower bits of lowU64
	I128 smallVal = I128(12345);
	CHECK_EQ(F64(smallVal), 12345.0);

	// Round-to-nearest-even edge cases for F64 (53-bit mantissa including hidden bit)
	// The LSB of the representable value corresponds to 2^(exponent - 52)

	// 2^53 is exactly representable
	I128 twoTo53 = I128(1) << I128(53);
	CHECK_EQ(F64(twoTo53), 9007199254740992.0);

	// 2^53 + 1: truncated bits = 1, which is < half of 2, so round down to 2^53
	CHECK_EQ(F64(twoTo53 + I128(1)), 9007199254740992.0);

	// 2^53 + 2: exactly representable (LSB = 2 at this scale)
	CHECK_EQ(F64(twoTo53 + I128(2)), 9007199254740994.0);

	// 2^53 + 3: truncated bits = 1, exactly half. Tie breaks to even (2^53 + 4 has even sig)
	CHECK_EQ(F64(twoTo53 + I128(3)), 9007199254740996.0);

	// 2^54 + 2: truncated bits = 2, which is exactly half of 4 (the LSB at this scale)
	// Result significand LSB is 0, so round down (to even)
	I128 twoTo54 = I128(1) << I128(54);
	CHECK_EQ(F64(twoTo54 + I128(2)), 18014398509481984.0); // 2^54

	// 2^54 + 6: truncated bits = 2, exactly half, result LSB would be 1, round up (to even)
	CHECK_EQ(F64(twoTo54 + I128(6)), 18014398509481992.0); // 2^54 + 8

	// 2^54 + 3: truncated bits = 3, which is > half of 4, round up
	CHECK_EQ(F64(twoTo54 + I128(3)), 18014398509481988.0); // 2^54 + 4

	// 2^54 + 5: truncated bits = 1, which is < half of 4, round down
	CHECK_EQ(F64(twoTo54 + I128(5)), 18014398509481988.0); // 2^54 + 4

	// 2^54 - 1: truncated bits exactly half. Tie breaks to even (2^54 has even sig=0)
	CHECK_EQ(F64(twoTo54 - I128(1)), 18014398509481984.0); // 2^54

	// INT64_MAX = 2^63 - 1: truncated bits are all 1s (> half), rounds up to 2^63
	CHECK_EQ(F64(I128(I64(INT64_MAX))), 9223372036854775808.0); // 2^63

	// UINT64_MAX = 2^64 - 1: truncated bits are all 1s (> half), rounds up to 2^64
	CHECK_EQ(F64(I128(U64(UINT64_MAX))), 18446744073709551616.0); // 2^64

	// Negative values should round symmetrically
	CHECK_EQ(F64(-twoTo53 - I128(1)), -9007199254740992.0);
	CHECK_EQ(F64(-twoTo54 - I128(3)), -18014398509481988.0);
}

static void testBoundaryValues(TEST_STATE_PARAM)
{
	// min/max have correct signs (implicitly checks they're not NaN)
	CHECK_GT(I128::max(), I128(0));
	CHECK_LT(I128::min(), I128(0));

	// Relationship between min and max (symmetric range: min = -max)
	CHECK_EQ(I128::max() + I128::min(), I128(0));
	CHECK_EQ(-I128::max(), I128::min());
	CHECK_EQ(-I128::min(), I128::max());

	// Operations at boundaries that don't overflow
	CHECK_EQ(I128::max() - I128::max(), I128(0));
	CHECK_EQ(I128::min() - I128::min(), I128(0));
	CHECK_EQ(I128::max() + I128::min(), I128(0));
	CHECK_EQ(I128::min() + I128::max(), I128(0));
}

static void testAssignmentOps(TEST_STATE_PARAM)
{
	I128 a;

	a = I128(100);
	a += I128(50);
	CHECK_EQ(a, I128(150));

	a = I128(100);
	a -= I128(50);
	CHECK_EQ(a, I128(50));

	a = I128(10);
	a *= I128(5);
	CHECK_EQ(a, I128(50));

	a = I128(100);
	a /= I128(5);
	CHECK_EQ(a, I128(20));

	a = I128(100);
	a %= I128(30);
	CHECK_EQ(a, I128(10));

	a = I128(0xFF);
	a &= I128(0x0F);
	CHECK_EQ(a, I128(0x0F));

	a = I128(0xF0);
	a |= I128(0x0F);
	CHECK_EQ(a, I128(0xFF));

	a = I128(0xFF);
	a ^= I128(0x0F);
	CHECK_EQ(a, I128(0xF0));

	a = I128(16);
	a >>= I128(2);
	CHECK_EQ(a, I128(4));

	a = I128(4);
	a <<= I128(2);
	CHECK_EQ(a, I128(16));
}

I32 execI128Test(int argc, char** argv)
{
	TEST_STATE_LOCAL;
	Timing::Timer timer;

	// Run deterministic tests first
	testNaNHandling(TEST_STATE_ARG);
	testOverflow(TEST_STATE_ARG);
	testDivision(TEST_STATE_ARG);
	testShifts(TEST_STATE_ARG);
	testBitwise(TEST_STATE_ARG);
	testComparisons(TEST_STATE_ARG);
	testUnaryOps(TEST_STATE_ARG);
	testConstructorsAndConversions(TEST_STATE_ARG);
	testFloatConversions(TEST_STATE_ARG);
	testBoundaryValues(TEST_STATE_ARG);
	testAssignmentOps(TEST_STATE_ARG);

	// Run randomized algebraic property tests
	RandomStream random(0);

	I128 phaseMasks[3] = {I128::max(), UINT64_MAX, 32767};
	for(Uptr phase = 0; phase < 3; ++phase)
	{
		for(Uptr i = 0; i < 100000; ++i)
		{
			I128 a = random.get() & phaseMasks[phase];
			I128 b = random.get() & phaseMasks[phase];
			I128 c = random.get() & phaseMasks[phase];

			CHECK_EQ_OR_NAN(a - a, I128(0));
			CHECK_EQ_OR_NAN(a + (-a), I128(0));
			CHECK_EQ_OR_NAN(a + 1, a - (-1));
			CHECK_EQ_OR_NAN(a - 1, a + (-1));
			CHECK_EQ_OR_NAN(a - 0, a);
			CHECK_EQ_OR_NAN(a + 0, a);                 // Identity
			CHECK_EQ_OR_NAN(a + b, b + a);             // Commutativity
			CHECK_EQ_OR_NAN((a + b) + c, a + (b + c)); // Associativity

			CHECK_EQ_OR_NAN(a * 0, I128(0));
			CHECK_EQ_OR_NAN(a * -1, -a);
			CHECK_EQ_OR_NAN(a * 1, a);                       // Identity
			CHECK_EQ_OR_NAN(a * b, b * a);                   // Commutativity
			CHECK_EQ_OR_NAN((a * b) * c, a * (b * c));       // Associativity
			CHECK_EQ_OR_NAN(a * (b + c), (a * b) + (a * c)); // Distributivity

			if(b != 0) { CHECK_EQ_OR_NAN((a * b) / b, a); }
			CHECK_EQ_OR_NAN((a + b) - b, a);
			CHECK_EQ_OR_NAN((a - b) + b, a);

			if(b != 0) { CHECK_EQ_OR_NAN(((a / b) * b) + (a % b), a); }
		}
	}

	Timing::logTimer("Ran I128 tests", timer);

	return testState.exitCode();
}
