#pragma once

#include <string.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Serialization {
	// An exception that is thrown for various errors during serialization.
	// Any code using serialization should handle it!
	struct FatalSerializationException
	{
		std::string message;
		FatalSerializationException(std::string&& inMessage) : message(std::move(inMessage)) {}
	};

	// An abstract output stream.
	struct OutputStream
	{
		static constexpr bool isInput = false;

		OutputStream() : next(nullptr), end(nullptr) {}
		virtual ~OutputStream() {}

		// Advances the stream cursor by numBytes, and returns a pointer to the previous stream
		// cursor.
		inline U8* advance(Uptr numBytes)
		{
			if(WAVM_UNLIKELY(Uptr(end - next) < numBytes)) { extendBuffer(numBytes); }
			WAVM_ASSERT(next + numBytes <= end);

			U8* data = next;
			next += numBytes;
			return data;
		}

	protected:
		U8* next;
		U8* end;

		// Called when there isn't enough space in the buffer to hold a write to the stream.
		// Should update next and end to point to a new buffer, and ensure that the new
		// buffer has at least numBytes. May throw FatalSerializationException.
		virtual void extendBuffer(Uptr numBytes) = 0;
	};

	// An output stream that writes to an array of bytes.
	struct ArrayOutputStream : public OutputStream
	{
		// Returns the number of bytes written so far.
		Uptr position() const { return next ? Uptr(next - bytes.data()) : 0; }

		// Moves the output array from the stream to the caller.
		std::vector<U8>&& getBytes()
		{
			bytes.resize(next - bytes.data());
			next = nullptr;
			end = nullptr;
			return std::move(bytes);
		}

	private:
		std::vector<U8> bytes;

		virtual void extendBuffer(Uptr numBytes)
		{
			const Uptr nextIndex = next - bytes.data();

			// Grow the array by larger and larger increments, so the time spent growing
			// the buffer is O(1).
			bytes.resize(std::max((Uptr)nextIndex + numBytes, (Uptr)bytes.size() * 7 / 5 + 32));

			next = bytes.data() + nextIndex;
			end = bytes.data() + bytes.size();
		}
	};

	// An abstract input stream.
	struct InputStream
	{
		static constexpr bool isInput = true;

		InputStream(const U8* inNext, const U8* inEnd) : next(inNext), end(inEnd) {}
		virtual ~InputStream() {}

		virtual Uptr capacity() const = 0;

		// Advances the stream cursor by numBytes, and returns a pointer to the previous stream
		// cursor. Throws FatalSerializationException if insufficient data is available.
		inline const U8* advance(Uptr numBytes)
		{
			if(WAVM_UNLIKELY(!next || next + numBytes > end)) { getMoreData(numBytes); }
			const U8* data = next;
			next += numBytes;
			return data;
		}

		// Returns a pointer to the current stream cursor, ensuring that there are at least numBytes
		// following it. Throws FatalSerializationException if insufficient data is available.
		inline const U8* peek(Uptr numBytes)
		{
			if(WAVM_UNLIKELY(next + numBytes > end)) { getMoreData(numBytes); }
			return next;
		}

		// Non-throwing advance: returns pointer to numBytes of data at current position and
		// advances, or returns nullptr if insufficient data is available.
		inline const U8* tryAdvance(Uptr numBytes)
		{
			if(WAVM_UNLIKELY(!next || next + numBytes > end))
			{
				if(!tryGetMoreData(numBytes)) { return nullptr; }
			}
			const U8* data = next;
			next += numBytes;
			return data;
		}

	protected:
		const U8* next;
		const U8* end;

		// Implemented by subclasses to provide more data when the buffer is exhausted.
		// Should update next and end to point to a new buffer with at least numBytes available.
		// Returns true on success, false if no more data is available.
		virtual bool tryGetMoreData(Uptr numBytes) = 0;

	private:
		// Non-virtual, non-inlined throwing path for advance/peek.
		WAVM_FORCENOINLINE void getMoreData(Uptr numBytes)
		{
			if(WAVM_UNLIKELY(!tryGetMoreData(numBytes)))
			{
				throw FatalSerializationException("expected data but found end of stream");
			}
		}
	};

	// An input stream that reads from a contiguous range of memory.
	struct MemoryInputStream : InputStream
	{
		MemoryInputStream(const void* inData, Uptr numBytes)
		: InputStream((const U8*)inData, (const U8*)inData + numBytes), data((const U8*)inData)
		{
		}

		// Creates a stream that extends to the end of the address space, for reading
		// trusted mapped memory where the total size is not known upfront (e.g. LSDA tables).
		explicit MemoryInputStream(const void* inData)
		: InputStream((const U8*)inData, (const U8*)UINTPTR_MAX), data((const U8*)inData)
		{
		}
		virtual Uptr capacity() const { return end - next; }

		// Returns the current byte position relative to the start of the memory region.
		Uptr position() const { return Uptr(next - data); }

		// Seeks to an absolute byte position. Returns false if out of range.
		bool seek(Uptr pos)
		{
			if(pos > Uptr(end - data)) { return false; }
			next = data + pos;
			return true;
		}

	private:
		const U8* data;

		virtual bool tryGetMoreData(Uptr numBytes) { return false; }
	};

	// Serialize raw byte sequences.
	WAVM_FORCEINLINE void serializeBytes(OutputStream& stream, const U8* bytes, Uptr numBytes)
	{
		if(numBytes) { memcpy(stream.advance(numBytes), bytes, numBytes); }
	}
	WAVM_FORCEINLINE void serializeBytes(InputStream& stream, U8* bytes, Uptr numBytes)
	{
		if(numBytes) { memcpy(bytes, stream.advance(numBytes), numBytes); }
	}

	// Write zero bytes until the stream reaches the given offset.
	inline void serializeZerosUntilOffset(ArrayOutputStream& stream, Uptr targetOffset)
	{
		Uptr pos = stream.position();
		WAVM_ASSERT(targetOffset >= pos);
		if(Uptr pad = targetOffset - pos) { memset(stream.advance(pad), 0, pad); }
	}

	// Serialize basic C++ types.
	template<typename Stream, typename Value>
	WAVM_FORCEINLINE void serializeNativeValue(Stream& stream, Value& value)
	{
		serializeBytes(stream, (U8*)&value, sizeof(Value));
	}

	// Non-throwing read of a fixed-size value from an input stream.
	template<typename T> WAVM_FORCEINLINE bool tryRead(InputStream& stream, T& outValue)
	{
		const U8* data = stream.tryAdvance(sizeof(T));
		if(!data) { return false; }
		memcpy(&outValue, data, sizeof(T));
		return true;
	}

	template<typename Stream> void serialize(Stream& stream, U8& i)
	{
		serializeNativeValue(stream, i);
	}
	template<typename Stream> void serialize(Stream& stream, U32& i)
	{
		serializeNativeValue(stream, i);
	}
	template<typename Stream> void serialize(Stream& stream, U64& i)
	{
		serializeNativeValue(stream, i);
	}
	template<typename Stream> void serialize(Stream& stream, I8& i)
	{
		serializeNativeValue(stream, i);
	}
	template<typename Stream> void serialize(Stream& stream, I32& i)
	{
		serializeNativeValue(stream, i);
	}
	template<typename Stream> void serialize(Stream& stream, I64& i)
	{
		serializeNativeValue(stream, i);
	}
	template<typename Stream> void serialize(Stream& stream, F32& f)
	{
		serializeNativeValue(stream, f);
	}
	template<typename Stream> void serialize(Stream& stream, F64& f)
	{
		serializeNativeValue(stream, f);
	}

	// Serializes a constant. If deserializing, throws a FatalSerializationException if the
	// deserialized value doesn't match the constant.
	template<typename Constant>
	void serializeConstant(InputStream& stream,
						   const char* constantMismatchMessage,
						   Constant constant)
	{
		Constant savedConstant;
		serialize(stream, savedConstant);
		if(savedConstant != constant)
		{
			throw FatalSerializationException(std::string(constantMismatchMessage) + ": loaded "
											  + std::to_string(savedConstant)
											  + " but was expecting " + std::to_string(constant));
		}
	}
	template<typename Constant>
	void serializeConstant(OutputStream& stream,
						   const char* constantMismatchMessage,
						   Constant constant)
	{
		serialize(stream, constant);
	}

	// Serialize containers.
	template<typename Stream> void serialize(Stream& stream, std::string& string)
	{
		Uptr size = string.size();
		serializeVarUInt32(stream, size);
		if(Stream::isInput)
		{
			// Advance the stream before resizing the string:
			// try to get a serialization exception before making a huge allocation for malformed
			// input.
			const U8* inputBytes = stream.advance(size);
			string.resize(size);
			memcpy(const_cast<char*>(string.data()), inputBytes, size);
			string.shrink_to_fit();
		}
		else
		{
			serializeBytes(stream, (U8*)string.c_str(), size);
		}
	}

	template<typename Stream, typename Element, typename Allocator, typename SerializeElement>
	void serializeArray(Stream& stream,
						std::vector<Element, Allocator>& vector,
						SerializeElement serializeElement)
	{
		Uptr size = vector.size();
		serializeVarUInt32(stream, size);
		if(Stream::isInput)
		{
			// Grow the vector one element at a time:
			// try to get a serialization exception before making a huge allocation for malformed
			// input.
			vector.clear();
			for(Uptr index = 0; index < size; ++index)
			{
				vector.push_back(Element());
				serializeElement(stream, vector.back());
			}
			vector.shrink_to_fit();
		}
		else
		{
			for(Uptr index = 0; index < vector.size(); ++index)
			{
				serializeElement(stream, vector[index]);
			}
		}
	}

	template<typename Stream, typename Element, typename Allocator>
	void serialize(Stream& stream, std::vector<Element, Allocator>& vector)
	{
		serializeArray(
			stream, vector, [](Stream& stream, Element& element) { serialize(stream, element); });
	}
}}
