#pragma once

#include "Core/Core.h"
#include "WebAssembly/WebAssembly.h"
#include "TaggedValue.h"
#include "Runtime.h"

namespace Intrinsics
{
	struct Function
	{
		Runtime::FunctionInstance* function;

		RUNTIME_API Function(const char* inName,const WebAssembly::FunctionType* type,void* nativeFunction);
		RUNTIME_API ~Function();

	private:
		const char* name;
	};
	
	struct Variable
	{
		Runtime::GlobalInstance* global;

		RUNTIME_API Variable(const char* inName,WebAssembly::GlobalType inType);
		RUNTIME_API ~Variable();

	protected:
		void* value;
	private:
		const char* name;
	};
	
	template<WebAssembly::ValueType type,bool isMutable>
	struct GenericVariable : Variable
	{
		typedef typename WebAssembly::ValueTypeInfo<type>::Value Value;

		GenericVariable(const char* inName,WebAssembly::GlobalType inType,Value inValue)
		: Variable(inName,inType) { *(Value*)value = inValue; }

		operator Value() const { return *(Value*)value; }
		void operator=(Value newValue) { *(Value*)value = newValue; }
	};
	template<WebAssembly::ValueType type>
	struct GenericVariable<type,false> : Variable
	{
		typedef typename WebAssembly::ValueTypeInfo<type>::Value Value;

		GenericVariable(const char* inName,WebAssembly::GlobalType inType,Value inValue)
		: Variable(inName,inType) { *(Value*)value = inValue; }

		operator Value() const { return *(Value*)value; }
	};

	struct Memory
	{
		RUNTIME_API Memory(const char* inName,const WebAssembly::MemoryType& inType);
		RUNTIME_API ~Memory();

		operator Runtime::Memory*() const { return memory; }

	private:
		const char* name;
		Runtime::Memory* const memory;
	};

	struct Table
	{
		RUNTIME_API Table(const char* inName,const WebAssembly::TableType& inType);
		RUNTIME_API ~Table();
		
		operator Runtime::Table*() const { return table; }

	private:
		const char* name;
		Runtime::Table* const table;
	};
	
	RUNTIME_API std::string getDecoratedName(const char* name,const WebAssembly::ObjectType& type);
	RUNTIME_API Runtime::Object* find(const char* decoratedName,const WebAssembly::ObjectType& type);
}

#define DEFINE_INTRINSIC_FUNCTION0(module,cName,name,returnType) \
	Runtime::NativeTypes::returnType cName##returnType(); \
	static Intrinsics::Function cName##returnType##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ReturnType::returnType),(void*)&cName##returnType); \
	Runtime::NativeTypes::returnType cName##returnType()

#define DEFINE_INTRINSIC_FUNCTION1(module,cName,name,returnType,arg0Type,arg0Name) \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type(Runtime::NativeTypes::arg0Type); \
	static Intrinsics::Function cName##returnType##arg0Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ReturnType::returnType,{WebAssembly::ValueType::arg0Type}),(void*)&cName##returnType##arg0Type); \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type(Runtime::NativeTypes::arg0Type arg0Name)

#define DEFINE_INTRINSIC_FUNCTION2(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name) \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type##arg1Type(Runtime::NativeTypes::arg0Type,Runtime::NativeTypes::arg1Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ReturnType::returnType,{WebAssembly::ValueType::arg0Type,WebAssembly::ValueType::arg1Type}),(void*)&cName##returnType##arg0Type##arg1Type); \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type##arg1Type(Runtime::NativeTypes::arg0Type arg0Name,Runtime::NativeTypes::arg1Type arg1Name)

#define DEFINE_INTRINSIC_FUNCTION3(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name) \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type(Runtime::NativeTypes::arg0Type,Runtime::NativeTypes::arg1Type,Runtime::NativeTypes::arg2Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ReturnType::returnType,{WebAssembly::ValueType::arg0Type,WebAssembly::ValueType::arg1Type,WebAssembly::ValueType::arg2Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type); \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type(Runtime::NativeTypes::arg0Type arg0Name,Runtime::NativeTypes::arg1Type arg1Name,Runtime::NativeTypes::arg2Type arg2Name)

#define DEFINE_INTRINSIC_FUNCTION4(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name) \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type(Runtime::NativeTypes::arg0Type,Runtime::NativeTypes::arg1Type,Runtime::NativeTypes::arg2Type,Runtime::NativeTypes::arg3Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ReturnType::returnType,{WebAssembly::ValueType::arg0Type,WebAssembly::ValueType::arg1Type,WebAssembly::ValueType::arg2Type,WebAssembly::ValueType::arg3Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type); \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type(Runtime::NativeTypes::arg0Type arg0Name,Runtime::NativeTypes::arg1Type arg1Name,Runtime::NativeTypes::arg2Type arg2Name,Runtime::NativeTypes::arg3Type arg3Name)

#define DEFINE_INTRINSIC_FUNCTION5(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name) \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type(Runtime::NativeTypes::arg0Type,Runtime::NativeTypes::arg1Type,Runtime::NativeTypes::arg2Type,Runtime::NativeTypes::arg3Type,Runtime::NativeTypes::arg4Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ReturnType::returnType,{WebAssembly::ValueType::arg0Type,WebAssembly::ValueType::arg1Type,WebAssembly::ValueType::arg2Type,WebAssembly::ValueType::arg3Type,WebAssembly::ValueType::arg4Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type); \
	Runtime::NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type(Runtime::NativeTypes::arg0Type arg0Name,Runtime::NativeTypes::arg1Type arg1Name,Runtime::NativeTypes::arg2Type arg2Name,Runtime::NativeTypes::arg3Type arg3Name,Runtime::NativeTypes::arg4Type arg4Name)

#define DEFINE_INTRINSIC_VARIABLE(module,cName,name,valueType,isMutable,initializer) \
	static Intrinsics::GenericVariable<WebAssembly::ValueType::valueType,isMutable> \
		cName(#module "." #name,{WebAssembly::ValueType::valueType,isMutable},initializer);

#define DEFINE_INTRINSIC_MEMORY(module,cName,name,type) static Intrinsics::Memory cName(#module "." #name,type);
#define DEFINE_INTRINSIC_TABLE(module,cName,name,type) static Intrinsics::Table cName(#module "." #name,type);