#pragma once

#include "Inline/BasicTypes.h"
#include "Inline/HashMap.h"
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
		const Intrinsics::Module& moduleRef,
		std::string&& debugName,
		const HashMap<std::string, Runtime::Object*>& extraExports = {}
		);

	// An intrinsic function.
	struct Function
	{
		RUNTIME_API Function(
			Intrinsics::Module& moduleRef,
			const char* inName,
			void* inNativeFunction,
			const IR::FunctionType* type,
			Runtime::CallingConvention inCallingConvention);
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

		Runtime::Value getValue() const { return value; }

	private:
		const char* name;
		IR::ValueType type;
		Runtime::Value value;
	};
	
	// An immutable global that provides typed initialization and reading of the global's value.
	template<typename Value>
	struct GenericGlobal : Global
	{
		GenericGlobal(Intrinsics::Module& moduleRef,const char* inName,Value inValue)
		: Global(
			moduleRef, inName, IR::inferValueType<Value>(),
			Runtime::Value(IR::inferValueType<Value>(),inValue))
		{}
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

	// Create new types for the memory and table IDs implicitly passed to some intrinsics,
	// so inferFunctionType can recognize them
	struct MemoryIdArg { uintptr_t id; };
	struct TableIdArg { uintptr_t id; };

	// Create a new return type for intrinsic functions that return their result in the ContextRuntimeData buffer.
	template<typename Result>
	struct ResultInContextRuntimeData;

	template<typename Result>
	ResultInContextRuntimeData<Result>* resultInContextRuntimeData(
		Runtime::ContextRuntimeData* contextRuntimeData,
		Result result)
	{
		*reinterpret_cast<Result*>(contextRuntimeData) = result;
		return reinterpret_cast<ResultInContextRuntimeData<Result>*>(contextRuntimeData);
	}

	template<typename R,typename... Args>
	const IR::FunctionType* inferIntrinsicFunctionType(
		R (*)(Runtime::ContextRuntimeData*,Args...))
	{
		return IR::FunctionType::get(IR::inferResultType<R>(),{IR::inferValueType<Args>()...});
	}
	template<typename R,typename... Args>
	const IR::FunctionType* inferIntrinsicWithMemAndTableFunctionType(
		R (*)(Runtime::ContextRuntimeData*,MemoryIdArg,TableIdArg,Args...))
	{
		return IR::FunctionType::get(IR::inferResultType<R>(),{IR::inferValueType<Args>()...});
	}
	template<typename R,typename... Args>
	const IR::FunctionType* inferIntrinsicWithContextSwitchFunctionType(
		ResultInContextRuntimeData<R>* (*)(Runtime::ContextRuntimeData*,Args...))
	{
		return IR::FunctionType::get(IR::inferResultType<R>(),{IR::inferValueType<Args>()...});
	}

}

#define DEFINE_INTRINSIC_MODULE(name) \
	Intrinsics::Module& getIntrinsicModule_##name() \
	{ \
		static Intrinsics::Module module; \
		return module; \
	}

#define DECLARE_INTRINSIC_MODULE(name) \
	extern Intrinsics::Module& getIntrinsicModule_##name();

#define INTRINSIC_MODULE_REF(name) getIntrinsicModule_##name()

#define DEFINE_INTRINSIC_FUNCTION(module,nameString,Result,cName,...) \
	static Result cName(Runtime::ContextRuntimeData* contextRuntimeData, ##__VA_ARGS__); \
	static Intrinsics::Function cName##Intrinsic( \
		getIntrinsicModule_##module(), nameString, (void*)&cName, \
		Intrinsics::inferIntrinsicFunctionType(&cName), \
		Runtime::CallingConvention::intrinsic); \
	static Result cName(Runtime::ContextRuntimeData* contextRuntimeData, ##__VA_ARGS__)

#define DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(module,nameString,Result,cName,...) \
	static Result cName( \
		Runtime::ContextRuntimeData* contextRuntimeData, \
		Intrinsics::MemoryIdArg defaultMemoryId, Intrinsics::TableIdArg defaultTableId, \
		##__VA_ARGS__); \
	static Intrinsics::Function cName##Intrinsic( \
		getIntrinsicModule_##module(), nameString, (void*)&cName, \
		Intrinsics::inferIntrinsicWithMemAndTableFunctionType(&cName), \
		Runtime::CallingConvention::intrinsicWithMemAndTable); \
	static Result cName( \
		Runtime::ContextRuntimeData* contextRuntimeData, \
		Intrinsics::MemoryIdArg defaultMemoryId, Intrinsics::TableIdArg defaultTableId, \
		##__VA_ARGS__)

#define DEFINE_INTRINSIC_FUNCTION_WITH_CONTEXT_SWITCH(module,nameString,Result,cName,...) \
	static Intrinsics::ResultInContextRuntimeData<Result>* cName( \
		Runtime::ContextRuntimeData* contextRuntimeData, ##__VA_ARGS__); \
	static Intrinsics::Function cName##Intrinsic( \
		getIntrinsicModule_##module(), nameString, (void*)&cName, \
		Intrinsics::inferIntrinsicWithContextSwitchFunctionType(&cName), \
		Runtime::CallingConvention::intrinsicWithContextSwitch); \
	static Intrinsics::ResultInContextRuntimeData<Result>* cName( \
		Runtime::ContextRuntimeData* contextRuntimeData, ##__VA_ARGS__)

// Macros for defining intrinsic globals, memories, and tables.
#define DEFINE_INTRINSIC_GLOBAL(module,name,Value,cName,initializer) \
	static Intrinsics::GenericGlobal<Value> cName(getIntrinsicModule_##module(),name,initializer);

#define DEFINE_INTRINSIC_MEMORY(module,cName,name,type) static Intrinsics::Memory cName(getIntrinsicModule_##module(),#name,type);
#define DEFINE_INTRINSIC_TABLE(module,cName,name,type) static Intrinsics::Table cName(getIntrinsicModule_##module(),#name,type);