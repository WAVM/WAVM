#include "Core/Core.h"
#include "Intrinsics.h"
#include "Core/Platform.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

#include <string>

namespace Intrinsics
{
	struct Singleton
	{
		std::map<std::string,Function*> functionMap;
		std::map<std::string,Global*> variableMap;
		std::map<std::string,Memory*> memoryMap;
		std::map<std::string,Table*> tableMap;
		Platform::Mutex mutex;

		Singleton() {}
		Singleton(const Singleton&) = delete;

		static Singleton& get()
		{
			static Singleton result;
			return result;
		}
	};
	
	std::string getDecoratedName(const char* name,const WebAssembly::ObjectType& type)
	{
		std::string decoratedName = name;
		decoratedName += " : ";
		decoratedName += WebAssembly::asString(type);
		return decoratedName;
	}

	Function::Function(const char* inName,const WebAssembly::FunctionType* type,void* nativeFunction)
	:	name(inName)
	{
		function = new Runtime::FunctionInstance(nullptr,type,nativeFunction);
		Platform::Lock lock(Singleton::get().mutex);
		Singleton::get().functionMap[getDecoratedName(inName,type)] = this;
	}

	Function::~Function()
	{
		{
			Platform::Lock Lock(Singleton::get().mutex);
			Singleton::get().functionMap.erase(Singleton::get().functionMap.find(getDecoratedName(name,function->type)));
		}
		delete function;
	}

	Global::Global(const char* inName,WebAssembly::GlobalType inType)
	:	name(inName)
	,	globalType(inType)
	{
		global = new Runtime::GlobalInstance(inType,Runtime::Value((int64)0));
		value = &global->value;
		{
			Platform::Lock lock(Singleton::get().mutex);
			Singleton::get().variableMap[getDecoratedName(inName,inType)] = this;
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
		global = new Runtime::GlobalInstance(globalType,Runtime::Value((int64)0));
		value = &global->value;
	}

	Table::Table(const char* inName,const WebAssembly::TableType& type)
	: name(inName)
	, table(Runtime::createTable(type))
	{
		if(!table) { Core::error("failed to create intrinsic table"); }

		Platform::Lock lock(Singleton::get().mutex);
		Singleton::get().tableMap[getDecoratedName(inName,type)] = this;
	}
	
	Table::~Table()
	{
		{
			Platform::Lock Lock(Singleton::get().mutex);
			Singleton::get().tableMap.erase(Singleton::get().tableMap.find(getDecoratedName(name,table->type)));
		}
		delete table;
	}
	
	Memory::Memory(const char* inName,const WebAssembly::MemoryType& type)
	: name(inName)
	, memory(Runtime::createMemory(type))
	{
		if(!memory) { Core::error("failed to create intrinsic memory"); }

		Platform::Lock lock(Singleton::get().mutex);
		Singleton::get().memoryMap[getDecoratedName(inName,type)] = this;
	}
	
	Memory::~Memory()
	{
		{
			Platform::Lock Lock(Singleton::get().mutex);
			Singleton::get().memoryMap.erase(Singleton::get().memoryMap.find(getDecoratedName(name,memory->type)));
		}
		delete memory;
	}

	Runtime::Object* find(const char* name,const WebAssembly::ObjectType& type)
	{
		std::string decoratedName = getDecoratedName(name,type);
		Platform::Lock Lock(Singleton::get().mutex);
		Runtime::Object* result = nullptr;
		switch(type.kind)
		{
		case WebAssembly::ObjectKind::function:
		{
			auto keyValue = Singleton::get().functionMap.find(decoratedName);
			result = keyValue == Singleton::get().functionMap.end() ? nullptr : asObject(keyValue->second->function);
			break;
		}
		case WebAssembly::ObjectKind::table:
		{
			auto keyValue = Singleton::get().tableMap.find(decoratedName);
			result = keyValue == Singleton::get().tableMap.end() ? nullptr : asObject((Runtime::Table*)*keyValue->second);
			break;
		}
		case WebAssembly::ObjectKind::memory:
		{
			auto keyValue = Singleton::get().memoryMap.find(decoratedName);
			result = keyValue == Singleton::get().memoryMap.end() ? nullptr : asObject((Runtime::Memory*)*keyValue->second);
			break;
		}
		case WebAssembly::ObjectKind::global:
		{
			auto keyValue = Singleton::get().variableMap.find(decoratedName);
			result = keyValue == Singleton::get().variableMap.end() ? nullptr : asObject(keyValue->second->global);
			break;
		}
		default: Core::unreachable();
		};
		if(result && !isA(result,type)) { result = nullptr; }
		return result;
	}
	
	std::vector<Runtime::Object*> getAllIntrinsicObjects()
	{
		Platform::Lock lock(Singleton::get().mutex);
		std::vector<Runtime::Object*> result;
		for(auto mapIt : Singleton::get().functionMap) { result.push_back(mapIt.second->function); }
		for(auto mapIt : Singleton::get().tableMap) { result.push_back((Runtime::Table*)*mapIt.second); }
		for(auto mapIt : Singleton::get().memoryMap) { result.push_back((Runtime::Memory*)*mapIt.second); }
		for(auto mapIt : Singleton::get().variableMap) { result.push_back(mapIt.second->global); }
		return result;
	}
}
