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
#include "WAVM/Inline/HashSet.h"
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

static void testStringSet(TEST_STATE_PARAM)
{
	static constexpr Uptr numStrings = 1000;

	HashSet<std::string> set;
	std::vector<std::string> strings;

	srand(0);

	for(Uptr i = 0; i < numStrings; ++i)
	{
		while(true)
		{
			std::string randomString = generateRandomString();

			bool alreadySawString = false;
			for(const std::string& string : strings)
			{
				if(string == randomString)
				{
					alreadySawString = true;
					break;
				}
			}

			if(!alreadySawString)
			{
				strings.push_back(std::move(randomString));
				break;
			}
		};
	}

	for(Uptr i = 0; i < strings.size(); ++i)
	{
		CHECK_TRUE(set.add(strings[i]));
		CHECK_FALSE(set.add(strings[i]));

		for(Uptr j = 0; j < strings.size(); ++j)
		{
			const bool expectedContains = j <= i;
			CHECK_EQ(set.contains(strings[j]), expectedContains);
		}
	}

	for(Uptr i = 0; i < strings.size(); ++i)
	{
		CHECK_TRUE(set.remove(strings[i]));
		CHECK_FALSE(set.remove(strings[i]));

		for(Uptr j = 0; j < strings.size(); ++j)
		{
			const bool expectedContains = j > i;
			CHECK_EQ(set.contains(strings[j]), expectedContains);
		}
	}
}

static void testU32Set(TEST_STATE_PARAM)
{
	HashSet<U32> set;

	static constexpr Uptr maxI = 1024 * 1024;

	for(Uptr i = 0; i < maxI; ++i) { CHECK_FALSE(set.contains(U32(i))); }

	CHECK_EQ(set.size(), Uptr(0));
	for(Uptr i = 0; i < maxI; ++i)
	{
		CHECK_FALSE(set.contains(U32(i)));
		CHECK_NULL(set.get(U32(i)));
		CHECK_TRUE(set.add(U32(i)));
		CHECK_TRUE(set.contains(U32(i)));
		CHECK_NOT_NULL(set.get(U32(i)));
		CHECK_EQ(set.size(), i + 1);
	}

	for(Uptr i = 0; i < maxI; ++i)
	{
		CHECK_TRUE(set.contains(U32(i)));
		CHECK_TRUE(set.remove(U32(i)));
		CHECK_FALSE(set.contains(U32(i)));
		CHECK_EQ(set.size(), maxI - i - 1);
	}

	for(Uptr i = 0; i < maxI; ++i) { CHECK_FALSE(set.contains(U32(i))); }
}

static void testSetCopy(TEST_STATE_PARAM)
{
	// Add 1000..1999 to a HashSet.
	HashSet<Uptr> a;
	for(Uptr i = 0; i < 1000; ++i) { a.add(i + 1000); }

	// Copy the set to a new HashSet.
	HashSet<Uptr> b{a};

	// Test that both the new and old HashSet contain the expected numbers.
	for(Uptr i = 0; i < 1000; ++i)
	{
		CHECK_FALSE(a.contains(i));
		CHECK_TRUE(a.contains(i + 1000));
		CHECK_FALSE(a.contains(i + 2000));

		CHECK_FALSE(b.contains(i));
		CHECK_TRUE(b.contains(i + 1000));
		CHECK_FALSE(b.contains(i + 2000));
	}

	// Test copying a set from itself.
	WAVM_SCOPED_DISABLE_SELF_ASSIGN_WARNINGS(b = b;)

	// Test that the set wasn't changed by the copy-to-self.
	for(Uptr i = 0; i < 1000; ++i)
	{
		CHECK_FALSE(b.contains(i));
		CHECK_TRUE(b.contains(i + 1000));
		CHECK_FALSE(b.contains(i + 2000));
	}

	// Test removing an element from the set.
	b.remove(1000);
	CHECK_TRUE(a.contains(1000));
	CHECK_FALSE(b.contains(1000));
}

static void testSetMove(TEST_STATE_PARAM)
{
	// Add 1000..1999 to a HashSet.
	HashSet<Uptr> a;
	for(Uptr i = 0; i < 1000; ++i) { a.add(i + 1000); }

	// Move the set to a new HashSet.
	HashSet<Uptr> b{std::move(a)};

	// Test that the new HashSet contains the expected numbers.
	for(Uptr i = 0; i < 1000; ++i)
	{
		CHECK_FALSE(b.contains(i));
		CHECK_TRUE(b.contains(i + 1000));
		CHECK_FALSE(b.contains(i + 2000));
	}

	// Test moving the set to itself.
	WAVM_SCOPED_DISABLE_SELF_ASSIGN_WARNINGS(b = std::move(b);)

	// Test that the set wasn't changed by the move-to-self.
	for(Uptr i = 0; i < 1000; ++i)
	{
		CHECK_FALSE(b.contains(i));
		CHECK_TRUE(b.contains(i + 1000));
		CHECK_FALSE(b.contains(i + 2000));
	}
}

static void testSetInitializerList(TEST_STATE_PARAM)
{
	HashSet<Uptr> set{1, 3, 5, 7, 11, 13, 17};
	CHECK_FALSE(set.contains(0));
	CHECK_TRUE(set.contains(1));
	CHECK_FALSE(set.contains(2));
	CHECK_TRUE(set.contains(3));
	CHECK_FALSE(set.contains(4));
	CHECK_TRUE(set.contains(5));
	CHECK_FALSE(set.contains(6));
	CHECK_TRUE(set.contains(7));
	CHECK_FALSE(set.contains(8));
	CHECK_FALSE(set.contains(9));
	CHECK_FALSE(set.contains(10));
	CHECK_TRUE(set.contains(11));
	CHECK_FALSE(set.contains(12));
	CHECK_TRUE(set.contains(13));
	CHECK_FALSE(set.contains(14));
	CHECK_FALSE(set.contains(15));
	CHECK_FALSE(set.contains(16));
	CHECK_TRUE(set.contains(17));
}

static void testSetIterator(TEST_STATE_PARAM)
{
	// Add 1..9 to a HashSet.
	HashSet<Uptr> a;
	for(Uptr i = 1; i < 10; ++i) { a.add(i); }

	// 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 = 45
	{
		Uptr sum = 0;
		for(Uptr i : a) { sum += i; }
		CHECK_EQ(sum, Uptr(45));
	}

	// Remove 5.
	a.remove(5);

	// 1 + 2 + 3 + 4 + 6 + 7 + 8 + 9 = 40
	{
		Uptr sum = 0;
		for(Uptr i : a) { sum += i; }
		CHECK_EQ(sum, Uptr(40));
	}
}

static void testSetBracketOperator(TEST_STATE_PARAM)
{
	HashSet<Uptr> set{1, 3, 5, 7, 11, 13, 17};
	CHECK_EQ(set[1], Uptr(1));
	CHECK_EQ(set[3], Uptr(3));
	CHECK_EQ(set[5], Uptr(5));
	CHECK_EQ(set[7], Uptr(7));
	CHECK_EQ(set[11], Uptr(11));
	CHECK_EQ(set[13], Uptr(13));
	CHECK_EQ(set[17], Uptr(17));
}

static void testSetEmpty(TEST_STATE_PARAM)
{
	HashSet<Uptr> set;

	CHECK_EQ(set.size(), Uptr(0));
	CHECK_FALSE(set.contains(0));
	CHECK_NULL(set.get(0));
	CHECK_FALSE(set.remove(0));

	// Iteration over empty set
	Uptr count = 0;
	for(Uptr value : set)
	{
		(void)value;
		++count;
	}
	CHECK_EQ(count, Uptr(0));

	// begin() == end() for empty set
	CHECK_TRUE(set.begin() == set.end());
}

static void testSetClear(TEST_STATE_PARAM)
{
	HashSet<Uptr> set;
	for(Uptr i = 0; i < 100; ++i) { set.add(i); }
	CHECK_EQ(set.size(), Uptr(100));

	set.clear();
	CHECK_EQ(set.size(), Uptr(0));
	CHECK_FALSE(set.contains(0));
	CHECK_FALSE(set.contains(50));
	CHECK_FALSE(set.contains(99));

	// Can add again after clear
	CHECK_TRUE(set.add(42));
	CHECK_TRUE(set.contains(42));
}

static void testSetReserve(TEST_STATE_PARAM)
{
	// Constructor with reserve
	HashSet<Uptr> set(1000);

	// Should be able to add many elements efficiently
	for(Uptr i = 0; i < 1000; ++i) { set.add(i); }

	CHECK_EQ(set.size(), Uptr(1000));
	for(Uptr i = 0; i < 1000; ++i) { CHECK_TRUE(set.contains(i)); }
}

static void testSetAddOrFail(TEST_STATE_PARAM)
{
	HashSet<Uptr> set;

	// addOrFail on empty set should succeed
	set.addOrFail(1);
	CHECK_TRUE(set.contains(1));
}

static void testSetRemoveOrFail(TEST_STATE_PARAM)
{
	HashSet<Uptr> set;
	set.add(1);

	// removeOrFail on existing key should succeed
	set.removeOrFail(1);
	CHECK_FALSE(set.contains(1));
}

static void testSetAnalyzeSpaceUsage(TEST_STATE_PARAM)
{
	HashSet<Uptr> set;
	for(Uptr i = 0; i < 100; ++i) { set.add(i); }

	Uptr totalMemoryBytes = 0;
	Uptr maxProbeCount = 0;
	F32 occupancy = 0;
	F32 averageProbeCount = 0;

	set.analyzeSpaceUsage(totalMemoryBytes, maxProbeCount, occupancy, averageProbeCount);

	CHECK_GT(totalMemoryBytes, Uptr(0));
	CHECK_GT(occupancy, 0.0f);
	CHECK_LE(occupancy, 1.0f);
	CHECK_GE(averageProbeCount, 1.0f);
}

static void testSetAnalyzeSpaceUsageAfterClear(TEST_STATE_PARAM)
{
	HashSet<Uptr> set;
	for(Uptr i = 0; i < 100; ++i) { set.add(i); }
	set.clear();

	// After clear, analyzeSpaceUsage should not crash (division by zero)
	Uptr totalMemoryBytes = 0;
	Uptr maxProbeCount = 0;
	F32 occupancy = 0;
	F32 averageProbeCount = 0;

	set.analyzeSpaceUsage(totalMemoryBytes, maxProbeCount, occupancy, averageProbeCount);

	CHECK_EQ(set.size(), Uptr(0));
}

static void testSetMovedFromState(TEST_STATE_PARAM)
{
	HashSet<Uptr> a;
	for(Uptr i = 0; i < 100; ++i) { a.add(i); }

	// Move to b
	HashSet<Uptr> b{std::move(a)};

	// The moved-from set 'a' should be in a valid, empty state
	CHECK_EQ(a.size(), Uptr(0));

	// Iterating over moved-from set should be safe (begin == end)
	Uptr count = 0;
	for(Uptr value : a)
	{
		(void)value;
		++count;
	}
	CHECK_EQ(count, Uptr(0));

	// Should be able to add to moved-from set
	CHECK_TRUE(a.add(42));
	CHECK_TRUE(a.contains(42));
}

// Custom hash policy that causes all elements to hash to the same bucket
struct CollidingHashPolicy
{
	static bool areKeysEqual(Uptr left, Uptr right) { return left == right; }
	static Uptr getKeyHash(Uptr) { return 0; } // All elements hash to 0
};

static void testSetHashCollisions(TEST_STATE_PARAM)
{
	// Use a set where all elements collide
	HashSet<Uptr, CollidingHashPolicy> set;

	// Add many elements that all hash to the same bucket
	static constexpr Uptr numElements = 100;
	for(Uptr i = 0; i < numElements; ++i) { CHECK_TRUE(set.add(i)); }

	CHECK_EQ(set.size(), numElements);

	// Verify all elements can be found
	for(Uptr i = 0; i < numElements; ++i) { CHECK_TRUE(set.contains(i)); }

	// Remove every other element
	for(Uptr i = 0; i < numElements; i += 2) { CHECK_TRUE(set.remove(i)); }

	CHECK_EQ(set.size(), numElements / 2);

	// Verify remaining elements
	for(Uptr i = 0; i < numElements; ++i)
	{
		if(i % 2 == 0) { CHECK_FALSE(set.contains(i)); }
		else
		{
			CHECK_TRUE(set.contains(i));
		}
	}
}

static void testSetAnalyzeSpaceUsageEmpty(TEST_STATE_PARAM)
{
	// Test analyzeSpaceUsage on a freshly constructed empty set
	HashSet<Uptr> set;

	Uptr totalMemoryBytes = 0;
	Uptr maxProbeCount = 0;
	F32 occupancy = 0;
	F32 averageProbeCount = 0;

	set.analyzeSpaceUsage(totalMemoryBytes, maxProbeCount, occupancy, averageProbeCount);

	CHECK_EQ(set.size(), Uptr(0));
}

static void testSetTrackedValueDestruction(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	{
		HashSet<TrackedValue> set;
		set.add(TrackedValue(realm, 1));
		set.add(TrackedValue(realm, 2));
	}

	// After set destruction, all TrackedValues should be destroyed
	CHECK_TRUE(realm.allDestroyed());
}

static void testSetTrackedValueClear(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	HashSet<TrackedValue> set;
	set.add(TrackedValue(realm, 1));
	set.add(TrackedValue(realm, 2));

	// Capture HeapValues before clear
	std::vector<std::shared_ptr<HeapValue>> heapValues;
	for(const auto& tv : set) { heapValues.push_back(tv.heapValue); }

	set.clear();

	// All captured HeapValues should be marked as destroyed
	for(const auto& hv : heapValues) { CHECK_TRUE(hv->destroyed); }

	CHECK_EQ(set.size(), Uptr(0));
}

static void testSetTrackedValueRemove(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	HashSet<TrackedValue> set;
	set.add(TrackedValue(realm, 1));

	// Capture the HeapValue of the element in the set
	WAVM_ERROR_UNLESS(set.size() == 1);
	std::shared_ptr<HeapValue> setHeapValue = set.begin()->heapValue;
	CHECK_FALSE(setHeapValue->destroyed);

	// Create a lookup key with the same value
	TrackedValue lookupKey(realm, 1);
	CHECK_TRUE(set.remove(lookupKey));

	// The HeapValue that was in the set should be destroyed
	CHECK_TRUE(setHeapValue->destroyed);
}

static void testSetTrackedValueContainerMove(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	HashSet<TrackedValue> a;
	a.add(TrackedValue(realm, 1));
	a.add(TrackedValue(realm, 2));

	// Capture HeapValues from a
	std::set<std::shared_ptr<HeapValue>> originalHeapValues;
	for(const auto& tv : a) { originalHeapValues.insert(tv.heapValue); }

	// Move container
	HashSet<TrackedValue> b{std::move(a)};

	// Original HeapValues should NOT be destroyed (moved, not copied)
	for(const auto& hv : originalHeapValues) { CHECK_FALSE(hv->destroyed); }

	// Elements in b should have the SAME HeapValues (no copy/move of elements)
	for(const auto& tv : b) { CHECK_TRUE(originalHeapValues.count(tv.heapValue) == 1); }
}

static void testSetTrackedValueContainerCopy(TEST_STATE_PARAM)
{
	TrackedValueRealm realm;

	HashSet<TrackedValue> a;
	a.add(TrackedValue(realm, 1));
	a.add(TrackedValue(realm, 2));

	// Capture original HeapValues
	std::set<std::shared_ptr<HeapValue>> originalHeapValues;
	for(const auto& tv : a) { originalHeapValues.insert(tv.heapValue); }

	// Copy container
	HashSet<TrackedValue> b{a};

	// Original HeapValues should NOT be destroyed (a still exists)
	for(const auto& hv : originalHeapValues) { CHECK_FALSE(hv->destroyed); }

	// Elements in b should have DIFFERENT HeapValues (copies were made)
	for(const auto& tv : b)
	{
		CHECK_TRUE(originalHeapValues.count(tv.heapValue) == 0);

		// Elide any internal moves to find the underlying provenance
		auto elided = tv.heapValue->elideMoves();
		WAVM_ERROR_UNLESS(elided);

		// Should be CopyConstructed with source pointing to originals
		CHECK_EQ(elided->provenance, HeapValue::Provenance::CopyConstructed);
		CHECK_TRUE(originalHeapValues.count(elided->source) == 1);
	}
}

I32 execHashSetTest(int argc, char** argv)
{
	TEST_STATE_LOCAL;
	Timing::Timer timer;

	testStringSet(TEST_STATE_ARG);
	testU32Set(TEST_STATE_ARG);
	testSetCopy(TEST_STATE_ARG);
	testSetMove(TEST_STATE_ARG);
	testSetInitializerList(TEST_STATE_ARG);
	testSetIterator(TEST_STATE_ARG);
	testSetBracketOperator(TEST_STATE_ARG);
	testSetEmpty(TEST_STATE_ARG);
	testSetClear(TEST_STATE_ARG);
	testSetReserve(TEST_STATE_ARG);
	testSetAddOrFail(TEST_STATE_ARG);
	testSetRemoveOrFail(TEST_STATE_ARG);
	testSetAnalyzeSpaceUsage(TEST_STATE_ARG);
	testSetAnalyzeSpaceUsageAfterClear(TEST_STATE_ARG);
	testSetAnalyzeSpaceUsageEmpty(TEST_STATE_ARG);
	testSetMovedFromState(TEST_STATE_ARG);
	testSetHashCollisions(TEST_STATE_ARG);
	testSetTrackedValueDestruction(TEST_STATE_ARG);
	testSetTrackedValueClear(TEST_STATE_ARG);
	testSetTrackedValueRemove(TEST_STATE_ARG);
	testSetTrackedValueContainerMove(TEST_STATE_ARG);
	testSetTrackedValueContainerCopy(TEST_STATE_ARG);

	Timing::logTimer("HashSetTest", timer);
	return testState.exitCode();
}
