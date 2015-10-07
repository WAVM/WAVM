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

	std::string getDecoratedFunctionName(const char* name,const AST::FunctionType& type);
	std::string getDecoratedValueName(const char* name,AST::TypeId type);

	const Function* findFunction(const char* decoratedName);
	const Value* findValue(const char* decoratedName);
}

#define DEFINE_INTRINSIC_FUNCTION0(module,name,returnType) \
	AST::NativeTypes::returnType name##returnType(); \
	static Intrinsics::Function name##returnType##Function(#module "." #name,AST::FunctionType(AST::TypeId::returnType),(void*)&name##returnType); \
	AST::NativeTypes::returnType name##returnType()

#define DEFINE_INTRINSIC_FUNCTION1(module,name,returnType,arg0Type,arg0Name) \
	AST::NativeTypes::returnType name##returnType##arg0Type(AST::NativeTypes::arg0Type); \
	static Intrinsics::Function name##returnType##arg0Type##Function(#module "." #name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type}),(void*)&name##returnType##arg0Type); \
	AST::NativeTypes::returnType name##returnType##arg0Type(AST::NativeTypes::arg0Type arg0Name)

#define DEFINE_INTRINSIC_FUNCTION2(module,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name) \
	AST::NativeTypes::returnType name##returnType##arg0Type##arg1Type(AST::NativeTypes::arg0Type,AST::NativeTypes::arg1Type); \
	static Intrinsics::Function name##returnType##arg0Type##arg1Type##Function(#module "." #name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type,AST::TypeId::arg1Type}),(void*)&name##returnType##arg0Type##arg1Type); \
	AST::NativeTypes::returnType name##returnType##arg0Type##arg1Type(AST::NativeTypes::arg0Type arg0Name,AST::NativeTypes::arg1Type arg1Name)

#define DEFINE_INTRINSIC_FUNCTION3(module,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name) \
	AST::NativeTypes::returnType name##returnType##arg0Type##arg1Type##arg2Type(AST::NativeTypes::arg0Type,AST::NativeTypes::arg1Type,AST::NativeTypes::arg2Type); \
	static Intrinsics::Function name##returnType##arg0Type##arg1Type##arg2Type##Function(#module "." #name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type,AST::TypeId::arg1Type,AST::TypeId::arg2Type}),(void*)&name##returnType##arg0Type##arg1Type##arg2Type); \
	AST::NativeTypes::returnType name##returnType##arg0Type##arg1Type##arg2Type(AST::NativeTypes::arg0Type arg0Name,AST::NativeTypes::arg1Type arg1Name,AST::NativeTypes::arg2Type arg2Name)

#define DEFINE_INTRINSIC_FUNCTION4(module,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name) \
	AST::NativeTypes::returnType name##returnType##arg0Type##arg1Type##arg2Type##arg3Type(AST::NativeTypes::arg0Type,AST::NativeTypes::arg1Type,AST::NativeTypes::arg2Type,AST::NativeTypes::arg3Type); \
	static Intrinsics::Function name##returnType##arg0Type##arg1Type##arg2Type##arg3Type##Function(#module "." #name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type,AST::TypeId::arg1Type,AST::TypeId::arg2Type,AST::TypeId::arg3Type}),(void*)&name##returnType##arg0Type##arg1Type##arg2Type##arg3Type); \
	AST::NativeTypes::returnType name##returnType##arg0Type##arg1Type##arg2Type##arg3Type(AST::NativeTypes::arg0Type arg0Name,AST::NativeTypes::arg1Type arg1Name,AST::NativeTypes::arg2Type arg2Name,AST::NativeTypes::arg3Type arg3Name)

#define DEFINE_INTRINSIC_FUNCTION5(module,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name) \
	AST::NativeTypes::returnType name##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type(AST::NativeTypes::arg0Type,AST::NativeTypes::arg1Type,AST::NativeTypes::arg2Type,AST::NativeTypes::arg3Type,AST::NativeTypes::arg4Type); \
	static Intrinsics::Function name##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##Function(#module "." #name,AST::FunctionType(AST::TypeId::returnType,{AST::TypeId::arg0Type,AST::TypeId::arg1Type,AST::TypeId::arg2Type,AST::TypeId::arg3Type,AST::TypeId::arg4Type}),(void*)&name##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type); \
	AST::NativeTypes::returnType name##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type(AST::NativeTypes::arg0Type arg0Name,AST::NativeTypes::arg1Type arg1Name,AST::NativeTypes::arg2Type arg2Name,AST::NativeTypes::arg3Type arg3Name,AST::NativeTypes::arg4Type arg4Name)

#define DEFINE_INTRINSIC_VALUE(module,name,type,initializer) \
	AST::NativeTypes::type name initializer; \
	static Intrinsics::Value name##IntrinsicValue(#module "." #name,AST::TypeId::type,(void*)&name); \
	DEFINE_INTRINSIC_FUNCTION0(module,get_##name,type) \
	{ \
		return name; \
	} \
	DEFINE_INTRINSIC_FUNCTION1(module,set_##name,type,type,newValue) \
	{ \
		name = newValue; \
		return newValue; \
	}
