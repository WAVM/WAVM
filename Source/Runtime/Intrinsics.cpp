#include "Inline/BasicTypes.h"
#include "Intrinsics.h"
#include "Platform/Platform.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

#include <string>

namespace Intrinsics
{
	struct Singleton
	{
		std::map<std::string,Intrinsics::Function*> functionMap;
		std::map<std::string,Intrinsics::Global*> variableMap;
		std::map<std::string,Intrinsics::Memory*> memoryMap;
		std::map<std::string,Intrinsics::Table*> tableMap;
		Platform::Mutex* mutex;

		Singleton(): mutex(Platform::createMutex()) {}
		Singleton(const Singleton&) = delete;

		static Singleton& get()
		{
			static Singleton result;
			return result;
		}
	};
	
	std::string getDecoratedName(const std::string& name,const IR::ObjectType& type)
	{
		std::string decoratedName = name;
		decoratedName += " : ";
		decoratedName += IR::asString(type);
		return decoratedName;
	}

	Function::Function(const char* inName,const IR::FunctionType* type,void* nativeFunction)
	:	name(inName)
	{
		function = new Runtime::FunctionInstance(nullptr,type,nativeFunction);
		Platform::Lock lock(Singleton::get().mutex);
		const std::string decoratedName = getDecoratedName(inName,type);
		if(Singleton::get().functionMap.count(decoratedName))
		{
			Errors::fatalf("Intrinsic function already registered: %s",decoratedName.c_str());
		}
		Singleton::get().functionMap[decoratedName] = this;
	}

	Function::~Function()
	{
		{
			Platform::Lock Lock(Singleton::get().mutex);
			Singleton::get().functionMap.erase(Singleton::get().functionMap.find(getDecoratedName(name,function->type)));
		}
		delete function;
	}

	Global::Global(const char* inName,IR::GlobalType inType)
	:	name(inName)
	,	globalType(inType)
	{
		global = Runtime::createGlobal(inType,Runtime::Value((I64)0));
		value = &global->value;
		{
			Platform::Lock lock(Singleton::get().mutex);
			const std::string decoratedName = getDecoratedName(inName,inType);
			if(Singleton::get().variableMap.count(decoratedName))
			{
				Errors::fatalf("Intrinsic global already registered: %s",decoratedName.c_str());
			}
			Singleton::get().variableMap[decoratedName] = this;
		}
	}

	Global::~Global()
	{
		{
			Platform::Lock Lock(Singleton::get().mutex);
			Singleton::get().variableMap.erase(Singleton::get().variableMap.find(getDecoratedName(name,global->type)));
		}
		delete global;
	}

	void Global::reset()
	{
		global = Runtime::createGlobal(globalType,Runtime::Value((I64)0));
		value = &global->value;
	}

	Table::Table(const char* inName,const IR::TableType& type)
	: name(inName)
	, table(Runtime::createTable(type))
	{
		if(!table) { Errors::fatal("failed to create intrinsic table"); }

		Platform::Lock lock(Singleton::get().mutex);
		const std::string decoratedName = getDecoratedName(inName,type);
		if(Singleton::get().tableMap.count(decoratedName))
		{
			Errors::fatalf("Intrinsic table already registered: %s",decoratedName.c_str());
		}
		Singleton::get().tableMap[decoratedName] = this;
	}
	
	Table::~Table()
	{
		{
			Platform::Lock Lock(Singleton::get().mutex);
			Singleton::get().tableMap.erase(Singleton::get().tableMap.find(getDecoratedName(name,table->type)));
		}
		delete table;
	}
	
	Memory::Memory(const char* inName,const IR::MemoryType& type)
	: name(inName)
	, memory(Runtime::createMemory(type))
	{
		if(!memory) { Errors::fatal("failed to create intrinsic memory"); }

		Platform::Lock lock(Singleton::get().mutex);
		const std::string decoratedName = getDecoratedName(inName,type);
		if(Singleton::get().memoryMap.count(decoratedName))
		{
			Errors::fatalf("Intrinsic memory already registered: %s",decoratedName.c_str());
		}
		Singleton::get().memoryMap[decoratedName] = this;
	}
	
	Memory::~Memory()
	{
		{
			Platform::Lock Lock(Singleton::get().mutex);
			Singleton::get().memoryMap.erase(Singleton::get().memoryMap.find(getDecoratedName(name,memory->type)));
		}
		delete memory;
	}

	Runtime::ObjectInstance* find(const std::string& name,const IR::ObjectType& type)
	{
		std::string decoratedName = getDecoratedName(name,type);
		Platform::Lock Lock(Singleton::get().mutex);
		Runtime::ObjectInstance* result = nullptr;
		switch(type.kind)
		{
		case IR::ObjectKind::function:
		{
			auto keyValue = Singleton::get().functionMap.find(decoratedName);
			result = keyValue == Singleton::get().functionMap.end() ? nullptr : asObject(keyValue->second->getObject());
			break;
		}
		case IR::ObjectKind::table:
		{
			auto keyValue = Singleton::get().tableMap.find(decoratedName);
			result = keyValue == Singleton::get().tableMap.end() ? nullptr : asObject((Runtime::TableInstance*)*keyValue->second);
			break;
		}
		case IR::ObjectKind::memory:
		{
			auto keyValue = Singleton::get().memoryMap.find(decoratedName);
			result = keyValue == Singleton::get().memoryMap.end() ? nullptr : asObject((Runtime::MemoryInstance*)*keyValue->second);
			break;
		}
		case IR::ObjectKind::global:
		{
			auto keyValue = Singleton::get().variableMap.find(decoratedName);
			result = keyValue == Singleton::get().variableMap.end() ? nullptr : asObject(keyValue->second->getObject());
			break;
		}
		default: Errors::unreachable();
		};
		if(result && !isA(result,type)) { result = nullptr; }
		return result;
	}
}
