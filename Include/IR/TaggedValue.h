#pragma once

#include "IR/Types.h"

#include <string.h>

namespace IR
{
	// A runtime value of any type.
	struct UntaggedValue
	{
		union
		{
			I32 i32;
			U32 u32;
			I64 i64;
			U64 u64;
			F32 f32;
			F64 f64;
			V128 v128;
			U8 bytes[16];
		};

		UntaggedValue(I32 inI32) { i32 = inI32; }
		UntaggedValue(I64 inI64) { i64 = inI64; }
		UntaggedValue(U32 inU32) { u32 = inU32; }
		UntaggedValue(U64 inU64) { u64 = inU64; }
		UntaggedValue(F32 inF32) { f32 = inF32; }
		UntaggedValue(F64 inF64) { f64 = inF64; }
		UntaggedValue(V128 inV128) { v128 = inV128; }
		UntaggedValue() { memset(this, 0, sizeof(*this)); }
	};

	// A boxed value: may hold any value that can be passed to a function invoked through the
	// runtime.
	struct Value : UntaggedValue
	{
		IR::ValueType type;

		Value(I32 inI32) : UntaggedValue(inI32), type(IR::ValueType::i32) {}
		Value(I64 inI64) : UntaggedValue(inI64), type(IR::ValueType::i64) {}
		Value(U32 inU32) : UntaggedValue(inU32), type(IR::ValueType::i32) {}
		Value(U64 inU64) : UntaggedValue(inU64), type(IR::ValueType::i64) {}
		Value(F32 inF32) : UntaggedValue(inF32), type(IR::ValueType::f32) {}
		Value(F64 inF64) : UntaggedValue(inF64), type(IR::ValueType::f64) {}
		Value(const V128& inV128) : UntaggedValue(inV128), type(IR::ValueType::v128) {}
		Value(IR::ValueType inType, UntaggedValue inValue) : UntaggedValue(inValue), type(inType) {}
		Value() : type(IR::ValueType::any) {}

		friend std::string asString(const Value& value)
		{
			switch(value.type)
			{
			case IR::ValueType::i32: return "i32.const " + IR::asString(value.i32);
			case IR::ValueType::i64: return "i64.const " + IR::asString(value.i64);
			case IR::ValueType::f32: return "f32.const " + IR::asString(value.f32);
			case IR::ValueType::f64: return "f64.const " + IR::asString(value.f64);
			case IR::ValueType::v128: return "v128.const i32 " + IR::asString(value.v128);
			default: Errors::unreachable();
			}
		}

		friend bool operator==(const Value& left, const Value& right)
		{
			if(left.type != right.type) { return false; }
			switch(left.type)
			{
			case IR::ValueType::i32:
			case IR::ValueType::f32: return left.i32 == right.i32;
			case IR::ValueType::i64:
			case IR::ValueType::f64: return left.i64 == right.i64;
			case IR::ValueType::v128: return left.v128 == right.v128;
			default: Errors::unreachable();
			};
		}

		friend bool operator!=(const Value& left, const Value& right) { return !(left == right); }
	};

	// A boxed value: may hold any value that can be returned from a function invoked through the
	// runtime.
	struct ValueTuple
	{
		std::vector<Value> values;

		ValueTuple(IR::ValueType inType, UntaggedValue inValue) : values({Value(inType, inValue)})
		{
		}
		ValueTuple(IR::TypeTuple types, UntaggedValue* inValues)
		{
			values.reserve(types.size());
			for(IR::ValueType type : types) { values.push_back(Value(type, *inValues++)); }
		}
		ValueTuple(const Value& inValue) : values({inValue}) {}
		ValueTuple() {}

		Uptr size() const { return values.size(); }
		Value& operator[](Uptr index) { return values[index]; }
		const Value& operator[](Uptr index) const { return values[index]; }

		friend std::string asString(const ValueTuple& valueTuple)
		{
			std::string result = "(";
			for(Uptr elementIndex = 0; elementIndex < valueTuple.size(); ++elementIndex)
			{
				if(elementIndex != 0) { result += ", "; }
				result += asString(valueTuple[elementIndex]);
			}
			result += ")";
			return result;
		}

		friend bool operator==(const ValueTuple& left, const ValueTuple& right)
		{
			if(left.size() != right.size()) { return false; }
			for(Uptr valueIndex = 0; valueIndex < left.size(); ++valueIndex)
			{
				if(left[valueIndex] != right[valueIndex]) { return false; }
			}
			return true;
		}

		friend bool operator!=(const ValueTuple& left, const ValueTuple& right)
		{
			return !(left == right);
		}
	};
}