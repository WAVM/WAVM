#pragma once

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Platform/Mutex.h"

namespace WAVM {
	template<typename Key,
			 typename Value,
			 typename KeyHashPolicy = DefaultHashPolicy<Key>,
			 Uptr numStripes = 64>
	struct ConcurrentHashMap
	{
		template<typename... ValueArgs> Value getOrAdd(const Key& key, ValueArgs&&... valueArgs)
		{
			Stripe& stripe = stripes[getStripeIndex(key)];
			Lock<Platform::Mutex> stripeLock(stripe.mutex);
			return stripe.map.getOrAdd(key, std::forward<ValueArgs>(valueArgs)...);
		}

		template<typename... ValueArgs> bool add(const Key& key, ValueArgs&&... valueArgs)
		{
			Stripe& stripe = stripes[getStripeIndex(key)];
			Lock<Platform::Mutex> stripeLock(stripe.mutex);
			return stripe.map.add(key, std::forward<ValueArgs>(valueArgs)...);
		}

		template<typename... ValueArgs> void addOrFail(const Key& key, ValueArgs&&... valueArgs)
		{
			Stripe& stripe = stripes[getStripeIndex(key)];
			Lock<Platform::Mutex> stripeLock(stripe.mutex);
			stripe.map.addOrFail(key, std::forward<ValueArgs>(valueArgs)...);
		}

		template<typename... ValueArgs> void set(const Key& key, ValueArgs&&... valueArgs)
		{
			Stripe& stripe = stripes[getStripeIndex(key)];
			Lock<Platform::Mutex> stripeLock(stripe.mutex);
			stripe.map.set(key, std::forward<ValueArgs>(valueArgs)...);
		}

		bool remove(const Key& key)
		{
			Stripe& stripe = stripes[getStripeIndex(key)];
			Lock<Platform::Mutex> stripeLock(stripe.mutex);
			return stripe.map.remove(key);
		}

		bool contains(const Key& key) const
		{
			Stripe& stripe = stripes[getStripeIndex(key)];
			Lock<Platform::Mutex> stripeLock(stripe.mutex);
			return stripe.map.contains(key);
		}

		const Value operator[](const Key& key) const
		{
			Stripe& stripe = stripes[getStripeIndex(key)];
			Lock<Platform::Mutex> stripeLock(stripe.mutex);
			return stripe.map[key];
		}

		Value get(const Key& key, Value&& nullValue) const
		{
			Stripe& stripe = stripes[getStripeIndex(key)];
			Lock<Platform::Mutex> stripeLock(stripe.mutex);
			Value* value = stripe.map.get(key);
			if(value) { return *value; }
			else
			{
				return nullValue;
			}
		}

	private:
		struct alignas(WAVM::Platform::numCacheLineBytes) Stripe
		{
			WAVM::Platform::Mutex mutex;
			HashMap<Key, Value, KeyHashPolicy> map;
		};

		Stripe stripes[numStripes];

		static Uptr getStripeIndex(const Key& key)
		{
			// Instead of just using the key hash, apply some mixing function so keys end up in
			// different buckets within the stripe for stripe hash tables with numBuckets <=
			// numStripes.
			Uptr hash = KeyHashPolicy::getKeyHash(key);
			if(sizeof(Uptr) == 8)
			{
				// Thomas Wang's 64-bit hash mixing function
				hash = (~hash) + (hash << 21);
				hash = hash ^ (hash >> 24);
				hash = (hash + (hash << 3)) + (hash << 8);
				hash = hash ^ (hash >> 14);
				hash = (hash + (hash << 2)) + (hash << 4);
				hash = hash ^ (hash >> 28);
				hash = hash + (hash << 31);
			}
			else
			{
				// Thomas Wang's 32-bit hash mixing function
				hash = ~hash + (hash << 15);
				hash = hash ^ (hash >> 12);
				hash = hash + (hash << 2);
				hash = hash ^ (hash >> 4);
				hash = hash * 2057;
				hash = hash ^ (hash >> 16);
			}
			return hash & (numStripes - 1);
		}
	};
}
