#pragma once

#include <initializer_list>
#include <string>
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Runtime/Runtime.h"

namespace WAVM { namespace Runtime {
	struct ContextRuntimeData;
}}

namespace WAVM { namespace Intrinsics {
	struct ModuleImpl;
	struct Function;

	struct Module
	{
		ModuleImpl* impl = nullptr;

		WAVM_API ~Module();
	};

	WAVM_API Runtime::ModuleInstance* instantiateModule(
		Runtime::Compartment* compartment,
		const std::initializer_list<const Intrinsics::Module*>& moduleRefs,
		std::string&& debugName,
		const HashMap<std::string, Runtime::Object*>& extraExports = {});

	// An intrinsic function.
	struct Function
	{
		WAVM_API Function(Intrinsics::Module* moduleRef,
						  const char* inName,
						  void* inNativeFunction,
						  IR::FunctionType type,
						  IR::CallingConvention inCallingConvention);
		WAVM_API Runtime::Function* instantiate(Runtime::Compartment* compartment);

		void* getNativeFunction() const { return nativeFunction; }
		IR::CallingConvention getCallingConvention() const { return callingConvention; }

	private:
		const char* name;
		IR::FunctionType type;
		void* nativeFunction;
		IR::CallingConvention callingConvention;
	};

	// The base class of Intrinsic globals.
	struct Global
	{
		WAVM_API Global(Intrinsics::Module* moduleRef,
						const char* inName,
						IR::ValueType inType,
						IR::Value inValue);
		WAVM_API Runtime::Global* instantiate(Runtime::Compartment* compartment);

		IR::Value getValue() const { return value; }

	private:
		const char* name;
		IR::ValueType type;
		IR::Value value;
	};

	// An immutable global that provides typed initialization and reading of the global's value.
	template<typename Value> struct GenericGlobal : Global
	{
		GenericGlobal(Intrinsics::Module* moduleRef, const char* inName, Value inValue)
		: Global(moduleRef,
				 inName,
				 IR::inferValueType<Value>(),
				 IR::Value(IR::inferValueType<Value>(), inValue))
		{
		}
	};

	// Intrinsic memories and tables
	struct Memory
	{
		WAVM_API
		Memory(Intrinsics::Module* moduleRef, const char* inName, const IR::MemoryType& inType);
		WAVM_API Runtime::Memory* instantiate(Runtime::Compartment* compartment);

		Runtime::Memory* getInstance(Runtime::ModuleInstance* moduleInstance)
		{
			return asMemory(Runtime::getInstanceExport(moduleInstance, name));
		}

	private:
		const char* name;
		const IR::MemoryType type;
	};

	struct Table
	{
		WAVM_API
		Table(Intrinsics::Module* moduleRef, const char* inName, const IR::TableType& inType);
		WAVM_API Runtime::Table* instantiate(Runtime::Compartment* compartment);

		Runtime::Table* getInstance(Runtime::ModuleInstance* moduleInstance)
		{
			return asTable(Runtime::getInstanceExport(moduleInstance, name));
		}

	private:
		const char* name;
		const IR::TableType type;
	};

	// Create a new return type for intrinsic functions that return their result in the
	// ContextRuntimeData buffer.
	template<typename Result> struct ResultInContextRuntimeData;

	template<typename Result>
	ResultInContextRuntimeData<Result>* resultInContextRuntimeData(
		Runtime::ContextRuntimeData* contextRuntimeData,
		Result result)
	{
		*reinterpret_cast<Result*>(contextRuntimeData) = result;
		return reinterpret_cast<ResultInContextRuntimeData<Result>*>(contextRuntimeData);
	}

	template<typename R, typename... Args>
	IR::FunctionType inferIntrinsicFunctionType(R (*)(Runtime::ContextRuntimeData*, Args...))
	{
		return IR::FunctionType(IR::inferResultType<R>(),
								IR::TypeTuple({IR::inferValueType<Args>()...}));
	}
	template<typename R, typename... Args>
	IR::FunctionType inferIntrinsicWithContextSwitchFunctionType(
		ResultInContextRuntimeData<R>* (*)(Runtime::ContextRuntimeData*, Args...))
	{
		return IR::FunctionType(IR::inferResultType<R>(),
								IR::TypeTuple({IR::inferValueType<Args>()...}));
	}
}}

#define WAVM_DEFINE_INTRINSIC_MODULE(name)                                                         \
	Intrinsics::Module* getIntrinsicModule_##name()                                                \
	{                                                                                              \
		static Intrinsics::Module module;                                                          \
		return &module;                                                                            \
	}

#define WAVM_DECLARE_INTRINSIC_MODULE(name) extern Intrinsics::Module* getIntrinsicModule_##name();

#define WAVM_INTRINSIC_MODULE_REF(name) getIntrinsicModule_##name()

#define WAVM_DEFINE_INTRINSIC_FUNCTION(module, nameString, Result, cName, ...)                     \
	static Result cName(Runtime::ContextRuntimeData* contextRuntimeData, ##__VA_ARGS__);           \
	static Intrinsics::Function cName##Intrinsic(getIntrinsicModule_##module(),                    \
												 nameString,                                       \
												 (void*)&cName,                                    \
												 Intrinsics::inferIntrinsicFunctionType(&cName),   \
												 IR::CallingConvention::intrinsic);                \
	static Result cName(Runtime::ContextRuntimeData* contextRuntimeData, ##__VA_ARGS__)

#define WAVM_DEFINE_INTRINSIC_FUNCTION_WITH_CONTEXT_SWITCH(module, nameString, Result, cName, ...) \
	static Intrinsics::ResultInContextRuntimeData<Result>* cName(                                  \
		Runtime::ContextRuntimeData* contextRuntimeData, ##__VA_ARGS__);                           \
	static Intrinsics::Function cName##Intrinsic(                                                  \
		getIntrinsicModule_##module(),                                                             \
		nameString,                                                                                \
		(void*)&cName,                                                                             \
		Intrinsics::inferIntrinsicWithContextSwitchFunctionType(&cName),                           \
		IR::CallingConvention::intrinsicWithContextSwitch);                                        \
	static Intrinsics::ResultInContextRuntimeData<Result>* cName(                                  \
		Runtime::ContextRuntimeData* contextRuntimeData, ##__VA_ARGS__)

// Macros for defining intrinsic globals, memories, and tables.
#define WAVM_DEFINE_INTRINSIC_GLOBAL(module, name, Value, cName, initializer)                      \
	static Intrinsics::GenericGlobal<Value> cName(getIntrinsicModule_##module(), name, initializer);

#define WAVM_DEFINE_INTRINSIC_MEMORY(module, cName, name, type)                                    \
	static Intrinsics::Memory cName(getIntrinsicModule_##module(), #name, type);
#define WAVM_DEFINE_INTRINSIC_TABLE(module, cName, name, type)                                     \
	static Intrinsics::Table cName(getIntrinsicModule_##module(), #name, type);
