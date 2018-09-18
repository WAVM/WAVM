#include <stdlib.h>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Inline/Timing.h"

using namespace WAVM;

static std::string generateRandomString()
{
	enum
	{
		maxChars = 16
	};

	const Uptr numChars = rand() % maxChars;
	char* buffer = (char*)alloca(numChars + 1);
	for(Uptr charIndex = 0; charIndex < numChars; ++charIndex)
	{ buffer[charIndex] = 0x20 + (rand() % (0x7E - 0x20)); }
	buffer[numChars] = 0;
	return std::string(buffer);
}

static void testStringSet()
{
	enum
	{
		numStrings = 1000
	};

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
		errorUnless(set.add(strings[i]));
		errorUnless(!set.add(strings[i]));

		for(Uptr j = 0; j < strings.size(); ++j)
		{
			const bool expectedContains = j <= i;
			errorUnless(set.contains(strings[j]) == expectedContains);
		}
	}

	for(Uptr i = 0; i < strings.size(); ++i)
	{
		errorUnless(set.remove(strings[i]));
		errorUnless(!set.remove(strings[i]));

		for(Uptr j = 0; j < strings.size(); ++j)
		{
			const bool expectedContains = j > i;
			errorUnless(set.contains(strings[j]) == expectedContains);
		}
	}
}

static void testU32Set()
{
	HashSet<U32> set;

	enum
	{
		maxI = 1024 * 1024
	};

	for(Uptr i = 0; i < maxI; ++i) { errorUnless(!set.contains(U32(i))); }

	errorUnless(set.size() == 0);
	for(Uptr i = 0; i < maxI; ++i)
	{
		errorUnless(!set.contains(U32(i)));
		errorUnless(!set.get(U32(i)));
		errorUnless(set.add(U32(i)));
		errorUnless(set.contains(U32(i)));
		errorUnless(set.get(U32(i)));
		errorUnless(set.size() == i + 1);
	}

	for(Uptr i = 0; i < maxI; ++i)
	{
		errorUnless(set.contains(U32(i)));
		errorUnless(set.remove(U32(i)));
		errorUnless(!set.contains(U32(i)));
		errorUnless(set.size() == maxI - i - 1);
	}

	for(Uptr i = 0; i < maxI; ++i) { errorUnless(!set.contains(U32(i))); }
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign"
#endif
static void testSetCopy()
{
	// Add 1000..1999 to a HashSet.
	HashSet<Uptr> a;
	for(Uptr i = 0; i < 1000; ++i) { a.add(i + 1000); }

	// Copy the set to a new HashSet.
	HashSet<Uptr> b{a};

	// Test that both the new and old HashSet contain the expected numbers.
	for(Uptr i = 0; i < 1000; ++i)
	{
		errorUnless(!a.contains(i));
		errorUnless(a.contains(i + 1000));
		errorUnless(!a.contains(i + 2000));

		errorUnless(!b.contains(i));
		errorUnless(b.contains(i + 1000));
		errorUnless(!b.contains(i + 2000));
	}

	// Test copying a set from itself.
	b = b;

	// Test that the set wasn't changed by the copy-to-self.
	for(Uptr i = 0; i < 1000; ++i)
	{
		errorUnless(!b.contains(i));
		errorUnless(b.contains(i + 1000));
		errorUnless(!b.contains(i + 2000));
	}

	// Test removing an element from the set.
	b.remove(1000);
	errorUnless(a.contains(1000));
	errorUnless(!b.contains(1000));
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-move"
#endif
static void testSetMove()
{
	// Add 1000..1999 to a HashSet.
	HashSet<Uptr> a;
	for(Uptr i = 0; i < 1000; ++i) { a.add(i + 1000); }

	// Move the set to a new HashSet.
	HashSet<Uptr> b{std::move(a)};

	// Test that the new HashSet contains the expected numbers.
	for(Uptr i = 0; i < 1000; ++i)
	{
		errorUnless(!b.contains(i));
		errorUnless(b.contains(i + 1000));
		errorUnless(!b.contains(i + 2000));
	}

	// Test moving the set to itself.
	b = std::move(b);

	// Test that the set wasn't changed by the move-to-self.
	for(Uptr i = 0; i < 1000; ++i)
	{
		errorUnless(!b.contains(i));
		errorUnless(b.contains(i + 1000));
		errorUnless(!b.contains(i + 2000));
	}
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

static void testSetInitializerList()
{
	HashSet<Uptr> set{1, 3, 5, 7, 11, 13, 17};
	errorUnless(!set.contains(0));
	errorUnless(set.contains(1));
	errorUnless(!set.contains(2));
	errorUnless(set.contains(3));
	errorUnless(!set.contains(4));
	errorUnless(set.contains(5));
	errorUnless(!set.contains(6));
	errorUnless(set.contains(7));
	errorUnless(!set.contains(8));
	errorUnless(!set.contains(9));
	errorUnless(!set.contains(10));
	errorUnless(set.contains(11));
	errorUnless(!set.contains(12));
	errorUnless(set.contains(13));
	errorUnless(!set.contains(14));
	errorUnless(!set.contains(15));
	errorUnless(!set.contains(16));
	errorUnless(set.contains(17));
}

static void testSetIterator()
{
	// Add 1..9 to a HashSet.
	HashSet<Uptr> a;
	for(Uptr i = 1; i < 10; ++i) { a.add(i); }

	// 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 = 45
	{
		Uptr sum = 0;
		for(Uptr i : a) { sum += i; }
		errorUnless(sum == 45);
	}

	// Remove 5.
	a.remove(5);

	// 1 + 2 + 3 + 4 + 6 + 7 + 8 + 9 = 40
	{
		Uptr sum = 0;
		for(Uptr i : a) { sum += i; }
		errorUnless(sum == 40);
	}
}

static void testSetBracketOperator()
{
	HashSet<Uptr> set{1, 3, 5, 7, 11, 13, 17};
	errorUnless(set[1] == 1);
	errorUnless(set[3] == 3);
	errorUnless(set[5] == 5);
	errorUnless(set[7] == 7);
	errorUnless(set[11] == 11);
	errorUnless(set[13] == 13);
	errorUnless(set[17] == 17);
}

I32 main()
{
	Timing::Timer timer;
	testStringSet();
	testU32Set();
	testSetCopy();
	testSetMove();
	testSetInitializerList();
	testSetIterator();
	testSetBracketOperator();
	Timing::logTimer("HashSetTest", timer);
	return 0;
}