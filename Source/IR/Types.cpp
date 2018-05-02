#include "Inline/Hash.h"
#include "Inline/HashSet.h"
#include "IR/Types.h"

namespace IR
{
	struct FunctionTypeHashPolicy
	{
		static bool areKeysEqual(const IR::FunctionType* left, const IR::FunctionType* right)
		{
			return left->ret == right->ret && left->parameters == right->parameters;
		}
		static Uptr getKeyHash(const IR::FunctionType* functionType)
		{
			return functionType->hash;
		}
	};

	struct TupleTypeHashPolicy
	{
		static bool areKeysEqual(const IR::TupleType* left, const IR::TupleType* right)
		{
			return left->elements == right->elements;
		}
		static Uptr getKeyHash(const IR::TupleType* tupleType)
		{
			return tupleType->hash;
		}
	};

	struct FunctionTypeSet
	{
		static HashSet<const FunctionType*,FunctionTypeHashPolicy>& get()
		{
			static HashSet<const FunctionType*,FunctionTypeHashPolicy> set;
			return set;
		}
	};

	struct TupleTypeSet
	{
		static HashSet<const TupleType*,TupleTypeHashPolicy>& get()
		{
			static HashSet<const TupleType*,TupleTypeHashPolicy> set;
			return set;
		}
	};

	FunctionType::FunctionType(ResultType inRet,const std::vector<ValueType>& inParameters)
	: ret(inRet)
	, parameters(inParameters)
	{
		const U32 hash32 = XXH32_fixed(U32(inRet),0);
		hash = XXH32(parameters.data(), parameters.size() * sizeof(ValueType), hash32);
	}

	TupleType::TupleType(const std::vector<ValueType>& inElements)
	: elements(inElements)
	{
		hash = XXH32(elements.data(), elements.size() * sizeof(ValueType), 0);
	}

	template<typename Element, typename ElementHashPolicy>
	const Element* findExistingOrCreateNew(
		HashSet<const Element*, ElementHashPolicy>& set,
		Element&& element)
	{
		auto existingType = set.get(&element);
		if(existingType) { return *existingType; }
		else
		{
			const Element* newType = new Element(std::move(element));
			errorUnless(set.add(newType));
			return newType;
		}
	}

	const FunctionType* FunctionType::get(ResultType ret,const std::initializer_list<ValueType>& parameters)
	{ return findExistingOrCreateNew(FunctionTypeSet::get(),FunctionType {ret,parameters}); }
	const FunctionType* FunctionType::get(ResultType ret,const std::vector<ValueType>& parameters)
	{ return findExistingOrCreateNew(FunctionTypeSet::get(),FunctionType {ret,parameters}); }
	const FunctionType* FunctionType::get(ResultType ret)
	{ return findExistingOrCreateNew(FunctionTypeSet::get(),FunctionType {ret,{}}); }

	const TupleType* TupleType::get(const std::initializer_list<ValueType>& elements)
	{ return findExistingOrCreateNew(TupleTypeSet::get(),TupleType {elements}); }
	const TupleType* TupleType::get(const std::vector<ValueType>& elements)
	{ return findExistingOrCreateNew(TupleTypeSet::get(),TupleType {elements}); }
}