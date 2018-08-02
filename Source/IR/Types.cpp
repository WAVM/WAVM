#include "IR/Types.h"
#include "Inline/Hash.h"
#include "Inline/HashSet.h"

namespace IR
{
	struct TypeTupleHashPolicy
	{
		static bool areKeysEqual(TypeTuple left, TypeTuple right)
		{
			if(left.size() != right.size()) { return false; }
			for(Uptr elemIndex = 0; elemIndex < left.size(); ++elemIndex)
			{
				if(left[elemIndex] != right[elemIndex]) { return false; }
			}
			return true;
		}
		static Uptr getKeyHash(TypeTuple typeTuple) { return typeTuple.getHash(); }
	};

	struct FunctionTypeHashPolicy
	{
		static bool areKeysEqual(FunctionType left, FunctionType right)
		{
			return left.params() == right.params() && left.results() == right.results();
		}
		static Uptr getKeyHash(FunctionType functionType) { return functionType.getHash(); }
	};

	TypeTuple::Impl::Impl(Uptr inNumElems, const ValueType* inElems)
	: numElems(inNumElems)
	{
		memcpy(elems, inElems, sizeof(ValueType) * numElems);
		hash = XXH64(elems, numElems * sizeof(ValueType), 0);
	}

	TypeTuple::Impl::Impl(const Impl& inCopy)
	: hash(inCopy.hash)
	, numElems(inCopy.numElems)
	{
		memcpy(elems, inCopy.elems, numElems * sizeof(ValueType));
	}

	TypeTuple::TypeTuple()
	{
		static TypeTuple emptyTuple(getUniqueImpl(0, nullptr));
		impl = emptyTuple.impl;
	}

	TypeTuple::TypeTuple(ValueType inElem) { impl = getUniqueImpl(1, &inElem); }

	TypeTuple::TypeTuple(const std::initializer_list<ValueType>& inElems)
	{
		impl = getUniqueImpl(inElems.size(), inElems.begin());
	}

	TypeTuple::TypeTuple(const std::vector<ValueType>& inElems)
	{
		impl = getUniqueImpl(inElems.size(), inElems.data());
	}

	const TypeTuple::Impl* TypeTuple::getUniqueImpl(Uptr numElems, const ValueType* inElems)
	{
		const Uptr numImplBytes = Impl::calcNumBytes(numElems);
		Impl* localImpl         = new(alloca(numImplBytes)) Impl(numElems, inElems);

		static HashSet<TypeTuple, TypeTupleHashPolicy> uniqueTypeTupleSet;

		const TypeTuple* typeTuple = uniqueTypeTupleSet.get(TypeTuple(localImpl));
		if(typeTuple) { return typeTuple->impl; }
		else
		{
			Impl* globalImpl = new(malloc(numImplBytes)) Impl(*localImpl);
			errorUnless(uniqueTypeTupleSet.add(TypeTuple(globalImpl)));
			return globalImpl;
		}
	}

	FunctionType::Impl::Impl(TypeTuple inResults, TypeTuple inParams)
	: results(inResults)
	, params(inParams)
	{
		hash = Hash<Uptr>()(results.getHash(), params.getHash());
	}

	const FunctionType::Impl* FunctionType::getUniqueImpl(TypeTuple results, TypeTuple params)
	{
		Impl localImpl(results, params);

		static HashSet<FunctionType, FunctionTypeHashPolicy> uniqueFunctionTypeSet;

		const FunctionType* functionType = uniqueFunctionTypeSet.get(FunctionType(&localImpl));
		if(functionType) { return functionType->impl; }
		else
		{
			Impl* globalImpl = new Impl(localImpl);
			errorUnless(uniqueFunctionTypeSet.add(FunctionType(globalImpl)));
			return globalImpl;
		}
	}
} // namespace IR