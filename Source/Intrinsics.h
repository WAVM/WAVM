#pragma once

#include "WAVM.h"
#include "WASM.h"

namespace WAVM
{
	namespace IntrinsicTypeDefs
	{
		typedef uint32_t I32;
		typedef float F32;
		typedef double F64;
		typedef void Void;
	}

	struct IntrinsicFunction
	{
		const char* name;
		FunctionType type;
		void* value;

		IntrinsicFunction(const char* inName,FunctionType inType,void* inValue);
		~IntrinsicFunction();
	};

	struct IntrinsicValue
	{
		const char* name;
		WASM::Type type;
		void* value;

		IntrinsicValue(const char* inName,WASM::Type inType,void* inValue);
		~IntrinsicValue();
	};

	const IntrinsicFunction* findIntrinsicFunction(const char* name);
	const IntrinsicValue* findIntrinsicValue(const char* name);
}

#define DEFINE_INTRINSIC_FUNCTION0(name,returnType) \
	WAVM::IntrinsicTypeDefs::returnType __cdecl name##IntrinsicFunc(); \
	static WAVM::IntrinsicFunction name##Intrinsic(#name,WAVM::FunctionType(WASM::ReturnType::returnType),(void*)&name##IntrinsicFunc); \
	WAVM::IntrinsicTypeDefs::returnType __cdecl name##IntrinsicFunc()

#define DEFINE_INTRINSIC_FUNCTION1(name,returnType,arg0Type,arg0Name) \
	WAVM::IntrinsicTypeDefs::returnType __cdecl name##IntrinsicFunc(WAVM::IntrinsicTypeDefs::arg0Type); \
	static WAVM::IntrinsicFunction name##Intrinsic(#name,WAVM::FunctionType(WASM::ReturnType::returnType,{WASM::Type::arg0Type}),(void*)&name##IntrinsicFunc); \
	WAVM::IntrinsicTypeDefs::returnType __cdecl name##IntrinsicFunc(WAVM::IntrinsicTypeDefs::arg0Type arg0Name)

#define DEFINE_INTRINSIC_FUNCTION2(name,returnType,arg0Type,arg0Name,arg1Type,arg1Name) \
	WAVM::IntrinsicTypeDefs::returnType __cdecl name##IntrinsicFunc(WAVM::IntrinsicTypeDefs::arg0Type,WAVM::IntrinsicTypeDefs::arg1Type); \
	static WAVM::IntrinsicFunction name##Function(#name,WAVM::FunctionType(WASM::ReturnType::returnType,{WASM::Type::arg0Type,WASM::Type::arg1Type}),(void*)&name##IntrinsicFunc); \
	WAVM::IntrinsicTypeDefs::returnType __cdecl name##IntrinsicFunc(WAVM::IntrinsicTypeDefs::arg0Type arg0Name,WAVM::IntrinsicTypeDefs::arg1Type arg1Name)

#define DEFINE_INTRINSIC_FUNCTION3(name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name) \
	WAVM::IntrinsicTypeDefs::returnType __cdecl name##IntrinsicFunc(WAVM::IntrinsicTypeDefs::arg0Type,WAVM::IntrinsicTypeDefs::arg1Type,WAVM::IntrinsicTypeDefs::arg2Type); \
	static WAVM::IntrinsicFunction name##Function(#name,WAVM::FunctionType(WASM::ReturnType::returnType,{WASM::Type::arg0Type,WASM::Type::arg1Type,WASM::Type::arg2Type}),(void*)&name##IntrinsicFunc); \
	WAVM::IntrinsicTypeDefs::returnType __cdecl name##IntrinsicFunc(WAVM::IntrinsicTypeDefs::arg0Type arg0Name,WAVM::IntrinsicTypeDefs::arg1Type arg1Name,WAVM::IntrinsicTypeDefs::arg2Type arg2Name)

#define DEFINE_INTRINSIC_VALUE(name,type,initializer) \
	WAVM::IntrinsicTypeDefs::type name##Value initializer; \
	static WAVM::IntrinsicValue name##IntrinsicValue(#name,WASM::Type::type,(void*)&name##Value);