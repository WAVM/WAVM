#include "Inline/BasicTypes.h"
#include "Intrinsics.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

#include <string>

namespace Intrinsics
{
	struct ModuleImpl
	{
		std::map<std::string,Intrinsics::Function*> functionMap;
		std::map<std::string,Intrinsics::Global*> globalMap;
		std::map<std::string,Intrinsics::Memory*> memoryMap;
		std::map<std::string,Intrinsics::Table*> tableMap;
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

		if(moduleRef.impl->functionMap.count(name))
		{
			Errors::fatalf("Intrinsic function already registered: %s",name);
		}
		moduleRef.impl->functionMap[name] = this;
	}

	Runtime::FunctionInstance* Function::instantiate(Runtime::Compartment* compartment)
	{
		return new Runtime::FunctionInstance(
			nullptr,
			IR::FunctionType::get(type->ret,type->parameters),
			nativeFunction,
			callingConvention);
	}

	Global::Global(Intrinsics::Module& moduleRef,const char* inName,IR::ValueType inType,Runtime::Value inValue)
	: name(inName)
	, type(inType)
	, value(inValue)
	{
		initializeModule(moduleRef);

		if(moduleRef.impl->globalMap.count(name))
		{
			Errors::fatalf("Intrinsic global already registered: %s",name);
		}
		moduleRef.impl->globalMap[name] = this;
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

		if(moduleRef.impl->tableMap.count(name))
		{
			Errors::fatalf("Intrinsic table already registered: %s",name);
		}
		moduleRef.impl->tableMap[name] = this;
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

		if(moduleRef.impl->memoryMap.count(name))
		{
			Errors::fatalf("Intrinsic memory already registered: %s",name);
		}
		moduleRef.impl->memoryMap[name] = this;
	}

	Runtime::MemoryInstance* Memory::instantiate(Runtime::Compartment* compartment)
	{
		return Runtime::createMemory(compartment,type);
	}

	Runtime::ModuleInstance* instantiateModule(
		Runtime::Compartment* compartment,
		const Intrinsics::Module& moduleRef,
		const std::map<std::string,Runtime::Object*>& extraExports)
	{
		auto moduleInstance = new Runtime::ModuleInstance(compartment,{},{},{},{},{});

		if(moduleRef.impl)
		{
			for(const auto& pair : moduleRef.impl->functionMap)
			{
				auto functionInstance = pair.second->instantiate(compartment);
				moduleInstance->functions.push_back(functionInstance);
				moduleInstance->exportMap[pair.first] = functionInstance;
			}

			for(const auto& pair : moduleRef.impl->tableMap)
			{
				auto tableInstance = pair.second->instantiate(compartment);
				moduleInstance->tables.push_back(tableInstance);
				moduleInstance->exportMap[pair.first] = tableInstance;
			}

			for(const auto& pair : moduleRef.impl->memoryMap)
			{
				auto memoryInstance = pair.second->instantiate(compartment);
				moduleInstance->memories.push_back(memoryInstance);
				moduleInstance->exportMap[pair.first] = memoryInstance;
			}

			for(const auto& pair : moduleRef.impl->globalMap)
			{
				auto globalInstance = pair.second->instantiate(compartment);
				moduleInstance->globals.push_back(globalInstance);
				moduleInstance->exportMap[pair.first] = globalInstance;
			}

			for(const auto& pair : extraExports)
			{
				Runtime::Object* object = pair.second;
				moduleInstance->exportMap[pair.first] = object;

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
