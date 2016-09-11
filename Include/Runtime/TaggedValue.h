#pragma once

#include "WebAssembly/Types.h"

namespace Runtime
{
	struct Exception;

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
			Exception* exception;
		};
		
		UntaggedValue(int32 inI32) { i32 = inI32; }
		UntaggedValue(int64 inI64) { i64 = inI64; }
		UntaggedValue(uint32 inU32) { u32 = inU32; }
		UntaggedValue(uint64 inU64) { u64 = inU64; }
		UntaggedValue(float32 inF32) { f32 = inF32; }
		UntaggedValue(float64 inF64) { f64 = inF64; }
		UntaggedValue(Exception* inException) { exception = inException; }
		UntaggedValue() {i64=0;}
	};

	// The type of a runtime value.
	enum class TypeId : uint8
	{
		unit = 0,
		i32,
		i64,
		f32,
		f64,
		exception
	};
	
	// A boxed value: may hold any value that can be passed to, or returned from a function invoked through the runtime.
	struct Value : UntaggedValue
	{
		TypeId type;

		Value(int32 inI32): UntaggedValue(inI32), type(TypeId::i32) {}
		Value(int64 inI64): UntaggedValue(inI64), type(TypeId::i64) {}
		Value(uint32 inU32): UntaggedValue(inU32), type(TypeId::i32) {}
		Value(uint64 inU64): UntaggedValue(inU64), type(TypeId::i64) {}
		Value(float32 inF32): UntaggedValue(inF32), type(TypeId::f32) {}
		Value(float64 inF64): UntaggedValue(inF64), type(TypeId::f64) {}
		Value(Exception* inException): UntaggedValue(inException), type(TypeId::exception) {}
		Value(TypeId inType,UntaggedValue inValue): UntaggedValue(inValue), type(inType) {}
		Value(): type(TypeId::unit) {}
	};

	namespace NativeTypes
	{
		typedef void unit;
		typedef int32 i32;
		typedef int64 i64;
		typedef float32 f32;
		typedef float64 f64;
	};

	inline TypeId asRuntimeValueType(const WebAssembly::ValueType valueType)
	{
		switch(valueType)
		{
		case WebAssembly::ValueType::i32: return TypeId::i32;
		case WebAssembly::ValueType::i64: return TypeId::i64;
		case WebAssembly::ValueType::f32: return TypeId::f32;
		case WebAssembly::ValueType::f64: return TypeId::f64;
		default: throw;
		};
	}
	
	inline TypeId asRuntimeValueType(const WebAssembly::ResultType valueType)
	{
		switch(valueType)
		{
		case WebAssembly::ResultType::unit: return TypeId::unit;
		case WebAssembly::ResultType::i32: return TypeId::i32;
		case WebAssembly::ResultType::i64: return TypeId::i64;
		case WebAssembly::ResultType::f32: return TypeId::f32;
		case WebAssembly::ResultType::f64: return TypeId::f64;
		default: throw;
		};
	}
}