#include "Memory.h"
#include <memory>
#include <assert.h>

namespace Memory
{
	Arena::~Arena()
	{
		revert(nullptr,0,0);
	}

	void* Arena::allocate(size_t numBytes)
	{
		// If the current segment doesn't have enough space for this allocation, allocate a new segment.
		if(!currentSegment || currentSegmentAllocatedBytes + numBytes > currentSegment->totalBytes)
		{
			size_t segmentBytes = numBytes > defaultSegmentBytes ? numBytes : defaultSegmentBytes;
			uint8_t* newSegmentMemory = (uint8_t*)malloc(sizeof(Segment) - 1 + segmentBytes);
			Segment* newSegment = (Segment*)newSegmentMemory;
			newSegment->previousSegment = currentSegment;
			newSegment->totalBytes = segmentBytes;
			currentSegmentAllocatedBytes = 0;
			currentSegment = newSegment;
		}

		void* result = currentSegment->memory + currentSegmentAllocatedBytes;
		currentSegmentAllocatedBytes += numBytes;
		totalAllocatedBytes += numBytes;
		return result;
	}

	void* Arena::reallocateRaw(void* oldAllocation,size_t previousNumBytes,size_t newNumBytes)
	{
		auto bytePointer = (uint8_t*)oldAllocation;

		if(currentSegment
			&& bytePointer + previousNumBytes == currentSegment->memory + currentSegmentAllocatedBytes
			&& currentSegmentAllocatedBytes + newNumBytes - previousNumBytes <= currentSegment->totalBytes)
		{
			// If possible, just change the size of the old allocation.
			currentSegmentAllocatedBytes = currentSegmentAllocatedBytes - previousNumBytes + newNumBytes;
			totalAllocatedBytes = totalAllocatedBytes - previousNumBytes + newNumBytes;
			return oldAllocation;
		}
		else if(currentSegment
			&& bytePointer + previousNumBytes == currentSegment->memory + currentSegmentAllocatedBytes
			&& bytePointer == currentSegment->memory)
		{
			// If this is the only thing in the current segment, but it has outgrown the segment, just resize the segment.
			currentSegment = (Segment*)realloc(currentSegment,sizeof(Segment) - 1 + newNumBytes);
			currentSegmentAllocatedBytes = currentSegmentAllocatedBytes - previousNumBytes + newNumBytes;
			currentSegment->totalBytes = newNumBytes;
			return currentSegment->memory;
		}
		else if(newNumBytes < previousNumBytes)
		{
			// Even if we can't free some memory, never make a new allocation to shrink an old one.
			return oldAllocation;
		}
		else if(newNumBytes)
		{
			// Otherwise, make a new allocation and copy the old memory to it.
			void* newAllocation = allocate(newNumBytes);
			if(previousNumBytes)
			{
				memcpy(newAllocation,oldAllocation,previousNumBytes);
			}
			return newAllocation;
		}
		else
		{
			// If the new size is zero, return null.
			return nullptr;
		}
	}

	void Arena::revert(Segment* newSegment,size_t newSegmentAllocatedBytes,size_t newTotalAllocatedBytes)
	{
		currentSegmentAllocatedBytes = newSegmentAllocatedBytes;
		totalAllocatedBytes = newTotalAllocatedBytes;

		// Traverse the segment chain and free segments allocated since the specified segment.
		while(currentSegment != newSegment)
		{
			auto segmentToFree = currentSegment;
			currentSegment = currentSegment->previousSegment;
			free(segmentToFree);
		};
	}

	Arena::Mark::Mark(Arena& inArena)
	:	arena(&inArena)
	,	savedSegment(inArena.currentSegment)
	,	savedSegmentAllocatedBytes(inArena.currentSegmentAllocatedBytes)
	,	savedTotalAllocatedBytes(inArena.totalAllocatedBytes)
	{}

	void Arena::Mark::restore()
	{
		assert(arena);
		arena->revert(savedSegment,savedSegmentAllocatedBytes,savedTotalAllocatedBytes);
		arena = nullptr;
	}

	ScopedArena::ScopedArena() : Arena::Mark(getArena()) {}
	ScopedArena::~ScopedArena() { restore(); }
	Arena& ScopedArena::getArena() const
	{
		static __declspec(thread) Arena* scopedArena = nullptr;
		if(!scopedArena)
		{
			scopedArena = new Arena();
		}
		return *scopedArena;
	}
}