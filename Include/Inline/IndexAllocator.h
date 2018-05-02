#pragma once

#include "Inline/Errors.h"

template<typename Index, Index maxIndex>
struct IndexAllocator
{
	IndexAllocator()
	: minUnallocatedIndex()
	{}

	Index alloc()
	{
		if(freeIndices.size())
		{
			const Index freeIndex = freeIndices.back();
			freeIndices.pop_back();
			return freeIndex;
		}
		else
		{
			const Index newIndex = minUnallocatedIndex++;
			if(newIndex >= maxIndex)
			{
				Errors::fatal("Exhausted available indices");
			}
			return newIndex;
		}
	}

	void free(Index index)
	{
		freeIndices.push_back(index);
	}

private:
	std::vector<Index> freeIndices;
	Index minUnallocatedIndex;
};
