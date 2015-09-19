#pragma once

#include "Core/Core.h"
#include "AST/AST.h"

namespace Intrinsics
{
	struct Function
	{
		const char* name;
		AST::FunctionType type;
		void* value;

		Function(const char* inName,const AST::FunctionType& inType,void* inValue);
		~Function();
	};

	struct Value
	{
		const char* name;
		AST::TypeId type;
		void* value;

		Value(const char* inName,AST::TypeId inType,void* inValue);
		~Value();
	};

	const Function* findFunction(const char* name);
	const Value* findValue(const char* name);
}

#define DEFINE_INTRINSIC_FUNCTION0(name,returnType) \
	AST::NativeTypes::returnType name##IntrinsicFunc(); \
	static Intrinsics::Function name##Intrinsic(#name,AST::FunctionType(AST::TypeId::returnType),(void*)&name##IntrinsicFunc); \
	AST::NativeTypes::returnType name##IntrinsicFunc()

#define DEFINE_INTRINSIC_FUNCTION1(name,returnType,arg0Type,arg0Name) \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type); \
	static Intrinsics::Function name##Intrinsic(#name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type}),(void*)&name##IntrinsicFunc); \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type arg0Name)

#define DEFINE_INTRINSIC_FUNCTION2(name,returnType,arg0Type,arg0Name,arg1Type,arg1Name) \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type,AST::NativeTypes::arg1Type); \
	static Intrinsics::Function name##Function(#name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type,AST::TypeId::arg1Type}),(void*)&name##IntrinsicFunc); \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type arg0Name,AST::NativeTypes::arg1Type arg1Name)

#define DEFINE_INTRINSIC_FUNCTION3(name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name) \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type,AST::NativeTypes::arg1Type,AST::NativeTypes::arg2Type); \
	static Intrinsics::Function name##Function(#name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type,AST::TypeId::arg1Type,AST::TypeId::arg2Type}),(void*)&name##IntrinsicFunc); \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type arg0Name,AST::NativeTypes::arg1Type arg1Name,AST::NativeTypes::arg2Type arg2Name)

#define DEFINE_INTRINSIC_FUNCTION4(name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name) \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type,AST::NativeTypes::arg1Type,AST::NativeTypes::arg2Type,AST::NativeTypes::arg3Type); \
	static Intrinsics::Function name##Function(#name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type,AST::TypeId::arg1Type,AST::TypeId::arg2Type,AST::TypeId::arg3Type}),(void*)&name##IntrinsicFunc); \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type arg0Name,AST::NativeTypes::arg1Type arg1Name,AST::NativeTypes::arg2Type arg2Name,AST::NativeTypes::arg3Type arg3Name)

#define DEFINE_INTRINSIC_FUNCTION5(name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name) \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type,AST::NativeTypes::arg1Type,AST::NativeTypes::arg2Type,AST::NativeTypes::arg3Type,AST::NativeTypes::arg4Type); \
	static Intrinsics::Function name##Function(#name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type,AST::TypeId::arg1Type,AST::TypeId::arg2Type,AST::TypeId::arg3Type,AST::TypeId::arg4Type}),(void*)&name##IntrinsicFunc); \
	AST::NativeTypes::returnType name##IntrinsicFunc(AST::NativeTypes::arg0Type arg0Name,AST::NativeTypes::arg1Type arg1Name,AST::NativeTypes::arg2Type arg2Name,AST::NativeTypes::arg3Type arg3Name,AST::NativeTypes::arg4Type arg4Name)

#define DEFINE_INTRINSIC_VALUE(name,type,initializer) \
	AST::NativeTypes::type name##Value initializer; \
	static Intrinsics::Value name##IntrinsicValue(#name,AST::TypeId::type,(void*)&name##Value); \
	DEFINE_INTRINSIC_FUNCTION0(get_##name,type) \
	{ \
		return name##Value; \
	} \
	DEFINE_INTRINSIC_FUNCTION1(set_##name,type,type,newValue) \
	{ \
		name##Value = newValue; \
		return newValue; \
	}