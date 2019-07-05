// IWYU pragma: private, include "Inline/HashSet.h"
// You should only include this file indirectly by including HashMap.h.

// Use these macros to compress the boilerplate template declarations in a non-inline member
// function definition for HashSet.
#define HASHSET_PARAMETERS
#define HASHSET_ARGUMENTS

template<typename Element> bool HashSetIterator<Element>::operator!=(const HashSetIterator& other)
{
	return bucket != other.bucket;
}

template<typename Element> bool HashSetIterator<Element>::operator==(const HashSetIterator& other)
{
	return bucket == other.bucket;
}

template<typename Element> HashSetIterator<Element>::operator bool() const
{
	return bucket < endBucket && bucket->hashAndOccupancy;
}

template<typename Element> void HashSetIterator<Element>::operator++()
{
	do
	{
		++bucket;
	} while(bucket < endBucket && !bucket->hashAndOccupancy);
}

template<typename Element> const Element& HashSetIterator<Element>::operator*() const
{
	wavmAssert(bucket->hashAndOccupancy);
	return bucket->storage.contents;
}

template<typename Element> const Element* HashSetIterator<Element>::operator->() const
{
	wavmAssert(bucket->hashAndOccupancy);
	return &bucket->storage.contents;
}

template<typename Element>
HashSetIterator<Element>::HashSetIterator(const HashTableBucket<Element>* inBucket,
										  const HashTableBucket<Element>* inEndBucket)
: bucket(inBucket), endBucket(inEndBucket)
{
}

template<typename Element, typename ElementHashPolicy>
HashSet<Element, ElementHashPolicy>::HashSet(Uptr reserveNumElements) : table(reserveNumElements)
{
}

template<typename Element, typename ElementHashPolicy>
HashSet<Element, ElementHashPolicy>::HashSet(const std::initializer_list<Element>& initializerList)
: table(initializerList.size())
{
	for(const Element& element : initializerList)
	{
		const bool result = add(element);
		wavmAssert(result);
	}
}

template<typename Element, typename ElementHashPolicy>
bool HashSet<Element, ElementHashPolicy>::add(const Element& element)
{
	const Uptr hash = ElementHashPolicy::getKeyHash(element);
	HashTableBucket<Element>& bucket = table.getBucketForAdd(hash, element);
	if(bucket.hashAndOccupancy != 0) { return false; }
	else
	{
		bucket.hashAndOccupancy = hash | HashTableBucket<Element>::isOccupiedMask;
		bucket.storage.construct(element);
		return true;
	}
}

template<typename Element, typename ElementHashPolicy>
void HashSet<Element, ElementHashPolicy>::addOrFail(const Element& element)
{
	const Uptr hash = ElementHashPolicy::getKeyHash(element);
	HashTableBucket<Element>& bucket = table.getBucketForAdd(hash, element);
	wavmAssert(!bucket.hashAndOccupancy);
	bucket.hashAndOccupancy = hash | HashTableBucket<Element>::isOccupiedMask;
	bucket.storage.construct(element);
}

template<typename Element, typename ElementHashPolicy>
bool HashSet<Element, ElementHashPolicy>::remove(const Element& element)
{
	return table.remove(ElementHashPolicy::getKeyHash(element), element);
}

template<typename Element, typename ElementHashPolicy>
void HashSet<Element, ElementHashPolicy>::removeOrFail(const Element& element)
{
	const bool removed = table.remove(ElementHashPolicy::getKeyHash(element), element);
	wavmAssert(removed);
}

template<typename Element, typename ElementHashPolicy>
const Element& HashSet<Element, ElementHashPolicy>::operator[](const Element& element) const
{
	const Uptr hash = ElementHashPolicy::getKeyHash(element);
	const HashTableBucket<Element>* bucket = table.getBucketForRead(hash, element);
	wavmAssert(bucket);
	wavmAssert(bucket->hashAndOccupancy == (hash | HashTableBucket<Element>::isOccupiedMask));
	return bucket->storage.contents;
}

template<typename Element, typename ElementHashPolicy>
bool HashSet<Element, ElementHashPolicy>::contains(const Element& element) const
{
	const Uptr hash = ElementHashPolicy::getKeyHash(element);
	const HashTableBucket<Element>* bucket = table.getBucketForRead(hash, element);
	wavmAssert(!bucket
			   || bucket->hashAndOccupancy == (hash | HashTableBucket<Element>::isOccupiedMask));
	return bucket != nullptr;
}

template<typename Element, typename ElementHashPolicy>
const Element* HashSet<Element, ElementHashPolicy>::get(const Element& element) const
{
	const Uptr hash = ElementHashPolicy::getKeyHash(element);
	const HashTableBucket<Element>* bucket = table.getBucketForRead(hash, element);
	if(!bucket) { return nullptr; }
	else
	{
		wavmAssert(bucket->hashAndOccupancy == (hash | HashTableBucket<Element>::isOccupiedMask));
		return &bucket->storage.contents;
	}
}

template<typename Element, typename ElementHashPolicy>
void HashSet<Element, ElementHashPolicy>::clear()
{
	table.clear();
}

template<typename Element, typename ElementHashPolicy>
HashSetIterator<Element> HashSet<Element, ElementHashPolicy>::begin() const
{
	// Find the first occupied bucket.
	HashTableBucket<Element>* beginBucket = table.getBuckets();
	HashTableBucket<Element>* endBucket = table.getBuckets() + table.numBuckets();
	while(beginBucket < endBucket && !beginBucket->hashAndOccupancy) { ++beginBucket; };
	return HashSetIterator<Element>(beginBucket, endBucket);
}

template<typename Element, typename ElementHashPolicy>
HashSetIterator<Element> HashSet<Element, ElementHashPolicy>::end() const
{
	return HashSetIterator<Element>(table.getBuckets() + table.numBuckets(),
									table.getBuckets() + table.numBuckets());
}

template<typename Element, typename ElementHashPolicy>
Uptr HashSet<Element, ElementHashPolicy>::size() const
{
	return table.size();
}

template<typename Element, typename ElementHashPolicy>
void HashSet<Element, ElementHashPolicy>::analyzeSpaceUsage(Uptr& outTotalMemoryBytes,
															Uptr& outMaxProbeCount,
															F32& outOccupancy,
															F32& outAverageProbeCount) const
{
	return table.analyzeSpaceUsage(
		outTotalMemoryBytes, outMaxProbeCount, outOccupancy, outAverageProbeCount);
}

#undef HASHSET_PARAMETERS
#undef HASHSET_ARGUMENTS