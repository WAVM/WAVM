#include "WebAssembly.h"
#include "Types.h"

#include <map>

namespace WebAssembly
{
	struct FunctionTypeKey
	{
		ReturnType ret;
		std::vector<ValueType> parameters;

		friend bool operator==(const FunctionTypeKey& left,const FunctionTypeKey& right) { return left.ret == right.ret && left.parameters == right.parameters; }
		friend bool operator!=(const FunctionTypeKey& left,const FunctionTypeKey& right) { return left.ret != right.ret || left.parameters != right.parameters; }
		friend bool operator<(const FunctionTypeKey& left,const FunctionTypeKey& right) { return left.ret < right.ret || (left.ret == right.ret && left.parameters < right.parameters); }
	};
	static std::map<FunctionTypeKey,FunctionType*> uniqueFunctionTypeMap;

	template<typename Key,typename Value,typename CreateValueThunk>
	Value findExistingOrCreateNew(std::map<Key,Value>& map,Key&& key,CreateValueThunk createValueThunk)
	{
		auto mapIt = map.find(key);
		if(mapIt != map.end()) { return mapIt->second; }
		else
		{
			Value value = createValueThunk();
			map.insert({std::move(key),value});
			return value;
		}
	}

	const FunctionType* FunctionType::get(ReturnType ret,const std::initializer_list<ValueType>& parameters)
	{ return findExistingOrCreateNew(uniqueFunctionTypeMap,FunctionTypeKey {ret,parameters},[=]{return new FunctionType(ret,parameters);}); }
	const FunctionType* FunctionType::get(ReturnType ret,const std::vector<ValueType>& parameters)
	{ return findExistingOrCreateNew(uniqueFunctionTypeMap,FunctionTypeKey {ret,parameters},[=]{return new FunctionType(ret,parameters);}); }
	const FunctionType* FunctionType::get(ReturnType ret)
	{ return findExistingOrCreateNew(uniqueFunctionTypeMap,FunctionTypeKey {ret,{}},[=]{return new FunctionType(ret,{});}); }
}