#pragma once

#include "Core/Core.h"

#include <string.h>

// Encapsulates a set of integers that are in the range 0 to maxIndexPlusOne (excluding maxIndexPlusOne).
// It uses 1 bit of storage for each integer in the range, and many operations look at all bits, so it's best suited to small ranges.
// However, this avoids heap allocations, and so is pretty fast for sets of small integers (e.g. uint8).
template<typename Index,size_t maxIndexPlusOne>
struct DenseStaticIntSet
{
	DenseStaticIntSet()
	{
		memset(elements,0,sizeof(elements));
	}
	DenseStaticIntSet(Index index)
	{
		memset(elements,0,sizeof(elements));
		add(index);
	}

	// Queries

	inline bool contains(Index index) const
	{
		assert((size_t)index < maxIndexPlusOne);
		return (elements[index / indicesPerElement] & (1ull << (index % indicesPerElement))) != 0;
	}
	bool isEmpty() const
	{
		Element combinedElements = 0;
		for(uintp elementIndex = 0;elementIndex < numElements;++elementIndex)
		{
			combinedElements |= elements[elementIndex];
		}
		return combinedElements == 0;
	}
	inline Index getSmallestMember() const
	{
		// Find the first element that has any bits set.
		for(uintp elementIndex = 0;elementIndex < numElements;++elementIndex)
		{
			if(elements[elementIndex])
			{
				// Find the index of the lowest set bit in the element using countTrailingZeroes.
				const Index result = (Index)(elementIndex * indicesPerElement + Platform::countTrailingZeroes(elements[elementIndex]));
				assert(contains(result));
				return result;
			}
		}
		return maxIndexPlusOne;
	}

	// Adding/removing indices

	inline void add(Index index)
	{
		assert((size_t)index < maxIndexPlusOne);
		elements[index / indicesPerElement] |= 1ull << (index % indicesPerElement);
	}
	inline void addRange(Index rangeMin,Index rangeMax)
	{
		assert(rangeMin <= rangeMax);
		assert((size_t)rangeMax < maxIndexPlusOne);
		for(Index index = rangeMin;index <= rangeMax;++index)
		{
			add(index);
		}
	}
	inline bool remove(Index index)
	{
		const Element elementMask = 1ull << (index % indicesPerElement);
		const bool hadIndex = (elements[index / indicesPerElement] & elementMask) != 0;
		elements[index / indicesPerElement] &= ~elementMask;
		return hadIndex;
	}

	// Logical operators

	friend DenseStaticIntSet operator~(const DenseStaticIntSet& set)
	{
		DenseStaticIntSet result;
		for(uintp elementIndex = 0;elementIndex < numElements;++elementIndex)
		{
			result.elements[elementIndex] = ~set.elements[elementIndex];
		}
		return result;
	}
	friend DenseStaticIntSet operator|(const DenseStaticIntSet& left,const DenseStaticIntSet& right)
	{
		DenseStaticIntSet result;
		for(uintp elementIndex = 0;elementIndex < numElements;++elementIndex)
		{
			result.elements[elementIndex] = left.elements[elementIndex] | right.elements[elementIndex];
		}
		return result;
	}
	friend DenseStaticIntSet operator&(const DenseStaticIntSet& left,const DenseStaticIntSet& right)
	{
		DenseStaticIntSet result;
		for(uintp elementIndex = 0;elementIndex < numElements;++elementIndex)
		{
			result.elements[elementIndex] = left.elements[elementIndex] & right.elements[elementIndex];
		}
		return result;
	}
	friend DenseStaticIntSet operator^(const DenseStaticIntSet& left,const DenseStaticIntSet& right)
	{
		DenseStaticIntSet result;
		for(uintp elementIndex = 0;elementIndex < numElements;++elementIndex)
		{
			result.elements[elementIndex] = left.elements[elementIndex] ^ right.elements[elementIndex];
		}
		return result;
	}

	// Comparisons

	friend bool operator==(const DenseStaticIntSet& left,const DenseStaticIntSet& right)
	{
		return memcmp(left.elements,right.elements,sizeof(DenseStaticIntSet::elements)) == 0;
	}
	friend bool operator!=(const DenseStaticIntSet& left,const DenseStaticIntSet& right)
	{
		return memcmp(left.elements,right.elements,sizeof(DenseStaticIntSet::elements)) != 0;
	}
	friend bool operator<(const DenseStaticIntSet& left,const DenseStaticIntSet& right)
	{
		return memcmp(left.elements,right.elements,sizeof(DenseStaticIntSet::elements)) < 0;
	}

private:
	typedef uint64 Element;
	enum { indicesPerElement = sizeof(Element) * 8 };
	enum { numElements = (maxIndexPlusOne + indicesPerElement - 1) / indicesPerElement };
	uint64 elements[numElements];
};
