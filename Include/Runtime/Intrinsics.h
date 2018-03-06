#pragma once

#include "Inline/BasicTypes.h"
#include "IR/IR.h"
#include "Runtime.h"
#include "TaggedValue.h"

namespace Runtime { struct ContextRuntimeData; }

namespace Intrinsics
{
	struct Module;

	RUNTIME_API Runtime::ModuleInstance* instantiateModule(Runtime::Compartment* compartment,Intrinsics::Module* module);

	// An intrinsic function.
	struct Function
	{
		RUNTIME_API Function(Intrinsics::Module*& moduleRef,const char* inName,const IR::FunctionType* type,void* inNativeFunction);
		RUNTIME_API Runtime::FunctionInstance* instantiate(Runtime::Compartment* compartment);

	private:
		const char* name;
		const IR::FunctionType* type;
		void* nativeFunction;
	};
	
	// The base class of Intrinsic globals.
	struct Global
	{
		RUNTIME_API Global(Intrinsics::Module*& moduleRef,const char* inName,IR::ValueType inType,Runtime::Value inValue);
		RUNTIME_API Runtime::GlobalInstance* instantiate(Runtime::Compartment* compartment);

		void setValue(Runtime::Value newValue) { value = newValue; }
		Runtime::Value getValue() const { return value; }

	private:
		const char* name;
		IR::ValueType type;
		Runtime::Value value;
	};
	
	// An immutable global that provides typed initialization and reading of the global's value.
	template<IR::ValueType inType>
	struct GenericGlobal : Global
	{
		typedef typename IR::ValueTypeInfo<inType>::Value Value;

		GenericGlobal(Intrinsics::Module*& moduleRef,const char* inName,Value inValue)
		: Global(moduleRef,inName,inType,Runtime::Value(inType,inValue))
		{}
		
		void setValue(Value inValue)
		{
			Global::setValue(Runtime::Value(inType,inValue));
		}
	};

	// Intrinsic memories and tables
	struct Memory
	{
		RUNTIME_API Memory(Intrinsics::Module*& moduleRef,const char* inName,const IR::MemoryType& inType);
		RUNTIME_API Runtime::MemoryInstance* instantiate(Runtime::Compartment* compartment);

		Runtime::MemoryInstance* getInstance(Runtime::ModuleInstance* moduleInstance)
		{
			return  asMemory(Runtime::getInstanceExport(moduleInstance,name));
		}

	private:
		const char* name;
		const IR::MemoryType type;
	};

	struct Table
	{
		RUNTIME_API Table(Intrinsics::Module*& moduleRef,const char* inName,const IR::TableType& inType);
		RUNTIME_API Runtime::TableInstance* instantiate(Runtime::Compartment* compartment);

		Runtime::TableInstance* getInstance(Runtime::ModuleInstance* moduleInstance)
		{
			return asTable(Runtime::getInstanceExport(moduleInstance,name));
		}

	private:
		const char* name;
		const IR::TableType type;
	};
}

namespace NativeTypes
{
	typedef I32 i32;
	typedef I64 i64;
	typedef F32 f32;
	typedef F64 f64;
	typedef void none;
	#if ENABLE_SIMD_PROTOTYPE
	typedef V128 v128;
	#endif
};

#define DEFINE_INTRINSIC_MODULE(cName) \
	Intrinsics::Module* cName = nullptr;

// Macros for defining intrinsic functions of various arities.
#define DEFINE_INTRINSIC_FUNCTION0(module,cName,name,returnType) \
	NativeTypes::returnType cName##returnType(ContextRuntimeData**); \
	static Intrinsics::Function cName##returnType##Function(module,#name,IR::FunctionType::get(IR::ResultType::returnType),(void*)&cName##returnType); \
	NativeTypes::returnType cName##returnType(ContextRuntimeData** _context)

#define DEFINE_INTRINSIC_FUNCTION1(module,cName,name,returnType,arg0Type,arg0Name) \
	NativeTypes::returnType cName##returnType##arg0Type(ContextRuntimeData**,NativeTypes::arg0Type); \
	static Intrinsics::Function cName##returnType##arg0Type##Function(module,#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type}),(void*)&cName##returnType##arg0Type); \
	NativeTypes::returnType cName##returnType##arg0Type(ContextRuntimeData** _context,NativeTypes::arg0Type arg0Name)

#define DEFINE_INTRINSIC_FUNCTION2(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type(ContextRuntimeData**,NativeTypes::arg0Type,NativeTypes::arg1Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##Function(module,#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type}),(void*)&cName##returnType##arg0Type##arg1Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type(ContextRuntimeData** _context,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name)

#define DEFINE_INTRINSIC_FUNCTION3(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type(ContextRuntimeData**,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##Function(module,#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type(ContextRuntimeData** _context,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name)

#define DEFINE_INTRINSIC_FUNCTION4(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type(ContextRuntimeData**,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##Function(module,#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type,IR::ValueType::arg3Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type(ContextRuntimeData** _context,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name)

#define DEFINE_INTRINSIC_FUNCTION5(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type(ContextRuntimeData**,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type,NativeTypes::arg4Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##Function(module,#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type,IR::ValueType::arg3Type,IR::ValueType::arg4Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type(ContextRuntimeData** _context,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name,NativeTypes::arg4Type arg4Name)

#define DEFINE_INTRINSIC_FUNCTION6(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name,arg5Type,arg5Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##arg5Type(ContextRuntimeData**,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type,NativeTypes::arg4Type,NativeTypes::arg5Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##arg5Type##Function(module,#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type,IR::ValueType::arg3Type,IR::ValueType::arg4Type,IR::ValueType::arg5Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##arg5Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##arg5Type(ContextRuntimeData** _context,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name,NativeTypes::arg4Type arg4Name,NativeTypes::arg5Type arg5Name)

#define DEFINE_INTRINSIC_FUNCTION7(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name,arg5Type,arg5Name,arg6Type,arg6Name) \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##arg5Type##arg6Type(ContextRuntimeData**,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type,NativeTypes::arg4Type,NativeTypes::arg5Type,NativeTypes::arg6Type); \
	static Intrinsics::Function cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##arg5Type##arg6Type##Function(module,#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type,IR::ValueType::arg3Type,IR::ValueType::arg4Type,IR::ValueType::arg5Type,IR::ValueType::arg6Type}),(void*)&cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##arg5Type##arg6Type); \
	NativeTypes::returnType cName##returnType##arg0Type##arg1Type##arg2Type##arg3Type##arg4Type##arg5Type##arg6Type(ContextRuntimeData** _context,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name,NativeTypes::arg4Type arg4Name,NativeTypes::arg5Type arg5Name,NativeTypes::arg6Type arg6Name)

// Macros for defining intrinsic globals, memories, and tables.
#define DEFINE_INTRINSIC_GLOBAL(module,cName,name,valueType,initializer) \
	static Intrinsics::GenericGlobal<IR::ValueType::valueType> cName(module,#name,initializer);

#define DEFINE_INTRINSIC_MEMORY(module,cName,name,type) static Intrinsics::Memory cName(module,#name,type);
#define DEFINE_INTRINSIC_TABLE(module,cName,name,type) static Intrinsics::Table cName(module,#name,type);