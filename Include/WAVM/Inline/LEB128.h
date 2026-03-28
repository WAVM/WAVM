#pragma once

#include <cstdint>
#include <limits> // IWYU pragma: keep
#include <string> // IWYU pragma: keep
#include <type_traits>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Serialization {
	// Result enum for non-throwing LEB128 read.
	enum class LEB128Result
	{
		Success,
		Truncated,        // stream exhausted mid-encoding
		Overlong,         // continuation bit set on the maxBytes-th byte
		InvalidUnusedBits // unused bits in last byte don't match expected pattern
	};

	// Core non-throwing LEB128 read. Returns LEB128Result.
	template<typename Value, Uptr maxBits>
	WAVM_FORCEINLINE LEB128Result trySerializeVarInt(InputStream& stream, Value& outValue)
	{
		static constexpr Uptr maxBytes = (maxBits + 6) / 7;
		U8 bytes[maxBytes] = {0};
		Uptr numBytes = 0;
		I8 signExtendShift = (I8)sizeof(Value) * 8;
		while(numBytes < maxBytes)
		{
			const U8* bytePtr = stream.tryAdvance(1);
			if(!bytePtr) { return LEB128Result::Truncated; }
			U8 byte = *bytePtr;
			bytes[numBytes] = byte;
			++numBytes;
			signExtendShift -= 7;
			if(!(byte & 0x80)) { break; }
		}

		// If we consumed all maxBytes and the last one still has the continuation bit,
		// the encoding is overlong.
		if(numBytes == maxBytes && (bytes[maxBytes - 1] & 0x80)) { return LEB128Result::Overlong; }

		// Validate unused bits in the last byte when all maxBytes were consumed.
		if(numBytes == maxBytes)
		{
			static constexpr Uptr numUsedBitsInLastByte = maxBits - (maxBytes - 1) * 7;
			static constexpr Uptr numUnusedBitsInLast = 8 - numUsedBitsInLastByte;
			static constexpr U8 lastBitUsedMask = U8(1 << (numUsedBitsInLastByte - 1));
			static constexpr U8 lastByteUsedMask = U8(1 << numUsedBitsInLastByte) - U8(1);
			static constexpr U8 lastByteSignedMask = U8(~U8(lastByteUsedMask) & ~U8(0x80));
			const U8 lastByte = bytes[maxBytes - 1];
			if(!std::is_signed<Value>::value)
			{
				if((lastByte & ~lastByteUsedMask) != 0) { return LEB128Result::InvalidUnusedBits; }
			}
			else
			{
				const I8 signBit = I8((lastByte & lastBitUsedMask) << numUnusedBitsInLast);
				const I8 signExtendedLastBit = signBit >> numUnusedBitsInLast;
				if((lastByte & ~lastByteUsedMask) != (signExtendedLastBit & lastByteSignedMask))
				{
					return LEB128Result::InvalidUnusedBits;
				}
			}
		}

		// Decode the buffer's bytes into the output integer.
		outValue = 0;
		for(Uptr byteIndex = 0; byteIndex < maxBytes; ++byteIndex)
		{
			outValue |= Value(U64(bytes[byteIndex] & ~0x80) << U64(byteIndex * 7));
		}

		// Sign extend the output integer to the full size of Value.
		if(std::is_signed<Value>::value && signExtendShift > 0)
		{
			outValue = Value(outValue << signExtendShift) >> signExtendShift;
		}

		return LEB128Result::Success;
	}

	// LEB128 variable-length integer serialization.
	template<typename Value, Uptr maxBits>
	WAVM_FORCEINLINE void serializeVarInt(OutputStream& stream,
										  Value& inValue,
										  Value minValue,
										  Value maxValue)
	{
		Value value = inValue;

		if(value < minValue || value > maxValue)
		{
			throw FatalSerializationException(
				std::string("out-of-range value: ") + std::to_string(minValue)
				+ "<=" + std::to_string(value) + "<=" + std::to_string(maxValue));
		}

		bool more = true;
		while(more)
		{
			U8 outputByte = (U8)(value & 127);
			value >>= 7;
			more = std::is_signed<Value>::value
					   ? (value != 0 && value != Value(-1)) || (value >= 0 && (outputByte & 0x40))
							 || (value < 0 && !(outputByte & 0x40))
					   : (value != 0);
			if(more) { outputByte |= 0x80; }
			*stream.advance(1) = outputByte;
		};
	}

	template<typename Value, Uptr maxBits>
	WAVM_FORCEINLINE void serializeVarInt(InputStream& stream,
										  Value& value,
										  Value minValue,
										  Value maxValue)
	{
		LEB128Result result = trySerializeVarInt<Value, maxBits>(stream, value);
		switch(result)
		{
		case LEB128Result::Success: break;
		case LEB128Result::Truncated:
			throw FatalSerializationException("expected data but found end of stream");
		case LEB128Result::Overlong: throw FatalSerializationException("overlong LEB128 encoding");
		case LEB128Result::InvalidUnusedBits:
			throw FatalSerializationException(
				std::is_signed<Value>::value
					? "Invalid signed LEB encoding: unused bits in final byte must match the "
					  "most-significant used bit"
					: "Invalid unsigned LEB encoding: unused bits in final byte must be 0");
		default: WAVM_UNREACHABLE();
		}
		if(value < minValue || value > maxValue)
		{
			throw FatalSerializationException(
				std::string("out-of-range value: ") + std::to_string(minValue)
				+ "<=" + std::to_string(value) + "<=" + std::to_string(maxValue));
		}
	}

	// Non-throwing ULEB128 read.
	WAVM_FORCEINLINE bool trySerializeVarUInt64(InputStream& stream, U64& outValue)
	{
		return trySerializeVarInt<U64, 64>(stream, outValue) == LEB128Result::Success;
	}

	// Non-throwing SLEB128 read.
	WAVM_FORCEINLINE bool trySerializeVarSInt64(InputStream& stream, I64& outValue)
	{
		return trySerializeVarInt<I64, 64>(stream, outValue) == LEB128Result::Success;
	}

	// Typed non-throwing ULEB128 read with overflow checking.
	template<typename T> WAVM_FORCEINLINE bool trySerializeVarUInt(InputStream& stream, T& outValue)
	{
		static_assert(std::is_integral<T>::value && !std::is_signed<T>::value,
					  "trySerializeVarUInt requires unsigned integral type");
		U64 raw;
		if(!trySerializeVarUInt64(stream, raw)) { return false; }
		if(raw > U64(std::numeric_limits<T>::max())) { return false; }
		outValue = T(raw);
		return true;
	}

	// Typed non-throwing SLEB128 read with range checking.
	template<typename T> WAVM_FORCEINLINE bool trySerializeVarSInt(InputStream& stream, T& outValue)
	{
		static_assert(std::is_integral<T>::value && std::is_signed<T>::value,
					  "trySerializeVarSInt requires signed integral type");
		I64 raw;
		if(!trySerializeVarSInt64(stream, raw)) { return false; }
		if(raw < I64(std::numeric_limits<T>::min()) || raw > I64(std::numeric_limits<T>::max()))
		{
			return false;
		}
		outValue = T(raw);
		return true;
	}

	// Helpers for various common LEB128 parameters.
	template<typename Stream, typename Value> void serializeVarUInt1(Stream& stream, Value& value)
	{
		serializeVarInt<Value, 1>(stream, value, 0, 1);
	}
	template<typename Stream, typename Value> void serializeVarUInt7(Stream& stream, Value& value)
	{
		serializeVarInt<Value, 7>(stream, value, 0, 127);
	}
	template<typename Stream, typename Value> void serializeVarUInt8(Stream& stream, Value& value)
	{
		serializeVarInt<Value, 8>(stream, value, 0, 255);
	}
	template<typename Stream, typename Value> void serializeVarUInt32(Stream& stream, Value& value)
	{
		serializeVarInt<Value, 32>(stream, value, 0, UINT32_MAX);
	}
	template<typename Stream, typename Value> void serializeVarUInt64(Stream& stream, Value& value)
	{
		serializeVarInt<Value, 64>(stream, value, 0, UINT64_MAX);
	}
	template<typename Stream, typename Value> void serializeVarInt7(Stream& stream, Value& value)
	{
		serializeVarInt<Value, 7>(stream, value, -64, 63);
	}
	template<typename Stream, typename Value> void serializeVarInt32(Stream& stream, Value& value)
	{
		serializeVarInt<Value, 32>(stream, value, INT32_MIN, INT32_MAX);
	}
	template<typename Stream, typename Value> void serializeVarInt64(Stream& stream, Value& value)
	{
		serializeVarInt<Value, 64>(stream, value, INT64_MIN, INT64_MAX);
	}
}}
