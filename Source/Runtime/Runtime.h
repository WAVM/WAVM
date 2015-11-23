#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "AST/AST.h"

#ifndef RUNTIME_API
	#define RUNTIME_API DLL_IMPORT
#endif

namespace Runtime
{
	// Information about a runtime exception.
	struct Exception
	{
		enum class Cause : uint8
		{
			Unknown,
			AccessViolation,
			StackOverflow,
			IntegerDivideByZeroOrIntegerOverflow,
			InvalidFloatOperation,
			InvokeSignatureMismatch,
			OutOfMemory,
			GrowMemoryNotPageAligned,
			ReachedUnreachable
		};

		Cause cause;
		std::vector<std::string> callStack;		
	};
	
	// Used to represent a void runtime value.
	struct Void {};

	// The type of a runtime value.
	enum class TypeId : uint8
	{
		None = (uint8)AST::TypeId::None,
		I8 = (uint8)AST::TypeId::I8,
		I16 = (uint8)AST::TypeId::I16,
		I32 = (uint8)AST::TypeId::I32,
		I64 = (uint8)AST::TypeId::I64,
		F32 = (uint8)AST::TypeId::F32,
		F64 = (uint8)AST::TypeId::F64,
		Bool = (uint8)AST::TypeId::Bool,
		Void = (uint8)AST::TypeId::Void,
		Exception
	};

	// A boxed value: may hold any value that can be passed to, or returned from a function invoked through the runtime.
	struct Value
	{
		union
		{
			uint8 i8;
			uint16 i16;
			uint32 i32;
			uint64 i64;
			float32 f32;
			float64 f64;
			bool bool_;
			Void void_;
			Exception* exception;
		};
		TypeId type;

		Value(uint8 inI8): i8(inI8), type(TypeId::I8) {}
		Value(uint16 inI16): i16(inI16), type(TypeId::I16) {}
		Value(uint32 inI32): i32(inI32), type(TypeId::I32) {}
		Value(uint64 inI64): i64(inI64), type(TypeId::I64) {}
		Value(float32 inF32): f32(inF32), type(TypeId::F32) {}
		Value(float64 inF64): f64(inF64), type(TypeId::F64) {}
		Value(bool inBool): bool_(inBool), type(TypeId::Bool) {}
		Value(Void inVoid): void_(inVoid), type(TypeId::Void) {}
		Value(): type(TypeId::None) {}
		Value(Exception* inException): exception(inException), type(TypeId::Exception) {}
	};

	// Initializes the runtime.
	RUNTIME_API bool init();

	// Adds a module to the instance.
	RUNTIME_API bool loadModule(const AST::Module* module);

	// Invokes a function with the provided boxed parameters.
	RUNTIME_API Value invokeFunction(const AST::Module* module,uintptr functionIndex,const Value* parameters);

	// Returns a string that describes the given exception cause.
	RUNTIME_API const char* describeExceptionCause(Runtime::Exception::Cause cause);
}
