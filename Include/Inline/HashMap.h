#pragma once

#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashTable.h"

#include <initializer_list>

template<typename Key, typename Value>
struct HashMapPair
{
	Key key;
	Value value;
	
	template<typename... ValueArgs>
	HashMapPair(const Key& inKey, ValueArgs&&... valueArgs);
	
	template<typename... ValueArgs>
	HashMapPair(Key&& inKey, ValueArgs&&... valueArgs);
};

template<typename Key, typename Value>
struct HashMapIterator
{
	template<typename,typename,typename>
	friend struct HashMap;

	typedef HashMapPair<Key, Value> Pair;

	bool operator!=(const HashMapIterator& other);
	bool operator==(const HashMapIterator& other);
	operator bool() const;
	void operator++();
		
	const Pair& operator*() const;
	const Pair* operator->() const;

	const Key& key() const;
	const Value& value() const;

private:

	const HashTableBucket<Pair>* bucket;
	const HashTableBucket<Pair>* endBucket;

	HashMapIterator(const HashTableBucket<Pair>* inBucket, const HashTableBucket<Pair>* inEndBucket);
};

template<typename Key, typename Value, typename KeyHashPolicy = DefaultHashPolicy<Key>>
struct HashMap
{
	typedef HashMapPair<Key, Value> Pair;
	typedef HashMapIterator<Key, Value> Iterator;

	HashMap(Uptr reserveNumPairs = 0);
	HashMap(const std::initializer_list<Pair>& initializerList);
	
	// If the map contains the key already, returns the value bound to that key.
	// If the map doesn't contain the key, adds it to the map bound to a value constructed from the
	// provided arguments.
	template<typename... ValueArgs>
	Value& getOrAdd(const Key& key, ValueArgs&&... valueArgs);

	// If the map contains the key already, returns false.
	// If the map doesn't contain the key, adds it to the map bound to a value constructed from the
	// provided arguments, and returns true.
	template<typename... ValueArgs>
	bool add(const Key& key, ValueArgs&&... valueArgs);
	
	// If the map contains the key already, replaces the value bound to it with a value constructed
	// from the provided arguments.
	// If the map doesn't contain the key, adds it to the map bound to a value constructed from the
	// provided arguments.
	// In both cases, a reference to the value bound to the key is returned.
	template<typename... ValueArgs>
	Value& set(const Key& key, ValueArgs&&... valueArgs);

	// If the map contains the key, removes it and returns true.
	// If the map doesn't contain the key, returns false.
	bool remove(const Key& key);

	// Returns true if the map contains the key.
	bool contains(const Key& key) const;

	// Returns a reference to the value bound to the key. Assumes that the map contains the key.
	const Value& operator[](const Key& key) const;

	// Returns a pointer to the value bound to the key, or null if the map doesn't contain the key.
	const Value* get(const Key& key) const;
	
	// Returns a pointer to the key-value pair for a key, or null if the map doesn't contain the key.
	const Pair* getPair(const Key& key) const;
	
	Iterator begin() const;
	Iterator end() const;

	Uptr num() const;

	// Compute some statistics about the space usage of this map.
	void analyzeSpaceUsage(
		Uptr& outTotalMemoryBytes,
		Uptr& outMaxProbeCount,
		F32& outOccupancy,
		F32& outAverageProbeCount
		) const;

private:

	struct HashTablePolicy
	{
		FORCEINLINE static const Key& getKey(const Pair& pair)
		{
			return pair.key;
		}
		FORCEINLINE static bool areKeysEqual(const Key& left, const Key& right)
		{
			return KeyHashPolicy::areKeysEqual(left, right);
		}
	};

	HashTable<Key, Pair, HashTablePolicy> table;
};

// The implementation is defined in a separate file.
#include "HashMapImpl.h"