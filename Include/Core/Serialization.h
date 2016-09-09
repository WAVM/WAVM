#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"

#include <string>
#include <vector>

namespace Serialization
{
	struct FatalSerializationException
	{
		std::string message;
		FatalSerializationException(std::string&& inMessage)
		: message(std::move(inMessage)) {}
	};

	struct OutputStream
	{
		enum { isInput = false };

		OutputStream(): next(nullptr), end(nullptr) {}

		inline void serializeBytes(const uint8* data,size_t numBytes)
		{
			if(next + numBytes > end) { extendBuffer(numBytes); }
			memcpy(next,data,numBytes);
			next += numBytes;
		}

	protected:

		uint8* next;
		uint8* end;

		virtual void extendBuffer(size_t numBytes) = 0;
	};

	struct ArrayOutputStream : public OutputStream
	{
		std::vector<uint8>&& getBytes()
		{
			bytes.resize(next - bytes.data());
			next = nullptr;
			end = nullptr;
			return std::move(bytes);
		}
		
	private:

		std::vector<uint8> bytes;

		virtual void extendBuffer(size_t numBytes)
		{
			const uintptr nextIndex = next - bytes.data();

			bytes.resize(std::max(nextIndex+numBytes,bytes.size() * 7 / 5 + 32));

			next = bytes.data() + nextIndex;
			end = bytes.data() + bytes.size();
		}
	};

	struct InputStream
	{
		enum { isInput = true };

		InputStream(const uint8* inNext,uintptr numBytes)
		: next(inNext), end(inNext + numBytes) {}

		InputStream(InputStream& parentStream,uintptr numBytes)
		: next(parentStream.next)
		, end(parentStream.end + numBytes)
		{
			assert(parentStream.capacity() < numBytes);
			parentStream.next += numBytes;
		}

		inline uintptr capacity() const { return end - next; }

		inline void serializeBytes(uint8* data,size_t numBytes)
		{
			if(capacity() < numBytes) { throw FatalSerializationException("expected data but found end of stream"); }
			memcpy(data,next,numBytes);
			next += numBytes;
		}

		InputStream makeSubstream(uintptr numBytes)
		{
			if(capacity() < numBytes) { throw FatalSerializationException("expected data but found end of stream"); }
			const uint8* substreamStart = next;
			next += numBytes;
			return InputStream(substreamStart,next - substreamStart);
		}

	private:

		const uint8* next;
		const uint8* end;
	};

	template<typename Value,typename ByteValue>
	void serializeVarInt(OutputStream& stream,Value& inValue,Value minValue,Value maxValue)
	{
		Value value = inValue;

		if(value < minValue || value > maxValue)
		{
			throw FatalSerializationException(std::string("out-of-range value: ") + std::to_string(minValue) + "<=" + std::to_string(value) + "<=" + std::to_string(maxValue));
		}

		if(minValue < 0)
		{
			bool more = true;
			while(more)
			{
				ByteValue outputByte = (ByteValue)(value&127);
				value >>= 7;
				more = std::is_signed<Value>::value
					? (value != 0 && value != Value(-1)) || (value >= 0 && (outputByte & 0x40)) || (value < 0 && !(outputByte & 0x40))
					: (value != 0);
				outputByte |= more ? 0x80 : 0;
				stream.serializeBytes((uint8*)&outputByte,1);
			};
		}
		else
		{
			do
			{
				ByteValue outputByte = (ByteValue)(value&127);
				value >>= 7;
				if(value) { outputByte |= 0x80; }
				stream.serializeBytes((uint8*)&outputByte,1);
			}
			while(value != 0);
		}
	}
	
	template<typename Value,typename ByteValue>
	void serializeVarInt(InputStream& stream,Value& value,Value minValue,Value maxValue)
	{
		value = 0;
		Value shift = 0;
		ByteValue byte = 0;
		while (1)
		{
			stream.serializeBytes((uint8*)&byte,1);
			bool last = !(byte & 128);
			Value payload = byte & 127;
			typedef typename std::make_unsigned<Value>::type mask_type;
			auto shift_mask = 0 == shift
				? ~mask_type(0)
				: ((mask_type(1) << (sizeof(Value) * 8 - shift)) - 1u);
			Value significant_payload = payload & shift_mask;
			if (significant_payload != payload)
			{
				assert(std::is_signed<Value>::value && last &&
				"dropped bits only valid for signed LEB");
			}
			value |= significant_payload << shift;
			if (last) break;
			shift += 7;
			assert(size_t(shift) < sizeof(Value) * 8 && "LEB overflow");
		}
		// If signed LEB, then we might need to sign-extend. (compile should
		// optimize this out if not needed).
		if (std::is_signed<Value>::value)
		{
			shift += 7;
			if ((byte & 64) && size_t(shift) < 8 * sizeof(Value))
			{
				size_t sext_bits = 8 * sizeof(Value) - size_t(shift);
				value <<= sext_bits;
				value >>= sext_bits;
				assert(value < 0 && "sign-extend should produces a negative value");
			}
		}
		
		if(value < minValue || value > maxValue)
		{
			throw FatalSerializationException(std::string("out-of-range value: ") + std::to_string(minValue) + "<=" + std::to_string(value) + "<=" + std::to_string(maxValue));
		}

	}
	
	template<typename Stream,typename Value>
	void serializeVarUInt1(Stream& stream,Value& value) { serializeVarInt<Value,uint8>(stream,value,0,1); }
	
	template<typename Stream,typename Value>
	void serializeVarUInt7(Stream& stream,Value& value) { serializeVarInt<Value,uint8>(stream,value,0,127); }

	template<typename Stream,typename Value>
	void serializeVarUInt32(Stream& stream,Value& value) { serializeVarInt<Value,uint8>(stream,value,0,UINT32_MAX); }
	
	template<typename Stream,typename Value>
	void serializeVarUInt64(Stream& stream,Value& value) { serializeVarInt<Value,uint8>(stream,value,0,UINT32_MAX); }

	template<typename Stream,typename Value>
	void serializeVarInt32(Stream& stream,Value& value) { serializeVarInt<Value,int8>(stream,value,INT32_MIN,INT32_MAX); }

	template<typename Stream,typename Value>
	void serializeVarInt64(Stream& stream,Value& value) { serializeVarInt<Value,int8>(stream,value,INT64_MIN,INT64_MAX); }
	
	template<typename Stream,typename Value>
	FORCEINLINE void serializeNativeValue(Stream& stream,Value& value) { stream.serializeBytes((uint8*)&value,sizeof(Value)); }

	template<typename Constant>
	void serializeConstant(InputStream& stream,const char* constantMismatchMessage,Constant constant)
	{
		Constant savedConstant;
		serialize(stream,savedConstant);
		if(savedConstant != constant)
		{
			throw FatalSerializationException(std::string(constantMismatchMessage) + ": loaded " + std::to_string(savedConstant) + " but was expecting " + std::to_string(constant));
		}
	}
	template<typename Constant>
	void serializeConstant(OutputStream& stream,const char* constantMismatchMessage,Constant constant)
	{
		serialize(stream,constant);
	}

	template<typename Stream> void serialize(Stream& stream,uint8& i) { serializeNativeValue(stream,i); }
	template<typename Stream> void serialize(Stream& stream,uint32& i) { serializeNativeValue(stream,i); }
	template<typename Stream> void serialize(Stream& stream,uint64& i) { serializeNativeValue(stream,i);  }
	template<typename Stream> void serialize(Stream& stream,int8& i) { serializeNativeValue(stream,i); }
	template<typename Stream> void serialize(Stream& stream,int32& i) { serializeNativeValue(stream,i); }
	template<typename Stream> void serialize(Stream& stream,int64& i) { serializeNativeValue(stream,i); }
	template<typename Stream> void serialize(Stream& stream,float32& f) { serializeNativeValue(stream,f); }
	template<typename Stream> void serialize(Stream& stream,float64& f) { serializeNativeValue(stream,f); }

	template<typename Stream>
	void serialize(Stream& stream,std::string& string)
	{
		size_t size = string.size();
		serializeVarUInt32(stream,size);
		if(Stream::isInput) { string.resize(size); }
		stream.serializeBytes((uint8*)string.c_str(),size);
		if(Stream::isInput) { string.shrink_to_fit(); }
	}

	template<typename Stream,typename Element,typename Allocator,typename SerializeElement>
	void serializeArray(Stream& stream,std::vector<Element,Allocator>& vector,SerializeElement serializeElement)
	{
		size_t size = vector.size();
		serializeVarUInt32(stream,size);
		if(Stream::isInput) { vector.resize(size); }
		for(uintptr index = 0;index < vector.size();++index) { serializeElement(stream,vector[index]); }
		if(Stream::isInput) { vector.shrink_to_fit(); }
	}

	template<typename Stream,typename Element,typename Allocator>
	void serialize(Stream& stream,std::vector<Element,Allocator>& vector)
	{
		serializeArray(stream,vector,[](Stream& stream,Element& element){serialize(stream,element);});
	}
}