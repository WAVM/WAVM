#include <cstdint>
#include <functional>
#include <vector>
#include "TestUtils.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/LEB128.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "wavm-test.h"

using namespace WAVM;
using namespace WAVM::Serialization;
using namespace WAVM::Testing;

template<typename Value, Uptr maxBits>
static void roundtrip(TEST_STATE_PARAM, Value value, Value minValue, Value maxValue)
{
	// Serialize
	ArrayOutputStream outputStream;
	serializeVarInt<Value, maxBits>(outputStream, value, minValue, maxValue);
	std::vector<U8> bytes = outputStream.getBytes();

	// Deserialize
	MemoryInputStream inputStream(bytes.data(), bytes.size());
	Value decoded = 0;
	serializeVarInt<Value, maxBits>(inputStream, decoded, minValue, maxValue);

	CHECK_EQ(decoded, value);
}

static void testUnsigned32(TEST_STATE_PARAM)
{
	// Single byte values (0-127)
	roundtrip<U32, 32>(TEST_STATE_ARG, 0, 0, UINT32_MAX);
	roundtrip<U32, 32>(TEST_STATE_ARG, 1, 0, UINT32_MAX);
	roundtrip<U32, 32>(TEST_STATE_ARG, 127, 0, UINT32_MAX);

	// Two byte values (128-16383)
	roundtrip<U32, 32>(TEST_STATE_ARG, 128, 0, UINT32_MAX);
	roundtrip<U32, 32>(TEST_STATE_ARG, 255, 0, UINT32_MAX);
	roundtrip<U32, 32>(TEST_STATE_ARG, 16383, 0, UINT32_MAX);

	// Larger values
	roundtrip<U32, 32>(TEST_STATE_ARG, 16384, 0, UINT32_MAX);
	roundtrip<U32, 32>(TEST_STATE_ARG, 1000000, 0, UINT32_MAX);

	// Maximum value
	roundtrip<U32, 32>(TEST_STATE_ARG, UINT32_MAX, 0, UINT32_MAX);
}

static void testSigned32(TEST_STATE_PARAM)
{
	// Zero
	roundtrip<I32, 32>(TEST_STATE_ARG, 0, INT32_MIN, INT32_MAX);

	// Positive values
	roundtrip<I32, 32>(TEST_STATE_ARG, 1, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, 63, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, 64, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, 127, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, 128, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, 1000000, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, INT32_MAX, INT32_MIN, INT32_MAX);

	// Negative values
	roundtrip<I32, 32>(TEST_STATE_ARG, -1, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, -64, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, -65, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, -128, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, -1000000, INT32_MIN, INT32_MAX);
	roundtrip<I32, 32>(TEST_STATE_ARG, INT32_MIN, INT32_MIN, INT32_MAX);
}

static void testUnsigned64(TEST_STATE_PARAM)
{
	roundtrip<U64, 64>(TEST_STATE_ARG, 0, 0, UINT64_MAX);
	roundtrip<U64, 64>(TEST_STATE_ARG, 1, 0, UINT64_MAX);
	roundtrip<U64, 64>(TEST_STATE_ARG, 127, 0, UINT64_MAX);
	roundtrip<U64, 64>(TEST_STATE_ARG, 128, 0, UINT64_MAX);
	roundtrip<U64, 64>(TEST_STATE_ARG, U64(1) << 32, 0, UINT64_MAX);
	roundtrip<U64, 64>(TEST_STATE_ARG, UINT64_MAX, 0, UINT64_MAX);
}

static void testSigned64(TEST_STATE_PARAM)
{
	roundtrip<I64, 64>(TEST_STATE_ARG, 0, INT64_MIN, INT64_MAX);
	roundtrip<I64, 64>(TEST_STATE_ARG, 1, INT64_MIN, INT64_MAX);
	roundtrip<I64, 64>(TEST_STATE_ARG, -1, INT64_MIN, INT64_MAX);
	roundtrip<I64, 64>(TEST_STATE_ARG, 63, INT64_MIN, INT64_MAX);
	roundtrip<I64, 64>(TEST_STATE_ARG, -64, INT64_MIN, INT64_MAX);
	roundtrip<I64, 64>(TEST_STATE_ARG, I64(1) << 32, INT64_MIN, INT64_MAX);
	roundtrip<I64, 64>(TEST_STATE_ARG, INT64_MAX, INT64_MIN, INT64_MAX);
	roundtrip<I64, 64>(TEST_STATE_ARG, INT64_MIN, INT64_MIN, INT64_MAX);
}

static void testSmallWidths(TEST_STATE_PARAM)
{
	// VarUInt1
	roundtrip<U32, 1>(TEST_STATE_ARG, 0, 0, 1);
	roundtrip<U32, 1>(TEST_STATE_ARG, 1, 0, 1);

	// VarUInt7
	roundtrip<U32, 7>(TEST_STATE_ARG, 0, 0, 127);
	roundtrip<U32, 7>(TEST_STATE_ARG, 127, 0, 127);

	// VarInt7
	roundtrip<I32, 7>(TEST_STATE_ARG, 0, -64, 63);
	roundtrip<I32, 7>(TEST_STATE_ARG, 63, -64, 63);
	roundtrip<I32, 7>(TEST_STATE_ARG, -64, -64, 63);
	roundtrip<I32, 7>(TEST_STATE_ARG, -1, -64, 63);
}

static bool expectSerializationException(std::function<void()> func)
{
	try
	{
		func();
		return false;
	}
	catch(FatalSerializationException&)
	{
		return true;
	}
}

static void testOutOfRange(TEST_STATE_PARAM)
{
	// Serializing out-of-range values should throw

	// Unsigned value exceeds max
	CHECK_TRUE(expectSerializationException([]() {
		ArrayOutputStream out;
		U32 v = 2;
		serializeVarUInt1(out, v);
	}));

	CHECK_TRUE(expectSerializationException([]() {
		ArrayOutputStream out;
		U32 v = 128;
		serializeVarUInt7(out, v);
	}));

	// Signed value exceeds range
	CHECK_TRUE(expectSerializationException([]() {
		ArrayOutputStream out;
		I32 v = 64;
		serializeVarInt7(out, v);
	}));

	CHECK_TRUE(expectSerializationException([]() {
		ArrayOutputStream out;
		I32 v = -65;
		serializeVarInt7(out, v);
	}));
}

static void testTruncatedInput(TEST_STATE_PARAM)
{
	// Reading from an empty stream should throw
	CHECK_TRUE(expectSerializationException([]() {
		MemoryInputStream in(nullptr, 0);
		U32 v = 0;
		serializeVarUInt32(in, v);
	}));

	// Truncated multi-byte encoding (high bit set but no continuation)
	U8 truncated[] = {0x80}; // high bit set, expects more bytes
	CHECK_TRUE(expectSerializationException([&]() {
		MemoryInputStream in(truncated, sizeof(truncated));
		U32 v = 0;
		serializeVarUInt32(in, v);
	}));
}

static void testInvalidEncoding(TEST_STATE_PARAM)
{
	// Unsigned LEB128 with unused bits set in the final byte should be rejected.
	// For a 1-bit unsigned LEB, only bit 0 is used; bits 1-6 must be 0.
	// Byte 0x02 has bit 1 set, which is invalid for a 1-bit value.
	U8 invalidUint1[] = {0x02};
	CHECK_TRUE(expectSerializationException([&]() {
		MemoryInputStream in(invalidUint1, sizeof(invalidUint1));
		U32 v = 0;
		serializeVarUInt1(in, v);
	}));

	// For unsigned 7-bit LEB, the value 128 requires two bytes.
	// A single byte 0x80 has the continuation bit set but no second byte.
	// Using a two-byte encoding: 0x80 0x01 = 128, which exceeds the range [0,127] for VarUInt7.
	// This is caught as out-of-range.
	U8 overflowUint7[] = {0x80, 0x01};
	CHECK_TRUE(expectSerializationException([&]() {
		MemoryInputStream in(overflowUint7, sizeof(overflowUint7));
		U32 v = 0;
		serializeVarUInt7(in, v);
	}));

	// Signed LEB128 with continuation bit set in final byte: for VarInt7,
	// maxBytes=1, so only one byte is read. If that byte has the continuation
	// bit (0x80) set, the unused bits check rejects it.
	U8 invalidInt7[] = {0xC0}; // continuation bit + sign bit set
	CHECK_TRUE(expectSerializationException([&]() {
		MemoryInputStream in(invalidInt7, sizeof(invalidInt7));
		I32 v = 0;
		serializeVarInt7(in, v);
	}));
}

static void testEncodingSize(TEST_STATE_PARAM)
{
	// Verify that LEB128 encoding uses the expected number of bytes

	// Single byte: 0-127 unsigned, -64 to 63 signed
	{
		ArrayOutputStream out;
		U32 v = 0;
		serializeVarUInt32(out, v);
		CHECK_EQ((U32)out.getBytes().size(), U32(1));
	}
	{
		ArrayOutputStream out;
		U32 v = 127;
		serializeVarUInt32(out, v);
		CHECK_EQ((U32)out.getBytes().size(), U32(1));
	}

	// Two bytes: 128-16383 unsigned
	{
		ArrayOutputStream out;
		U32 v = 128;
		serializeVarUInt32(out, v);
		CHECK_EQ((U32)out.getBytes().size(), U32(2));
	}

	// Five bytes for max U32
	{
		ArrayOutputStream out;
		U32 v = UINT32_MAX;
		serializeVarUInt32(out, v);
		CHECK_EQ((U32)out.getBytes().size(), U32(5));
	}
}

I32 execLEB128Test(int argc, char** argv)
{
	TEST_STATE_LOCAL;
	Timing::Timer timer;

	testUnsigned32(TEST_STATE_ARG);
	testSigned32(TEST_STATE_ARG);
	testUnsigned64(TEST_STATE_ARG);
	testSigned64(TEST_STATE_ARG);
	testSmallWidths(TEST_STATE_ARG);
	testEncodingSize(TEST_STATE_ARG);
	testOutOfRange(TEST_STATE_ARG);
	testTruncatedInput(TEST_STATE_ARG);
	testInvalidEncoding(TEST_STATE_ARG);

	Timing::logTimer("Ran LEB128 tests", timer);

	return testState.exitCode();
}
