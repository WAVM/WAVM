#pragma once

#include "Core/Core.h"
#include "WebAssembly/WebAssembly.h"
#include "Runtime.h"

namespace Intrinsics
{
	// An intrinsic function.
	struct Function
	{
		Runtime::FunctionInstance* function;

		RUNTIME_API Function(const char* inName,const WebAssembly::FunctionType* type,void* nativeFunction);
		RUNTIME_API ~Function();

	private:
		const char* name;
	};
	
	// The base class of Intrinsic globals.
	struct Global
	{
		Runtime::GlobalInstance* global;

		RUNTIME_API Global(const char* inName,WebAssembly::GlobalType inType);
		RUNTIME_API ~Global();

	protected:
		void* value;
	private:
		const char* name;
	};
	
	// A partially specialized template for Intrinsic globals:
	// Provides access via implicit coercion to a value, and for mutable globals an assignment operator.
	template<WebAssembly::ValueType type,bool isMutable>
	struct GenericGlobal : Global
	{
		typedef typename WebAssembly::ValueTypeInfo<type>::Value Value;

		GenericGlobal(const char* inName,WebAssembly::GlobalType inType,Value inValue)
		: Global(inName,inType) { *(Value*)value = inValue; }

		operator Value() const { return *(Value*)value; }
		void operator=(Value newValue) { *(Value*)value = newValue; }
	};
	template<WebAssembly::ValueType type>
	struct GenericGlobal<type,false> : Global
	{
		typedef typename WebAssembly::ValueTypeInfo<type>::Value Value;

		GenericGlobal(const char* inName,WebAssembly::GlobalType inType,Value inValue)
		: Global(inName,inType) { *(Value*)value = inValue; }

		operator Value() const { return *(Value*)value; }
	};

	// Intrinsic memories and tables
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

	// Finds an intrinsic object by name and type.
	RUNTIME_API Runtime::Object* find(const char* name,const WebAssembly::ObjectType& type);

	// Returns an array of all intrinsic runtime Objects; used as roots for garbage collection.
	RUNTIME_API std::vector<Runtime::Object*> getAllIntrinsicObjects();
}

namespace NativeTypes
{
	typedef int32 i32;
	typedef int64 i64;
	typedef float32 f32;
	typedef float64 f64;
	typedef void none;
};

// Macros for defining intrinsic functions of various arities.
#define DEFINE_INTRINSIC_FUNCTION0(module,cName,name,returnType) \
	NativeTypes::returnType cName##returnType(); \
	static Intrinsics::Function cName##returnType##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ResultType::returnType),(void*)&cName##returnType); \
	NativeTypes::returnType cName##returnType()

#define DEFINE_INTRINSIC_FUNCTION1(module,cName,name,returnType,arg0Type,arg0Name) \
	NativeTypes::returnType cName##returnType##arg0Type(NativeTypes::arg0Type); \
	static Intrinsics::Function cName##returnType##arg0Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ResultType::returnType,{WebAssembly::ValueType::arg0Type}),(void*)&cName##returnType##arg0Type); \
	NativeTypes::returnType cName##returnType##arg0Type(NativeTypes::arg0Type arg0Name)

#define DEFINE_INTRINSIC_FUNCTION2(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type(NativeTypes::arg0Type,NativeTypes::arg1Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ResultType::returnType,{WebAssembly::ValueType::arg0Type,WebAssembly::ValueType::arg1Type}),(void*)&cName##returnType##arg0Type##arg1Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type(NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name)

#define DEFINE_INTRINSIC_FUNCTION3(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type(NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ResultType::returnType,{WebAssembly::ValueType::arg0Type,WebAssembly::ValueType::arg1Type,WebAssembly::ValueType::arg2Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type(NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name)

#define DEFINE_INTRINSIC_FUNCTION4(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type(NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ResultType::returnType,{WebAssembly::ValueType::arg0Type,WebAssembly::ValueType::arg1Type,WebAssembly::ValueType::arg2Type,WebAssembly::ValueType::arg3Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type(NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name)

#define DEFINE_INTRINSIC_FUNCTION5(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type(NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type,NativeTypes::arg4Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##Function(#module "." #name,WebAssembly::FunctionType::get(WebAssembly::ResultType::returnType,{WebAssembly::ValueType::arg0Type,WebAssembly::ValueType::arg1Type,WebAssembly::ValueType::arg2Type,WebAssembly::ValueType::arg3Type,WebAssembly::ValueType::arg4Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type(NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name,NativeTypes::arg4Type arg4Name)

// Macros for defining intrinsic globals, memories, and tables.
#define DEFINE_INTRINSIC_GLOBAL(module,cName,name,valueType,isMutable,initializer) \
	static Intrinsics::GenericGlobal<WebAssembly::ValueType::valueType,isMutable> \
		cName(#module "." #name,{WebAssembly::ValueType::valueType,isMutable},initializer);

#define DEFINE_INTRINSIC_MEMORY(module,cName,name,type) static Intrinsics::Memory cName(#module "." #name,type);
#define DEFINE_INTRINSIC_TABLE(module,cName,name,type) static Intrinsics::Table cName(#module "." #name,type);