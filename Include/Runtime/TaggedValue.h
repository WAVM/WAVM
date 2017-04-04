#pragma once

#include "WebAssembly/Types.h"
#include "Core/Floats.h"

namespace Runtime
{
	// A runtime value of any type.
	struct UntaggedValue
	{
		union
		{
			int32 i32;
			uint32 u32;
			int64 i64;
			uint64 u64;
			float32 f32;
			float64 f64;
		};
		
		UntaggedValue(int32 inI32) { i32 = inI32; }
		UntaggedValue(int64 inI64) { i64 = inI64; }
		UntaggedValue(uint32 inU32) { u32 = inU32; }
		UntaggedValue(uint64 inU64) { u64 = inU64; }
		UntaggedValue(float32 inF32) { f32 = inF32; }
		UntaggedValue(float64 inF64) { f64 = inF64; }
		UntaggedValue() {i64=0;}
	};

	// A boxed value: may hold any value that can be passed to a function invoked through the runtime.
	struct Value : UntaggedValue
	{
		WebAssembly::ValueType type;

		Value(int32 inI32): UntaggedValue(inI32), type(WebAssembly::ValueType::i32) {}
		Value(int64 inI64): UntaggedValue(inI64), type(WebAssembly::ValueType::i64) {}
		Value(uint32 inU32): UntaggedValue(inU32), type(WebAssembly::ValueType::i32) {}
		Value(uint64 inU64): UntaggedValue(inU64), type(WebAssembly::ValueType::i64) {}
		Value(float32 inF32): UntaggedValue(inF32), type(WebAssembly::ValueType::f32) {}
		Value(float64 inF64): UntaggedValue(inF64), type(WebAssembly::ValueType::f64) {}
		Value(WebAssembly::ValueType inType,UntaggedValue inValue): UntaggedValue(inValue), type(inType) {}
		Value(): type(WebAssembly::ValueType::any) {}
		
		friend std::string asString(const Value& value)
		{
			switch(value.type)
			{
			case WebAssembly::ValueType::i32: return "i32(" + std::to_string(value.i32) + ")";
			case WebAssembly::ValueType::i64: return "i64(" + std::to_string(value.i64) + ")";
			case WebAssembly::ValueType::f32: return "f32(" + Floats::asString(value.f32) + ")";
			case WebAssembly::ValueType::f64: return "f64(" + Floats::asString(value.f64) + ")";
			default: Core::unreachable();
			}
		}
	};

	// A boxed value: may hold any value that can be returned from a function invoked through the runtime.
	struct Result : UntaggedValue
	{
		WebAssembly::ResultType type;

		Result(int32 inI32): UntaggedValue(inI32), type(WebAssembly::ResultType::i32) {}
		Result(int64 inI64): UntaggedValue(inI64), type(WebAssembly::ResultType::i64) {}
		Result(uint32 inU32): UntaggedValue(inU32), type(WebAssembly::ResultType::i32) {}
		Result(uint64 inU64): UntaggedValue(inU64), type(WebAssembly::ResultType::i64) {}
		Result(float32 inF32): UntaggedValue(inF32), type(WebAssembly::ResultType::f32) {}
		Result(float64 inF64): UntaggedValue(inF64), type(WebAssembly::ResultType::f64) {}
		Result(WebAssembly::ResultType inType,UntaggedValue inValue): UntaggedValue(inValue), type(inType) {}
		Result(const Value& inValue): UntaggedValue(inValue), type(asResultType(inValue.type)) {}
		Result(): type(WebAssembly::ResultType::none) {}

		friend std::string asString(const Result& result)
		{
			switch(result.type)
			{
			case WebAssembly::ResultType::none: return "()";
			case WebAssembly::ResultType::i32: return "i32(" + std::to_string(result.i32) + ")";
			case WebAssembly::ResultType::i64: return "i64(" + std::to_string(result.i64) + ")";
			case WebAssembly::ResultType::f32: return "f32(" + Floats::asString(result.f32) + ")";
			case WebAssembly::ResultType::f64: return "f64(" + Floats::asString(result.f64) + ")";
			default: Core::unreachable();
			}
		}
	};

	// Compares whether two UntaggedValue of the same type have identical bits.
	inline bool areBitsEqual(WebAssembly::ResultType type,UntaggedValue a,UntaggedValue b)
	{
		switch(type)
		{
		case WebAssembly::ResultType::i32:
		case WebAssembly::ResultType::f32: return a.i32 == b.i32;
		case WebAssembly::ResultType::i64:
		case WebAssembly::ResultType::f64: return a.i64 == b.i64;
		case WebAssembly::ResultType::none: return true;
		default: Core::unreachable();
		};
	}

	// Compares whether two Value/Result have the same type and bits.
	inline bool areBitsEqual(const Value& a,const Value& b) { return a.type == b.type && areBitsEqual(asResultType(a.type),a,b); }
	inline bool areBitsEqual(const Result& a,const Result& b) { return a.type == b.type && areBitsEqual(a.type,a,b); }
	inline bool areBitsEqual(const Result& a,const Value& b) { return a.type == asResultType(b.type) && areBitsEqual(a.type,a,b); }
	inline bool areBitsEqual(const Value& a,const Result& b) { return asResultType(a.type) == b.type && areBitsEqual(b.type,a,b); }
}