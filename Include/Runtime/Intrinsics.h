#pragma once

#include "Inline/BasicTypes.h"
#include "IR/IR.h"
#include "IR/TaggedValue.h"
#include "Runtime.h"

namespace Intrinsics
{
	struct ModuleImpl;

	struct Module
	{
		ModuleImpl* impl = nullptr;

		RUNTIME_API ~Module();
	};

	RUNTIME_API Runtime::ModuleInstance* instantiateModule(
		Runtime::Compartment* compartment,
		const Intrinsics::Module& moduleRef);

	// An intrinsic function.
	struct Function
	{
		RUNTIME_API Function(
			Intrinsics::Module& moduleRef,
			const char* inName,
			const IR::FunctionType* type,
			void* inNativeFunction,
			Runtime::CallingConvention inCallingConvention = Runtime::CallingConvention::intrinsic);
		RUNTIME_API Runtime::FunctionInstance* instantiate(Runtime::Compartment* compartment);

	private:
		const char* name;
		const IR::FunctionType* type;
		void* nativeFunction;
		Runtime::CallingConvention callingConvention;
	};
	
	// The base class of Intrinsic globals.
	struct Global
	{
		RUNTIME_API Global(
			Intrinsics::Module& moduleRef,
			const char* inName,
			IR::ValueType inType,
			Runtime::Value inValue);
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

		GenericGlobal(Intrinsics::Module& moduleRef,const char* inName,Value inValue)
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
		RUNTIME_API Memory(
			Intrinsics::Module& moduleRef,
			const char* inName,
			const IR::MemoryType& inType);
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
		RUNTIME_API Table(
			Intrinsics::Module& moduleRef,
			const char* inName,
			const IR::TableType& inType);
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
	typedef V128 v128;
	typedef void none;
};

#define SET_INTRINSIC_RESULT_i32(value) I32 result = value; *(I32*)contextRuntimeData = result;
#define SET_INTRINSIC_RESULT_i64(value) I64 result = value; *(I64*)contextRuntimeData = result;
#define SET_INTRINSIC_RESULT_f32(value) F32 result = value; *(F32*)contextRuntimeData = result;
#define SET_INTRINSIC_RESULT_f64(value) F64 result = value; *(F64*)contextRuntimeData = result;
#define SET_INTRINSIC_RESULT_v128(value) V128 result = value; *(V128*)contextRuntimeData = result;
#define SET_INTRINSIC_RESULT_none(value) value;

#define DEFINE_INTRINSIC_MODULE(name) \
	Intrinsics::Module& getIntrinsicModule_##name() \
	{ \
		static Intrinsics::Module module; \
		return module; \
	}

#define DECLARE_INTRINSIC_MODULE(name) \
	extern Intrinsics::Module& getIntrinsicModule_##name();

#define INTRINSIC_MODULE_REF(name) getIntrinsicModule_##name()

// Macros for defining intrinsic functions of various arities.
#define DEFINE_INTRINSIC_FUNCTION0(module,cName,name,returnType) \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData*); \
	static Intrinsics::Function cName##Intrinsic(getIntrinsicModule_##module(),#name,IR::FunctionType::get(IR::ResultType::returnType),(void*)&cName); \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData* contextRuntimeData)

#define DEFINE_INTRINSIC_FUNCTION1(module,cName,name,returnType,arg0Type,arg0Name) \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData*,NativeTypes::arg0Type); \
	static Intrinsics::Function cName##Intrinsic(getIntrinsicModule_##module(),#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type}),(void*)&cName); \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData* contextRuntimeData,NativeTypes::arg0Type arg0Name)

#define DEFINE_INTRINSIC_FUNCTION2(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name) \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData*,NativeTypes::arg0Type,NativeTypes::arg1Type); \
	static Intrinsics::Function cName##Intrinsic(getIntrinsicModule_##module(),#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type}),(void*)&cName); \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData* contextRuntimeData,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name)

#define DEFINE_INTRINSIC_FUNCTION3(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name) \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData*,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type); \
	static Intrinsics::Function cName##Intrinsic(getIntrinsicModule_##module(),#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type}),(void*)&cName); \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData* contextRuntimeData,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name)

#define DEFINE_INTRINSIC_FUNCTION4(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name) \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData*,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type); \
	static Intrinsics::Function cName##Intrinsic(getIntrinsicModule_##module(),#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type,IR::ValueType::arg3Type}),(void*)&cName); \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData* contextRuntimeData,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name)

#define DEFINE_INTRINSIC_FUNCTION5(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name) \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData*,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type,NativeTypes::arg4Type); \
	static Intrinsics::Function cName##Intrinsic(getIntrinsicModule_##module(),#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type,IR::ValueType::arg3Type,IR::ValueType::arg4Type}),(void*)&cName); \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData* contextRuntimeData,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name,NativeTypes::arg4Type arg4Name)

#define DEFINE_INTRINSIC_FUNCTION6(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name,arg5Type,arg5Name) \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData*,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type,NativeTypes::arg4Type,NativeTypes::arg5Type); \
	static Intrinsics::Function cName##Intrinsic(getIntrinsicModule_##module(),#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type,IR::ValueType::arg3Type,IR::ValueType::arg4Type,IR::ValueType::arg5Type}),(void*)&cName); \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData* contextRuntimeData,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name,NativeTypes::arg4Type arg4Name,NativeTypes::arg5Type arg5Name)

#define DEFINE_INTRINSIC_FUNCTION7(module,cName,name,returnType,arg0Type,arg0Name,arg1Type,arg1Name,arg2Type,arg2Name,arg3Type,arg3Name,arg4Type,arg4Name,arg5Type,arg5Name,arg6Type,arg6Name) \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData*,NativeTypes::arg0Type,NativeTypes::arg1Type,NativeTypes::arg2Type,NativeTypes::arg3Type,NativeTypes::arg4Type,NativeTypes::arg5Type,NativeTypes::arg6Type); \
	static Intrinsics::Function cName##Intrinsic(getIntrinsicModule_##module(),#name,IR::FunctionType::get(IR::ResultType::returnType,{IR::ValueType::arg0Type,IR::ValueType::arg1Type,IR::ValueType::arg2Type,IR::ValueType::arg3Type,IR::ValueType::arg4Type,IR::ValueType::arg5Type,IR::ValueType::arg6Type}),(void*)&cName); \
	NativeTypes::returnType cName(Runtime::ContextRuntimeData* contextRuntimeData,NativeTypes::arg0Type arg0Name,NativeTypes::arg1Type arg1Name,NativeTypes::arg2Type arg2Name,NativeTypes::arg3Type arg3Name,NativeTypes::arg4Type arg4Name,NativeTypes::arg5Type arg5Name,NativeTypes::arg6Type arg6Name)

// Macros for defining intrinsic globals, memories, and tables.
#define DEFINE_INTRINSIC_GLOBAL(module,cName,name,valueType,initializer) \
	static Intrinsics::GenericGlobal<IR::ValueType::valueType> cName(getIntrinsicModule_##module(),#name,initializer);

#define DEFINE_INTRINSIC_MEMORY(module,cName,name,type) static Intrinsics::Memory cName(getIntrinsicModule_##module(),#name,type);
#define DEFINE_INTRINSIC_TABLE(module,cName,name,type) static Intrinsics::Table cName(getIntrinsicModule_##module(),#name,type);