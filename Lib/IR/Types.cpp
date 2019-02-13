#include "WAVM/IR/Types.h"

#include <stdlib.h>
#include <string.h>
#include <new>
#include <utility>

#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Mutex.h"

using namespace WAVM;
using namespace WAVM::IR;

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

IR::TypeTuple::Impl::Impl(Uptr inNumElems, const ValueType* inElems) : numElems(inNumElems)
{
	if(numElems) { memcpy(elems, inElems, sizeof(ValueType) * numElems); }
	hash = XXH<Uptr>(elems, numElems * sizeof(ValueType), 0);
}

IR::TypeTuple::Impl::Impl(const Impl& inCopy) : hash(inCopy.hash), numElems(inCopy.numElems)
{
	if(numElems) { memcpy(elems, inCopy.elems, numElems * sizeof(ValueType)); }
}

IR::TypeTuple::TypeTuple(ValueType inElem) { impl = getUniqueImpl(1, &inElem); }

IR::TypeTuple::TypeTuple(const std::initializer_list<ValueType>& inElems)
{
	impl = getUniqueImpl(inElems.size(), inElems.begin());
}

IR::TypeTuple::TypeTuple(const std::vector<ValueType>& inElems)
{
	impl = getUniqueImpl(inElems.size(), inElems.data());
}

IR::TypeTuple::TypeTuple(const ValueType* inElems, Uptr numElems)
{
	impl = getUniqueImpl(numElems, inElems);
}

const TypeTuple::Impl* IR::TypeTuple::getUniqueImpl(Uptr numElems, const ValueType* inElems)
{
	if(numElems == 0)
	{
		static Impl emptyImpl(0, nullptr);
		return &emptyImpl;
	}
	else
	{
		const Uptr numImplBytes = Impl::calcNumBytes(numElems);
		Impl* localImpl = new(alloca(numImplBytes)) Impl(numElems, inElems);

		static Platform::Mutex uniqueTypeTupleSetMutex;
		static HashSet<TypeTuple, TypeTupleHashPolicy> uniqueTypeTupleSet;

		Lock<Platform::Mutex> uniqueTypeTupleSetLock(uniqueTypeTupleSetMutex);

		const TypeTuple* typeTuple = uniqueTypeTupleSet.get(TypeTuple(localImpl));
		if(typeTuple) { return typeTuple->impl; }
		else
		{
			Impl* globalImpl = new(malloc(numImplBytes)) Impl(*localImpl);
			Platform::expectLeakedObject(globalImpl);
			uniqueTypeTupleSet.addOrFail(TypeTuple(globalImpl));
			return globalImpl;
		}
	}
}

IR::FunctionType::Impl::Impl(TypeTuple inResults, TypeTuple inParams)
: results(inResults), params(inParams)
{
	hash = Hash<Uptr>()(results.getHash(), params.getHash());
}

const FunctionType::Impl* IR::FunctionType::getUniqueImpl(TypeTuple results, TypeTuple params)
{
	if(results.size() == 0 && params.size() == 0)
	{
		static Impl emptyImpl{TypeTuple(), TypeTuple()};
		return &emptyImpl;
	}
	else
	{
		Impl localImpl(results, params);

		static Platform::Mutex uniqueFunctionTypeSetMutex;
		static HashSet<FunctionType, FunctionTypeHashPolicy> uniqueFunctionTypeSet;

		Lock<Platform::Mutex> uniqueFunctionTypeSetLock(uniqueFunctionTypeSetMutex);

		const FunctionType* functionType = uniqueFunctionTypeSet.get(FunctionType(&localImpl));
		if(functionType) { return functionType->impl; }
		else
		{
			Impl* globalImpl = new Impl(localImpl);
			Platform::expectLeakedObject(globalImpl);
			uniqueFunctionTypeSet.addOrFail(FunctionType(globalImpl));
			return globalImpl;
		}
	}
}
