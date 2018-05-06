#include "Inline/BasicTypes.h"
#include "Inline/HashMap.h"
#include "Intrinsics.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

#include <string>

namespace Intrinsics
{
	struct ModuleImpl
	{
		HashMap<std::string, Intrinsics::Function*> functionMap;
		HashMap<std::string, Intrinsics::Global*> globalMap;
		HashMap<std::string, Intrinsics::Memory*> memoryMap;
		HashMap<std::string, Intrinsics::Table*> tableMap;
	};

	Module::~Module()
	{
		if(impl)
		{
			delete impl;
		}
	}

	static void initializeModule(Intrinsics::Module& moduleRef)
	{
		if(!moduleRef.impl) { moduleRef.impl = new Intrinsics::ModuleImpl; }
	}

	Function::Function(
		Intrinsics::Module& moduleRef,
		const char* inName,
		void* inNativeFunction,
		const IR::FunctionType* inType,
		Runtime::CallingConvention inCallingConvention)
	: name(inName)
	, type(inType)
	, nativeFunction(inNativeFunction)
	, callingConvention(inCallingConvention)
	{
		initializeModule(moduleRef);

		if(moduleRef.impl->functionMap.contains(name))
		{
			Errors::fatalf("Intrinsic function already registered: %s",name);
		}
		moduleRef.impl->functionMap.set(name, this);
	}

	Runtime::FunctionInstance* Function::instantiate(Runtime::Compartment* compartment)
	{
		return new Runtime::FunctionInstance(
			nullptr,
			IR::FunctionType::get(type->ret,type->parameters),
			nativeFunction,
			callingConvention,
			name);
	}

	Global::Global(Intrinsics::Module& moduleRef,const char* inName,IR::ValueType inType,Runtime::Value inValue)
	: name(inName)
	, type(inType)
	, value(inValue)
	{
		initializeModule(moduleRef);

		if(moduleRef.impl->globalMap.contains(name))
		{
			Errors::fatalf("Intrinsic global already registered: %s",name);
		}
		moduleRef.impl->globalMap.set(name, this);
	}

	Runtime::GlobalInstance* Global::instantiate(Runtime::Compartment* compartment)
	{
		return Runtime::createGlobal(compartment,IR::GlobalType(type,false),value);
	}

	Table::Table(Intrinsics::Module& moduleRef,const char* inName,const IR::TableType& inType)
	: name(inName)
	, type(inType)
	{
		initializeModule(moduleRef);

		if(moduleRef.impl->tableMap.contains(name))
		{
			Errors::fatalf("Intrinsic table already registered: %s",name);
		}
		moduleRef.impl->tableMap.set(name, this);
	}

	Runtime::TableInstance* Table::instantiate(Runtime::Compartment* compartment)
	{
		return Runtime::createTable(compartment,type);
	}
	
	Memory::Memory(Intrinsics::Module& moduleRef,const char* inName,const IR::MemoryType& inType)
	: name(inName)
	, type(inType)
	{
		initializeModule(moduleRef);

		if(moduleRef.impl->memoryMap.contains(name))
		{
			Errors::fatalf("Intrinsic memory already registered: %s",name);
		}
		moduleRef.impl->memoryMap.set(name, this);
	}

	Runtime::MemoryInstance* Memory::instantiate(Runtime::Compartment* compartment)
	{
		return Runtime::createMemory(compartment,type);
	}

	Runtime::ModuleInstance* instantiateModule(
		Runtime::Compartment* compartment,
		const Intrinsics::Module& moduleRef,
		std::string&& debugName,
		const HashMap<std::string,Runtime::Object*>& extraExports)
	{
		auto moduleInstance = new Runtime::ModuleInstance(compartment,{},{},{},{},{},std::move(debugName));

		if(moduleRef.impl)
		{
			for(const auto& pair : moduleRef.impl->functionMap)
			{
				auto functionInstance = pair.value->instantiate(compartment);
				moduleInstance->functions.push_back(functionInstance);
				errorUnless(moduleInstance->exportMap.add(pair.key, functionInstance));
			}

			for(const auto& pair : moduleRef.impl->tableMap)
			{
				auto tableInstance = pair.value->instantiate(compartment);
				moduleInstance->tables.push_back(tableInstance);
				errorUnless(moduleInstance->exportMap.add(pair.key, tableInstance));
			}

			for(const auto& pair : moduleRef.impl->memoryMap)
			{
				auto memoryInstance = pair.value->instantiate(compartment);
				moduleInstance->memories.push_back(memoryInstance);
				errorUnless(moduleInstance->exportMap.add(pair.key, memoryInstance));
			}

			for(const auto& pair : moduleRef.impl->globalMap)
			{
				auto globalInstance = pair.value->instantiate(compartment);
				moduleInstance->globals.push_back(globalInstance);
				errorUnless(moduleInstance->exportMap.add(pair.key, globalInstance));
			}

			for(const auto& pair : extraExports)
			{
				Runtime::Object* object = pair.value;
				moduleInstance->exportMap.set(pair.key, object);

				switch(object->kind)
				{
				case Runtime::ObjectKind::function: moduleInstance->functions.push_back(asFunction(object)); break;
				case Runtime::ObjectKind::table: moduleInstance->tables.push_back(asTable(object)); break;
				case Runtime::ObjectKind::memory: moduleInstance->memories.push_back(asMemory(object)); break;
				case Runtime::ObjectKind::global: moduleInstance->globals.push_back(asGlobal(object)); break;
				case Runtime::ObjectKind::exceptionType: moduleInstance->exceptionTypes.push_back(asExceptionType(object)); break;
				default: Errors::unreachable();
				};
			}
		}

		return moduleInstance;
	}
}
