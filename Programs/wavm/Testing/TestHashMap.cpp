#include <stdlib.h>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "TestUtils.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Platform/Alloca.h"
#include "WAVM/Platform/Defines.h"
#include "wavm-test.h"

using namespace WAVM;
using namespace WAVM::Testing;

static std::string generateRandomString()
{
	static constexpr Uptr maxChars = 16;

	const Uptr numChars = rand() % maxChars;
	char* buffer = (char*)alloca(numChars + 1);
	for(Uptr charIndex = 0; charIndex < numChars; ++charIndex)
	{
		buffer[charIndex] = 0x20 + (rand() % (0x7E - 0x20));
	}
	buffer[numChars] = 0;
	return std::string(buffer);
}

static void testStringMap(TEST_STATE_PARAM)
{
	static constexpr Uptr numStrings = 1000;

	HashMap<std::string, U32> map;
	std::vector<HashMapPair<std::string, U32>> pairs;

	srand(0);

	for(Uptr i = 0; i < numStrings; ++i)
	{
		while(true)
		{
			std::string randomString = generateRandomString();

			bool alreadySawString = false;
			for(const HashMapPair<std::string, U32>& pair : pairs)
			{
				if(pair.key == randomString)
				{
					alreadySawString = true;
					break;
				}
			}

			if(!alreadySawString)
			{
				pairs.emplace_back(std::move(randomString), rand());
				break;
			}
		};
	}

	for(Uptr i = 0; i < pairs.size(); ++i)
	{
		CHECK_TRUE(map.add(pairs[i].key, pairs[i].value));
		CHECK_FALSE(map.add(pairs[i].key, pairs[i].value));

		for(Uptr j = 0; j < pairs.size(); ++j)
		{
			const U32* valuePtr = map.get(pairs[j].key);
			if(j <= i)
			{
				CHECK_NOT_NULL(valuePtr);
				if(valuePtr) { CHECK_EQ(*valuePtr, pairs[j].value); }
			}
			else
			{
				CHECK_NULL(valuePtr);
			}
		}
	}

	for(Uptr i = 0; i < pairs.size(); ++i)
	{
		CHECK_TRUE(map.remove(pairs[i].key));
		CHECK_FALSE(map.remove(pairs[i].key));

		for(Uptr j = 0; j < pairs.size(); ++j)
		{
			const U32* valuePtr = map.get(pairs[j].key);
			if(j > i)
			{
				CHECK_NOT_NULL(valuePtr);
				if(valuePtr) { CHECK_EQ(*valuePtr, pairs[j].value); }
			}
			else
			{
				CHECK_NULL(valuePtr);
			}
		}
	}
}

static void testU32Map(TEST_STATE_PARAM)
{
	HashMap<U32, U32> map;

	static constexpr Uptr maxI = 1024 * 1024;

	for(Uptr i = 0; i < maxI; ++i) { CHECK_FALSE(map.contains(U32(i))); }

	CHECK_EQ(map.size(), Uptr(0));
	for(Uptr i = 0; i < maxI; ++i)
	{
		CHECK_FALSE(map.contains(U32(i)));
		CHECK_NULL(map.get(U32(i)));
		CHECK_TRUE(map.add(U32(i), U32(i * 2)));
		CHECK_TRUE(map.contains(U32(i)));
		CHECK_EQ(map[U32(i)], U32(i * 2));
		CHECK_EQ(map.size(), i + 1);
	}

	for(Uptr i = 0; i < maxI; ++i)
	{
		CHECK_TRUE(map.contains(U32(i)));
		CHECK_TRUE(map.remove(U32(i)));
		CHECK_FALSE(map.contains(U32(i)));
		CHECK_EQ(map.size(), maxI - i - 1);
	}

	for(Uptr i = 0; i < maxI; ++i) { CHECK_FALSE(map.contains(U32(i))); }
}

static void testMapCopy(TEST_STATE_PARAM)
{
	// Add 1000..1999 to a HashMap.
	HashMap<Uptr, Uptr> a;
	for(Uptr i = 0; i < 1000; ++i) { a.add(i + 1000, i); }

	// Copy the map to a new HashMap.
	HashMap<Uptr, Uptr> b{a};

	// Test that both the new and old HashMap contain the expected numbers.
	for(Uptr i = 0; i < 1000; ++i)
	{
		CHECK_NULL(a.get(i));
		CHECK_EQ(a[i + 1000], i);
		CHECK_NULL(a.get(i + 2000));

		CHECK_NULL(b.get(i));
		CHECK_EQ(b[i + 1000], i);
		CHECK_NULL(b.get(i + 2000));
	}

	// Test copying a map from itself.
	WAVM_SCOPED_DISABLE_SELF_ASSIGN_WARNINGS(b = b;)

	// Test that the map wasn't changed by the copy-to-self.
	for(Uptr i = 0; i < 1000; ++i)
	{
		CHECK_NULL(b.get(i));
		CHECK_EQ(b[i + 1000], i);
		CHECK_NULL(b.get(i + 2000));
	}

	// Test removing an element from the map.
	b.remove(1000);
	CHECK_EQ(a[1000], Uptr(0));
	CHECK_NULL(b.get(1000));
}

static void testMapMove(TEST_STATE_PARAM)
{
	// Add 1000..1999 to a HashMap.
	HashMap<Uptr, Uptr> a;
	for(Uptr i = 0; i < 1000; ++i) { a.add(i + 1000, i); }

	// Move the map to a new HashMap.
	HashMap<Uptr, Uptr> b{std::move(a)};

	// Test that the new HashMap contains the expected numbers.
	for(Uptr i = 0; i < 1000; ++i)
	{
		CHECK_NULL(b.get(i));
		CHECK_EQ(b[i + 1000], i);
		CHECK_NULL(b.get(i + 2000));
	}

	// Test moving the map to itself.
	WAVM_SCOPED_DISABLE_SELF_ASSIGN_WARNINGS(b = std::move(b);)

	// Test that the map wasn't changed by the move-to-self.
	for(Uptr i = 0; i < 1000; ++i)
	{
		CHECK_NULL(b.get(i));
		CHECK_EQ(b[i + 1000], i);
		CHECK_NULL(b.get(i + 2000));
	}
}

static void testMapInitializerList(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map{{1, 1}, {3, 2}, {5, 3}, {7, 4}, {11, 5}, {13, 6}, {17, 7}};
	CHECK_NULL(map.get(0));
	CHECK_EQ(map[1], Uptr(1));
	CHECK_NULL(map.get(2));
	CHECK_EQ(map[3], Uptr(2));
	CHECK_NULL(map.get(4));
	CHECK_EQ(map[5], Uptr(3));
	CHECK_NULL(map.get(6));
	CHECK_EQ(map[7], Uptr(4));
	CHECK_NULL(map.get(8));
	CHECK_NULL(map.get(9));
	CHECK_NULL(map.get(10));
	CHECK_EQ(map[11], Uptr(5));
	CHECK_NULL(map.get(12));
	CHECK_EQ(map[13], Uptr(6));
	CHECK_NULL(map.get(14));
	CHECK_NULL(map.get(15));
	CHECK_NULL(map.get(16));
	CHECK_EQ(map[17], Uptr(7));
}

static void testMapIterator(TEST_STATE_PARAM)
{
	// Add 1..9 to a HashMap.
	HashMap<Uptr, Uptr> a;
	for(Uptr i = 1; i < 10; ++i) { a.add(i, i * 2); }

	// 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 = 45
	{
		Uptr keySum = 0;
		Uptr valueSum = 0;
		for(const auto& pair : a)
		{
			keySum += pair.key;
			valueSum += pair.value;
		}
		CHECK_EQ(keySum, Uptr(45));
		CHECK_EQ(valueSum, Uptr(90));
	}

	// Remove 5.
	a.remove(5);

	// 1 + 2 + 3 + 4 + 6 + 7 + 8 + 9 = 40
	{
		Uptr keySum = 0;
		Uptr valueSum = 0;
		for(const auto& pair : a)
		{
			keySum += pair.key;
			valueSum += pair.value;
		}
		CHECK_EQ(keySum, Uptr(40));
		CHECK_EQ(valueSum, Uptr(80));
	}
}

static void testMapGetOrAdd(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map;

	CHECK_NULL(map.get(0));
	CHECK_EQ(map.getOrAdd(0, 1), Uptr(1));
	CHECK_EQ(map[0], Uptr(1));
	CHECK_EQ(map.getOrAdd(0, 3), Uptr(1));
	CHECK_EQ(map[0], Uptr(1));
	CHECK_EQ((map.getOrAdd(0, 5) += 7), Uptr(8));
	CHECK_EQ(map[0], Uptr(8));
}

static void testMapSet(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map;

	CHECK_NULL(map.get(0));
	CHECK_EQ(map.set(0, 1), Uptr(1));
	CHECK_EQ(map[0], Uptr(1));
	CHECK_EQ(map.set(0, 3), Uptr(3));
	CHECK_EQ(map[0], Uptr(3));
}

struct EmplacedValue
{
	std::string a;
	std::string b;

	EmplacedValue(const std::string& inA, const std::string& inB) : a(inA), b(inB) {}
};

static void testMapEmplace(TEST_STATE_PARAM)
{
	HashMap<Uptr, EmplacedValue> map;

	EmplacedValue& a = map.getOrAdd(0, "a", "b");
	CHECK_EQ(a.a, std::string("a"));
	CHECK_EQ(a.b, std::string("b"));

	CHECK_TRUE(map.add(1, "c", "d"));
	const EmplacedValue* bPtr = map.get(1);
	CHECK_NOT_NULL(bPtr);
	if(bPtr)
	{
		CHECK_EQ(bPtr->a, std::string("c"));
		CHECK_EQ(bPtr->b, std::string("d"));
	}

	EmplacedValue& c = map.set(2, "e", "f");
	CHECK_EQ(c.a, std::string("e"));
	CHECK_EQ(c.b, std::string("f"));

	EmplacedValue& d = map.getOrAdd(2, "g", "h");
	CHECK_EQ(d.a, std::string("e"));
	CHECK_EQ(d.b, std::string("f"));
}

static void testMapBracketOperator(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map{{1, 1}, {3, 2}, {5, 3}, {7, 4}, {11, 5}, {13, 6}, {17, 7}};
	CHECK_EQ(map[1], Uptr(1));
	CHECK_EQ(map[3], Uptr(2));
	CHECK_EQ(map[5], Uptr(3));
	CHECK_EQ(map[7], Uptr(4));
	CHECK_EQ(map[11], Uptr(5));
	CHECK_EQ(map[13], Uptr(6));
	CHECK_EQ(map[17], Uptr(7));
}

static void testMapEmpty(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map;

	CHECK_EQ(map.size(), Uptr(0));
	CHECK_FALSE(map.contains(0));
	CHECK_NULL(map.get(0));
	CHECK_FALSE(map.remove(0));

	// Iteration over empty map
	Uptr count = 0;
	for(const auto& pair : map)
	{
		(void)pair;
		++count;
	}
	CHECK_EQ(count, Uptr(0));

	// begin() == end() for empty map
	CHECK_TRUE(map.begin() == map.end());
}

static void testMapClear(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map;
	for(Uptr i = 0; i < 100; ++i) { map.add(i, i * 2); }
	CHECK_EQ(map.size(), Uptr(100));

	map.clear();
	CHECK_EQ(map.size(), Uptr(0));
	CHECK_FALSE(map.contains(0));
	CHECK_FALSE(map.contains(50));
	CHECK_FALSE(map.contains(99));

	// Can add again after clear
	CHECK_TRUE(map.add(42, 84));
	CHECK_EQ(map[42], Uptr(84));
}

static void testMapGetPair(TEST_STATE_PARAM)
{
	HashMap<std::string, Uptr> map;
	map.add("hello", 1);
	map.add("world", 2);

	const HashMapPair<std::string, Uptr>* pair = map.getPair("hello");
	CHECK_NOT_NULL(pair);
	if(pair)
	{
		CHECK_EQ(pair->key, std::string("hello"));
		CHECK_EQ(pair->value, Uptr(1));
	}

	CHECK_NULL(map.getPair("missing"));
}

static void testMapReserve(TEST_STATE_PARAM)
{
	// Constructor with reserve
	HashMap<Uptr, Uptr> map(1000);

	// Should be able to add many elements efficiently
	for(Uptr i = 0; i < 1000; ++i) { map.add(i, i); }

	CHECK_EQ(map.size(), Uptr(1000));
	for(Uptr i = 0; i < 1000; ++i) { CHECK_EQ(map[i], i); }
}

static void testMapAddOrFail(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map;

	// addOrFail on empty map should succeed
	map.addOrFail(1, 10);
	CHECK_EQ(map[1], Uptr(10));
}

static void testMapRemoveOrFail(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map;
	map.add(1, 10);

	// removeOrFail on existing key should succeed
	map.removeOrFail(1);
	CHECK_FALSE(map.contains(1));
}

static void testMapAnalyzeSpaceUsage(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map;
	for(Uptr i = 0; i < 100; ++i) { map.add(i, i); }

	Uptr totalMemoryBytes = 0;
	Uptr maxProbeCount = 0;
	F32 occupancy = 0;
	F32 averageProbeCount = 0;

	map.analyzeSpaceUsage(totalMemoryBytes, maxProbeCount, occupancy, averageProbeCount);

	CHECK_GT(totalMemoryBytes, Uptr(0));
	CHECK_GT(occupancy, 0.0f);
	CHECK_LE(occupancy, 1.0f);
	CHECK_GE(averageProbeCount, 1.0f);
}

static void testMapAnalyzeSpaceUsageAfterClear(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> map;
	for(Uptr i = 0; i < 100; ++i) { map.add(i, i); }
	map.clear();

	// After clear, analyzeSpaceUsage should not crash (division by zero)
	Uptr totalMemoryBytes = 0;
	Uptr maxProbeCount = 0;
	F32 occupancy = 0;
	F32 averageProbeCount = 0;

	map.analyzeSpaceUsage(totalMemoryBytes, maxProbeCount, occupancy, averageProbeCount);

	CHECK_EQ(map.size(), Uptr(0));
}

static void testMapMovedFromState(TEST_STATE_PARAM)
{
	HashMap<Uptr, Uptr> a;
	for(Uptr i = 0; i < 100; ++i) { a.add(i, i * 2); }

	// Move to b
	HashMap<Uptr, Uptr> b{std::move(a)};

	// The moved-from map 'a' should be in a valid, empty state
	CHECK_EQ(a.size(), Uptr(0));

	// Iterating over moved-from map should be safe (begin == end)
	Uptr count = 0;
	for(const auto& pair : a)
	{
		(void)pair;
		++count;
	}
	CHECK_EQ(count, Uptr(0));

	// Should be able to add to moved-from map
	CHECK_TRUE(a.add(42, 84));
	CHECK_EQ(a[42], Uptr(84));
}

// Custom hash policy that causes all keys to hash to the same bucket
struct CollidingHashPolicy
{
	static bool areKeysEqual(Uptr left, Uptr right) { return left == right; }
	static Uptr getKeyHash(Uptr) { return 0; } // All keys hash to 0
};

static void testMapHashCollisions(TEST_STATE_PARAM)
{
	// Use a map where all keys collide
	HashMap<Uptr, Uptr, CollidingHashPolicy> map;

	// Add many elements that all hash to the same bucket
	static constexpr Uptr numElements = 100;
	for(Uptr i = 0; i < numElements; ++i) { CHECK_TRUE(map.add(i, i * 2)); }

	CHECK_EQ(map.size(), numElements);

	// Verify all elements can be found
	for(Uptr i = 0; i < numElements; ++i)
	{
		CHECK_TRUE(map.contains(i));
		CHECK_EQ(map[i], i * 2);
	}

	// Remove every other element
	for(Uptr i = 0; i < numElements; i += 2) { CHECK_TRUE(map.remove(i)); }

	CHECK_EQ(map.size(), numElements / 2);

	// Verify remaining elements
	for(Uptr i = 0; i < numElements; ++i)
	{
		if(i % 2 == 0) { CHECK_FALSE(map.contains(i)); }
		else
		{
			CHECK_TRUE(map.contains(i));
			CHECK_EQ(map[i], i * 2);
		}
	}
}

static void testMapAnalyzeSpaceUsageEmpty(TEST_STATE_PARAM)
{
	// Test analyzeSpaceUsage on a freshly constructed empty map
	HashMap<Uptr, Uptr> map;

	Uptr totalMemoryBytes = 0;
	Uptr maxProbeCount = 0;
	F32 occupancy = 0;
	F32 averageProbeCount = 0;

	map.analyzeSpaceUsage(totalMemoryBytes, maxProbeCount, occupancy, averageProbeCount);

	CHECK_EQ(map.size(), Uptr(0));
}

static void testMapTrackedValueDestruction(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	{
		HashMap<TrackedValue, TrackedValue> map;
		map.add(TrackedValue(realm, 1), TrackedValue(realm, 10));
		map.add(TrackedValue(realm, 2), TrackedValue(realm, 20));
		map.add(TrackedValue(realm, 3), TrackedValue(realm, 30));
		CHECK_EQ(map.size(), Uptr(3));
	}

	// All elements should be destroyed when map goes out of scope
	CHECK_TRUE(realm.allDestroyed());
}

static void testMapTrackedValueClear(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	HashMap<TrackedValue, TrackedValue> map;
	map.add(TrackedValue(realm, 1), TrackedValue(realm, 10));
	map.add(TrackedValue(realm, 2), TrackedValue(realm, 20));
	map.add(TrackedValue(realm, 3), TrackedValue(realm, 30));

	// Capture HeapValues before clear
	std::vector<std::shared_ptr<HeapValue>> heapValuesBefore;
	for(const auto& pair : map)
	{
		heapValuesBefore.push_back(pair.key.heapValue);
		heapValuesBefore.push_back(pair.value.heapValue);
	}

	map.clear();

	// All captured HeapValues should be marked destroyed
	for(const auto& hv : heapValuesBefore) { CHECK_TRUE(hv->destroyed); }
}

static void testMapTrackedValueRemove(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	HashMap<Uptr, TrackedValue> map;
	map.add(1, TrackedValue(realm, 10));
	map.add(2, TrackedValue(realm, 20));
	map.add(3, TrackedValue(realm, 30));

	// Capture HeapValue for key 2
	TrackedValue* tv2 = map.get(2);
	WAVM_ERROR_UNLESS(tv2);
	std::shared_ptr<HeapValue> hv2 = tv2->heapValue;
	CHECK_FALSE(hv2->destroyed);

	map.remove(2);

	// HeapValue for removed element should be destroyed
	CHECK_TRUE(hv2->destroyed);
}

static void testMapTrackedValueContainerMove(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	HashMap<TrackedValue, TrackedValue> a;
	a.add(TrackedValue(realm, 1), TrackedValue(realm, 10));
	a.add(TrackedValue(realm, 2), TrackedValue(realm, 20));

	// Capture original HeapValues
	std::set<std::shared_ptr<HeapValue>> originalHeapValues;
	for(const auto& pair : a)
	{
		originalHeapValues.insert(pair.key.heapValue);
		originalHeapValues.insert(pair.value.heapValue);
	}

	// Move container
	HashMap<TrackedValue, TrackedValue> b = std::move(a);

	// Original HeapValues should NOT be destroyed (ownership transferred)
	for(const auto& hv : originalHeapValues) { CHECK_FALSE(hv->destroyed); }

	// Elements in b should have the same HeapValues (no copy/move of elements)
	for(const auto& pair : b)
	{
		CHECK_TRUE(originalHeapValues.count(pair.key.heapValue) == 1);
		CHECK_TRUE(originalHeapValues.count(pair.value.heapValue) == 1);
	}
}

static void testMapTrackedValueContainerCopy(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	HashMap<TrackedValue, TrackedValue> a;
	a.add(TrackedValue(realm, 1), TrackedValue(realm, 10));
	a.add(TrackedValue(realm, 2), TrackedValue(realm, 20));

	// Capture original HeapValues
	std::set<std::shared_ptr<HeapValue>> originalHeapValues;
	for(const auto& pair : a)
	{
		originalHeapValues.insert(pair.key.heapValue);
		originalHeapValues.insert(pair.value.heapValue);
	}

	// Copy container
	HashMap<TrackedValue, TrackedValue> b{a};

	// Original HeapValues should NOT be destroyed (a still exists)
	for(const auto& hv : originalHeapValues) { CHECK_FALSE(hv->destroyed); }

	// Elements in b should have DIFFERENT HeapValues (copies were made)
	for(const auto& pair : b)
	{
		CHECK_TRUE(originalHeapValues.count(pair.key.heapValue) == 0);
		CHECK_TRUE(originalHeapValues.count(pair.value.heapValue) == 0);

		// Elide any internal moves to find the underlying provenance
		auto keyElided = pair.key.heapValue->elideMoves();
		auto valueElided = pair.value.heapValue->elideMoves();
		WAVM_ERROR_UNLESS(keyElided);
		WAVM_ERROR_UNLESS(valueElided);

		// Should be CopyConstructed with source pointing to originals
		CHECK_EQ(keyElided->provenance, HeapValue::Provenance::CopyConstructed);
		CHECK_EQ(valueElided->provenance, HeapValue::Provenance::CopyConstructed);
		CHECK_TRUE(originalHeapValues.count(keyElided->source) == 1);
		CHECK_TRUE(originalHeapValues.count(valueElided->source) == 1);
	}
}

I32 execHashMapTest(int argc, char** argv)
{
	TEST_STATE_LOCAL;
	Timing::Timer timer;

	testStringMap(TEST_STATE_ARG);
	testU32Map(TEST_STATE_ARG);
	testMapCopy(TEST_STATE_ARG);
	testMapMove(TEST_STATE_ARG);
	testMapInitializerList(TEST_STATE_ARG);
	testMapIterator(TEST_STATE_ARG);
	testMapGetOrAdd(TEST_STATE_ARG);
	testMapSet(TEST_STATE_ARG);
	testMapEmplace(TEST_STATE_ARG);
	testMapBracketOperator(TEST_STATE_ARG);
	testMapEmpty(TEST_STATE_ARG);
	testMapClear(TEST_STATE_ARG);
	testMapGetPair(TEST_STATE_ARG);
	testMapReserve(TEST_STATE_ARG);
	testMapAddOrFail(TEST_STATE_ARG);
	testMapRemoveOrFail(TEST_STATE_ARG);
	testMapAnalyzeSpaceUsage(TEST_STATE_ARG);
	testMapAnalyzeSpaceUsageAfterClear(TEST_STATE_ARG);
	testMapAnalyzeSpaceUsageEmpty(TEST_STATE_ARG);
	testMapMovedFromState(TEST_STATE_ARG);
	testMapHashCollisions(TEST_STATE_ARG);
	testMapTrackedValueDestruction(TEST_STATE_ARG);
	testMapTrackedValueClear(TEST_STATE_ARG);
	testMapTrackedValueRemove(TEST_STATE_ARG);
	testMapTrackedValueContainerMove(TEST_STATE_ARG);
	testMapTrackedValueContainerCopy(TEST_STATE_ARG);

	Timing::logTimer("HashMapTest", timer);
	return testState.exitCode();
}
