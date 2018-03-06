#include "Types.h"

#include <map>

namespace IR
{
	struct FunctionTypeMap
	{
		struct Key
		{
			ResultType ret;
			std::vector<ValueType> parameters;

			friend bool operator==(const Key& left,const Key& right)
			{
				return left.ret == right.ret && left.parameters == right.parameters;
			}
			friend bool operator!=(const Key& left,const Key& right)
			{
				return left.ret != right.ret || left.parameters != right.parameters;
			}
			friend bool operator<(const Key& left,const Key& right)
			{
				if(left.ret != right.ret) { return left.ret < right.ret; }
				else { return left.parameters < right.parameters; }
			}
		};
		static std::map<Key,const FunctionType*>& get()
		{
			static std::map<Key,const FunctionType*> map;
			return map;
		}
	};

	struct TupleTypeMap
	{
		struct Key
		{
			std::vector<ValueType> elements;

			friend bool operator==(const Key& left,const Key& right)
			{
				return left.elements == right.elements;
			}
			friend bool operator!=(const Key& left,const Key& right)
			{
				return left.elements != right.elements;
			}
			friend bool operator<(const Key& left,const Key& right)
			{
				return left.elements < right.elements;
			}
		};
		static std::map<Key,const TupleType*>& get()
		{
			static std::map<Key,const TupleType*> map;
			return map;
		}
	};

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

	const FunctionType* FunctionType::get(ResultType ret,const std::initializer_list<ValueType>& parameters)
	{ return findExistingOrCreateNew(FunctionTypeMap::get(),FunctionTypeMap::Key {ret,parameters},[=]{return new FunctionType(ret,parameters);}); }
	const FunctionType* FunctionType::get(ResultType ret,const std::vector<ValueType>& parameters)
	{ return findExistingOrCreateNew(FunctionTypeMap::get(),FunctionTypeMap::Key {ret,parameters},[=]{return new FunctionType(ret,parameters);}); }
	const FunctionType* FunctionType::get(ResultType ret)
	{ return findExistingOrCreateNew(FunctionTypeMap::get(),FunctionTypeMap::Key {ret,{}},[=]{return new FunctionType(ret,{});}); }

	const TupleType* TupleType::get(const std::initializer_list<ValueType>& elements)
	{ return findExistingOrCreateNew(TupleTypeMap::get(),TupleTypeMap::Key {elements},[=]{return new TupleType(elements);}); }
	const TupleType* TupleType::get(const std::vector<ValueType>& elements)
	{ return findExistingOrCreateNew(TupleTypeMap::get(),TupleTypeMap::Key {elements},[=]{return new TupleType(elements);}); }
}