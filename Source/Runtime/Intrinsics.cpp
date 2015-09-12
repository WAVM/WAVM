#include "Common/WAVM.h"
#include "Intrinsics.h"

#include <string>

namespace Intrinsics
{
	struct Singleton
	{
		std::map<std::string,Function*> functionMap;
		std::map<std::string,Value*> valueMap;
		WAVM::Mutex mutex;

		Singleton() {}
		Singleton(const Singleton&) = delete;

		static Singleton& get()
		{
			static Singleton result;
			return result;
		}
	};

	Function::Function(const char* inName,const AST::FunctionType& inType,void* inValue)
	:	name(inName)
	,	type(inType)
	,	value(inValue)
	{
		WAVM::Lock lock(Singleton::get().mutex);
		Singleton::get().functionMap[inName] = this;
	}

	Function::~Function()
	{
		WAVM::Lock Lock(Singleton::get().mutex);
		Singleton::get().functionMap.erase(Singleton::get().functionMap.find(name));
	}

	Value::Value(const char* inName,AST::TypeId inType,void* inValue)
	:	name(inName)
	,	type(inType)
	,	value(inValue)
	{
		WAVM::Lock lock(Singleton::get().mutex);
		Singleton::get().valueMap[inName] = this;
	}

	Value::~Value()
	{
		WAVM::Lock Lock(Singleton::get().mutex);
		Singleton::get().valueMap.erase(Singleton::get().valueMap.find(name));
	}

	const Function* findFunction(const char* name)
	{
		WAVM::Lock Lock(Singleton::get().mutex);
		auto keyValue = Singleton::get().functionMap.find(name);
		return keyValue == Singleton::get().functionMap.end() ? nullptr : keyValue->second;
	}

	const Value* findValue(const char* name)
	{
		WAVM::Lock Lock(Singleton::get().mutex);
		auto keyValue = Singleton::get().valueMap.find(name);
		return keyValue == Singleton::get().valueMap.end() ? nullptr : keyValue->second;
	}
}