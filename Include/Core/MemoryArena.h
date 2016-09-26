#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"

#include <cstdint>
#include <memory>
#include <assert.h>

namespace MemoryArena
{
	// A memory allocator that can satisfy many allocation requests very efficiently, but
	// cannot free individual allocations. Instead, a Arena::Mark may be created to save
	// the state of the arena, and Arena::Mark::restore may then be called to free all
	// allocations in the arena since the mark was created.
	class Arena
	{
		struct Segment;
	public:

		// Captures the state of an arena, and in the future reset the arena back to that state.
		struct Mark
		{
			Mark(Arena& inArena);
			~Mark();
		
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
		CORE_API ~Arena();

		// Don't allow copying arenas, but allow them to be moved.
		Arena(const Arena&) = delete;
		void operator=(const Arena&) = delete;

		Arena(Arena&& inMove) : defaultSegmentBytes(0), currentSegment(nullptr), currentSegmentAllocatedBytes(0), totalAllocatedBytes(0), totalWastedBytes(0)
		{ *this = std::move(inMove); }
		void operator=(Arena&& inMove)
		{
			if(this != &inMove)
			{
				// Free this arena's memory.
				revert(nullptr,0,0,0);

				// Move the other Arena's state into this Arena, clearing the other Arena.
				defaultSegmentBytes = inMove.defaultSegmentBytes;
				currentSegment = inMove.currentSegment;
				currentSegmentAllocatedBytes = inMove.currentSegmentAllocatedBytes;
				totalAllocatedBytes = inMove.totalAllocatedBytes;
				totalWastedBytes = inMove.totalWastedBytes;
				inMove.currentSegment = nullptr;
				inMove.currentSegmentAllocatedBytes = 0;
				inMove.totalAllocatedBytes = 0;
				inMove.totalWastedBytes = 0;
			}
		}

		// Allocates numBytes from the arena.
		CORE_API void* allocate(size_t numBytes);
		template<typename T> T* allocate(size_t numT) { return (T*)allocate(sizeof(T) * numT); }

		// Changes the size of an allocation that was made from this arena.
		// Tries to extend it in place, but that is only possible if there is space in the same arena segment,
		// and there have been no allocations after the original to follow it in the segment.
		CORE_API void* reallocateRaw(void* oldAllocation,size_t previousNumBytes,size_t newNumBytes);
		template<typename T> T* reallocate(T* oldAllocation,size_t oldNumT,size_t newNumT)
		{ return (T*)reallocateRaw(oldAllocation,sizeof(T) * oldNumT,sizeof(T) * newNumT); }

		// Copies an array into the arena.
		template<typename T> T* copyToArena(const T* source,size_t count) { auto dest = allocate<T>(count); std::copy(source,source+count,dest); return dest; }

		// Statistics on the memory used by the arena.
		size_t getTotalAllocatedBytes() const { return totalAllocatedBytes; }
		size_t getTotalWastedBytes() const { return totalWastedBytes; }

	private:

		// The arena maintains a linked list of segments for its memory:
		// It starts with no segments, and on the first allocation creates a defaultSegmentBytes-sized segment to hold the allocation.
		// Each allocation follows the last in the most recently created segment, but when the segment is full a new one will be created.
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

		// Reverts the arena's state to the given values, freeing memory segments allocated since the state originally occured.
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
	// As you grow the array, it will try to call Arena::reallocate to grow the array without copying
	// it in the Arena, but if other allocations are made after the Array was allocated, this will
	// result in a second, larger chunk of memory being allocated for the Array, and the original
	// "leaking". For this reason, Array is best used for convenience when you don't know the size
	// of an array you would like to allocate in the Arena up front, but would like to start initializing
	// it in place, and will know the size of the Array before making other allocations from the arena.
	template<typename Element>
	struct Array
	{
		Array(): elements(nullptr), numElements(0), numReservedElements(0) {}
		Array(const Array&) = delete;
		Array(Array&& inMove)
		: elements(inMove.elements), numElements(inMove.numElements), numReservedElements(inMove.numReservedElements)
		{ inMove.elements = nullptr; inMove.numElements = 0; inMove.numReservedElements = 0; }

		// Functions to access and change the array's size.
		size_t size() const { return numElements; }
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

		// Functions to access the array's elements.
		const Element* data() const { return elements; }
		Element* data() { return elements; }
		const Element& operator[](uintp index) const { assert(index < numElements); return elements[index]; }
		Element& operator[](uintp index) { assert(index < numElements); return elements[index]; }
	private:
		Element* elements;
		size_t numElements;
		size_t numReservedElements;
	};

	// Encapsulates a string allocated from a memory arena.
	// See the above comments on Array for caveats.
	struct String
	{
		// Functions to access and change the string's length.
		size_t length() const { return characters.size() ? characters.size() - 1 : 0; }
		void reset(Arena& arena) { characters.reset(arena); }
		void shrink(Arena& arena) { characters.shrink(arena); }

		// Appends characters to the string.
		void append(Arena& arena,const char c)
		{
			size_t newLength = length() + 1;
			characters.resize(arena,newLength + 1);
			characters[characters.size() - 2] = c;
			characters[characters.size() - 1] = 0;
		}
		void append(Arena& arena,const char* string,size_t numChars)
		{
			auto originalLength = length();
			auto newLength = length() + numChars;
			characters.resize(arena,newLength + 1);
			for(uintp i = 0;i < numChars;++i) { characters[originalLength + i] = string[i]; }
			characters[originalLength + numChars] = 0;
		}
		template<size_t numCharsWithNull>
		void append(Arena& arena,const char (&string)[numCharsWithNull])
		{
			auto numChars = numCharsWithNull - 1;
			auto originalLength = length();
			auto newLength = length() + numChars;
			characters.resize(arena,newLength + 1);
			for(uintp i = 0;i < numCharsWithNull;++i) { characters[originalLength + i] = string[i]; }
		}

		// Accesses the string's characters.
		const char* c_str() const { if(characters.size()) { return characters.data(); } else { return ""; } }
		const char operator[](uintp index) const { assert(index < length()); return characters[index]; }
		char& operator[](uintp index) { assert(index < length()); return characters[index]; }
	private:
		Array<char> characters;
	};
}

// New/delete operators that allocate memory from an Arena.
inline void* operator new(size_t numBytes,MemoryArena::Arena& arena)
{
	return arena.allocate(numBytes);
}
inline void operator delete(void*,MemoryArena::Arena&) {}

inline void* operator new[](size_t numBytes,MemoryArena::Arena& arena)
{
	return arena.allocate(numBytes);
}
inline void operator delete[](void*,MemoryArena::Arena&) {}
