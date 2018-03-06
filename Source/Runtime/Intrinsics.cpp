#include "Inline/BasicTypes.h"
#include "Intrinsics.h"
#include "Platform/Platform.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

#include <string>

namespace Intrinsics
{
	struct Module
	{
		std::map<std::string,Intrinsics::Function*> functionMap;
		std::map<std::string,Intrinsics::Global*> globalMap;
		std::map<std::string,Intrinsics::Memory*> memoryMap;
		std::map<std::string,Intrinsics::Table*> tableMap;

		~Module() = delete;
	};

	static void initializeModuleRef(Intrinsics::Module*& moduleRef)
	{
		if(!moduleRef) { moduleRef = new Intrinsics::Module(); }
	}

	Function::Function(Intrinsics::Module*& moduleRef,const char* inName,const IR::FunctionType* inType,void* inNativeFunction)
	: name(inName)
	, type(inType)
	, nativeFunction(inNativeFunction)
	{
		initializeModuleRef(moduleRef);

		if(moduleRef->functionMap.count(name))
		{
			Errors::fatalf("Intrinsic function already registered: %s",name);
		}
		moduleRef->functionMap[name] = this;
	}

	Runtime::FunctionInstance* Function::instantiate(Runtime::Compartment* compartment)
	{
		return new Runtime::FunctionInstance(
			nullptr,
			IR::FunctionType::get(type->ret,type->parameters),
			LLVMJIT::getIntrinsicThunk(nativeFunction,type));
	}

	Global::Global(Intrinsics::Module*& moduleRef,const char* inName,IR::ValueType inType,Runtime::Value inValue)
	: name(inName)
	, type(inType)
	, value(inValue)
	{
		initializeModuleRef(moduleRef);

		if(moduleRef->globalMap.count(name))
		{
			Errors::fatalf("Intrinsic global already registered: %s",name);
		}
		moduleRef->globalMap[name] = this;
	}

	Runtime::GlobalInstance* Global::instantiate(Runtime::Compartment* compartment)
	{
		return Runtime::createGlobal(compartment,IR::GlobalType(type,false),value);
	}

	Table::Table(Intrinsics::Module*& moduleRef,const char* inName,const IR::TableType& inType)
	: name(inName)
	, type(inType)
	{
		initializeModuleRef(moduleRef);

		if(moduleRef->tableMap.count(name))
		{
			Errors::fatalf("Intrinsic table already registered: %s",name);
		}
		moduleRef->tableMap[name] = this;
	}

	Runtime::TableInstance* Table::instantiate(Runtime::Compartment* compartment)
	{
		return Runtime::createTable(compartment,type);
	}
	
	Memory::Memory(Intrinsics::Module*& moduleRef,const char* inName,const IR::MemoryType& inType)
	: name(inName)
	, type(inType)
	{
		initializeModuleRef(moduleRef);

		if(moduleRef->memoryMap.count(name))
		{
			Errors::fatalf("Intrinsic memory already registered: %s",name);
		}
		moduleRef->memoryMap[name] = this;
	}

	Runtime::MemoryInstance* Memory::instantiate(Runtime::Compartment* compartment)
	{
		return Runtime::createMemory(compartment,type);
	}

	Runtime::ModuleInstance* instantiateModule(Runtime::Compartment* compartment,Intrinsics::Module* module)
	{
		auto moduleInstance = new Runtime::ModuleInstance(compartment,{},{},{},{},{});
		
		for(const auto& pair : module->functionMap)
		{
			auto functionInstance = pair.second->instantiate(compartment);
			moduleInstance->functions.push_back(functionInstance);
			moduleInstance->exportMap[pair.first] = functionInstance;
		}

		for(const auto& pair : module->tableMap)
		{
			auto tableInstance = pair.second->instantiate(compartment);
			moduleInstance->tables.push_back(tableInstance);
			moduleInstance->exportMap[pair.first] = tableInstance;
		}

		for(const auto& pair : module->memoryMap)
		{
			auto memoryInstance = pair.second->instantiate(compartment);
			moduleInstance->memories.push_back(memoryInstance);
			moduleInstance->exportMap[pair.first] = memoryInstance;
		}

		for(const auto& pair : module->globalMap)
		{
			auto globalInstance = pair.second->instantiate(compartment);
			moduleInstance->globals.push_back(globalInstance);
			moduleInstance->exportMap[pair.first] = globalInstance;
		}

		return moduleInstance;
	}
}
