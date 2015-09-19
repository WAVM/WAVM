#include "Core/Core.h"
#include "Intrinsics.h"
#include "Core/Platform.h"

#include <string>

namespace Intrinsics
{
	struct Singleton
	{
		std::map<std::string,Function*> functionMap;
		std::map<std::string,Value*> valueMap;
		Platform::Mutex mutex;

		Singleton() {}
		Singleton(const Singleton&) = delete;

		static Singleton& get()
		{
			static Singleton result;
			return result;
		}
	};

	std::string getDecoratedFunctionName(const char* name,const AST::FunctionType& type)
	{
		std::string decoratedName = name;
		decoratedName += " (";
		for(uintptr_t parameterIndex = 0;parameterIndex < type.parameters.size();++parameterIndex)
		{
			if(parameterIndex != 0) { decoratedName += ','; }
			decoratedName += AST::getTypeName(type.parameters[parameterIndex]);
		}
		decoratedName += ")->";
		decoratedName += getTypeName(type.returnType);
		return decoratedName;
	}
	
	std::string getDecoratedValueName(const char* name,AST::TypeId type)
	{
		std::string decoratedName = name;
		decoratedName += " (";
		decoratedName += getTypeName(type);
		decoratedName += ")";
		return decoratedName;
	}

	Function::Function(const char* inName,const AST::FunctionType& inType,void* inValue)
	:	name(inName)
	,	type(inType)
	,	value(inValue)
	{
		Platform::Lock lock(Singleton::get().mutex);
		Singleton::get().functionMap[getDecoratedFunctionName(inName,inType)] = this;
	}

	Function::~Function()
	{
		Platform::Lock Lock(Singleton::get().mutex);
		Singleton::get().functionMap.erase(Singleton::get().functionMap.find(getDecoratedFunctionName(name,type)));
	}

	Value::Value(const char* inName,AST::TypeId inType,void* inValue)
	:	name(inName)
	,	type(inType)
	,	value(inValue)
	{
		Platform::Lock lock(Singleton::get().mutex);
		Singleton::get().valueMap[getDecoratedValueName(inName,inType)] = this;
	}

	Value::~Value()
	{
		Platform::Lock Lock(Singleton::get().mutex);
		Singleton::get().valueMap.erase(Singleton::get().valueMap.find(getDecoratedValueName(name,type)));
	}

	const Function* findFunction(const char* decoratedName)
	{
		Platform::Lock Lock(Singleton::get().mutex);
		auto keyValue = Singleton::get().functionMap.find(decoratedName);
		return keyValue == Singleton::get().functionMap.end() ? nullptr : keyValue->second;
	}

	const Value* findValue(const char* decoratedName)
	{
		Platform::Lock Lock(Singleton::get().mutex);
		auto keyValue = Singleton::get().valueMap.find(decoratedName);
		return keyValue == Singleton::get().valueMap.end() ? nullptr : keyValue->second;
	}
}