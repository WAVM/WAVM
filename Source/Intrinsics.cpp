#include "WAVM.h"
#include "Intrinsics.h"

#include <string>

namespace Intrinsics
{
	struct Singleton
	{
		std::map<std::string,WAVM::IntrinsicFunction*> functionMap;
		std::map<std::string,WAVM::IntrinsicValue*> valueMap;
		WAVM::Mutex mutex;

		Singleton() {}
		Singleton(const Singleton&) = delete;

		static Singleton& get()
		{
			static Singleton result;
			return result;
		}
	};
};

namespace WAVM
{
	IntrinsicFunction::IntrinsicFunction(const char* inName,FunctionType inType,void* inValue)
	:	name(inName)
	,	type(inType)
	,	value(inValue)
	{
		Lock lock(Intrinsics::Singleton::get().mutex);
		Intrinsics::Singleton::get().functionMap[inName] = this;
	}

	IntrinsicFunction::~IntrinsicFunction()
	{
		Lock Lock(Intrinsics::Singleton::get().mutex);
		Intrinsics::Singleton::get().functionMap.erase(Intrinsics::Singleton::get().functionMap.find(name));
	}

	IntrinsicValue::IntrinsicValue(const char* inName,WASM::Type inType,void* inValue)
	:	name(inName)
	,	type(inType)
	,	value(inValue)
	{
		Lock lock(Intrinsics::Singleton::get().mutex);
		Intrinsics::Singleton::get().valueMap[inName] = this;
	}

	IntrinsicValue::~IntrinsicValue()
	{
		Lock Lock(Intrinsics::Singleton::get().mutex);
		Intrinsics::Singleton::get().valueMap.erase(Intrinsics::Singleton::get().valueMap.find(name));
	}

	const IntrinsicFunction* findIntrinsicFunction(const char* name)
	{
		Lock Lock(Intrinsics::Singleton::get().mutex);
		auto keyValue = Intrinsics::Singleton::get().functionMap.find(name);
		return keyValue == Intrinsics::Singleton::get().functionMap.end() ? nullptr : keyValue->second;
	}

	const IntrinsicValue* findIntrinsicValue(const char* name)
	{
		Lock Lock(Intrinsics::Singleton::get().mutex);
		auto keyValue = Intrinsics::Singleton::get().valueMap.find(name);
		return keyValue == Intrinsics::Singleton::get().valueMap.end() ? nullptr : keyValue->second;
	}
}