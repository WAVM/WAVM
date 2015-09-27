#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"

#include <cstdint>
#include <memory>
#include <assert.h>

namespace Memory
{
	class Arena
	{
		struct Segment;
	public:

		struct Mark
		{
			Mark(Arena& inArena);
		
			void restore();

		private:
			Arena* arena;
			Segment* savedSegment;
			size_t savedSegmentAllocatedBytes;
			size_t savedTotalAllocatedBytes;
			size_t savedTotalWastedBytes;
		};

		Arena(size_t inDefaultSegmentBytes = 8192)
		: defaultSegmentBytes(inDefaultSegmentBytes), currentSegment(nullptr), currentSegmentAllocatedBytes(0), totalAllocatedBytes(0), totalWastedBytes(0) {}
		Arena(const Arena&) = delete;
		CORE_API ~Arena();
	
		CORE_API void* allocate(size_t numBytes);
		CORE_API void* reallocateRaw(void* oldAllocation,size_t previousNumBytes,size_t newNumBytes);

		template<typename T> T* allocate(size_t numT) { return (T*)allocate(sizeof(T) * numT); }
		template<typename T> T* reallocate(T* oldAllocation,size_t oldNumT,size_t newNumT)
		{ return (T*)reallocateRaw(oldAllocation,sizeof(T) * oldNumT,sizeof(T) * newNumT); }

		template<typename T> T* copyToArena(const T* source,size_t count) { auto dest = allocate<T>(count); std::copy(source,source+count,dest); return dest; }

		size_t getTotalAllocatedBytes() const { return totalAllocatedBytes; }
		size_t getTotalWastedBytes() const { return totalWastedBytes; }

	private:

		struct Segment
		{
			Segment* previousSegment;

			size_t totalBytes;

			uint8 memory[1];
		};

		size_t defaultSegmentBytes;
		Segment* currentSegment;
		size_t currentSegmentAllocatedBytes;
		size_t totalAllocatedBytes;
		size_t totalWastedBytes;

		CORE_API void revert(Segment* newSegment,size_t newSegmentAllocatedBytes,size_t newTotalAllocatedBytes,size_t newTotalWastedBytes);
	};

	// Encapsulates an arena used to allocate memory that is freed when the ScopedArena goes out of scope.
	// Implemented using a thread-local Arena.
	struct ScopedArena : private Arena::Mark
	{
		CORE_API ScopedArena();
		CORE_API ~ScopedArena();

		CORE_API Arena& getArena() const;
		operator Arena&() const { return getArena(); }
	};
	
	// Encapsulates an array allocated from a memory arena. Doesn't call constructors/destructors.
	template<typename Element>
	struct ArenaArray
	{
		ArenaArray(): elements(nullptr), numElements(0), numReservedElements(0) {}
		ArenaArray(const ArenaArray&) = delete;
		ArenaArray(ArenaArray&& inMove)
		: elements(inMove.elements), numElements(inMove.numElements), numReservedElements(inMove.numReservedElements)
		{ inMove.elements = nullptr; inMove.numElements = 0; inMove.numReservedElements = 0; }
		
		void reset(Arena& arena) { elements = arena.reallocate(elements,numReservedElements,0); numElements = 0; numReservedElements = 0; }
		void resize(Arena& arena,size_t newNumElements)
		{
			numElements = newNumElements;
			if(numElements > numReservedElements)
			{
				auto newNumReservedElements = numElements * 2;
				elements = arena.reallocate(elements,numReservedElements,newNumReservedElements);
				numReservedElements = newNumReservedElements;
			}
		}
		void shrink(Arena& arena)
		{
			if(numReservedElements != numElements)
			{
				elements = arena.reallocate(elements,numReservedElements,numElements);
				numReservedElements = numElements;
			}
		}

		friend bool operator==(const ArenaArray<Element>& left,const ArenaArray<Element>& right)
		{
			if(left.size() != right.size()) { return false; }
			for(uintptr elementIndex = 0;elementIndex < left.size();++elementIndex)
			{ if(left[elementIndex] != right[elementIndex]) { return false; } }
			return true;
		}

		const Element* data() const { return elements; }
		Element* data() { return elements; }
		const Element& operator[](uintptr index) const { assert(index < numElements); return elements[index]; }
		Element& operator[](uintptr index) { assert(index < numElements); return elements[index]; }
		size_t size() const { return numElements; }
	private:
		Element* elements;
		size_t numElements;
		size_t numReservedElements;
	};

	// Encapsulates a string allocated from a memory arena.
	struct ArenaString
	{
		void reset(Arena& arena) { characters.reset(arena); }
		void append(Arena& arena,const char c)
		{
			size_t newLength = length() + 1;
			characters.resize(arena,newLength + 1);
			characters[characters.size() - 2] = c;
			characters[characters.size() - 1] = 0;
		}
		void append(Arena& arena,const char* string,size_t numChars)
		{
			auto numCharsWithNull = numChars + 1;
			auto originalLength = length();
			auto newLength = length() + numChars;
			characters.resize(arena,newLength + 1);
			for(uintptr i = 0;i < numCharsWithNull;++i) { characters[originalLength + i] = string[i]; }
		}
		template<size_t numCharsWithNull>
		void append(Arena& arena,const char (&string)[numCharsWithNull])
		{
			auto numChars = numCharsWithNull - 1;
			auto originalLength = length();
			auto newLength = length() + numChars;
			characters.resize(arena,newLength + 1);
			for(uintptr i = 0;i < numCharsWithNull;++i) { characters[originalLength + i] = string[i]; }
		}
		void shrink(Arena& arena) { characters.shrink(arena); }

		const char* c_str() const { if(characters.size()) { return characters.data(); } else { return ""; } }
		const char operator[](uintptr index) const { assert(index < length()); return characters[index]; }
		char& operator[](uintptr index) { assert(index < length()); return characters[index]; }
		size_t length() const { return characters.size() ? characters.size() - 1 : 0; }
	private:
		ArenaArray<char> characters;
	};
}

inline void* operator new(size_t numBytes,Memory::Arena& arena)
{
	return arena.allocate(numBytes);
}

inline void operator delete(void*,Memory::Arena&) {}

inline void* operator new[](size_t numBytes,Memory::Arena& arena)
{
	return arena.allocate(numBytes);
}

inline void operator delete[](void*,Memory::Arena&) {}
