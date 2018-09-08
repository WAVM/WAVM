#include <stdint.h>
#include <string.h>
#include <vector>

#include "IR/Types.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Lock.h"
#include "LLVMJIT/LLVMJIT.h"
#include "Platform/Platform.h"
#include "Runtime/Runtime.h"
#include "Runtime/RuntimeData.h"
#include "RuntimePrivate.h"

using namespace Runtime;

// Global lists of tables; used to query whether an address is reserved by one of them.
static Platform::Mutex tablesMutex;
static std::vector<TableInstance*> tables;

enum
{
	numGuardPages = 1
};

static Uptr getNumPlatformPages(Uptr numBytes)
{
	return (numBytes + (Uptr(1) << Platform::getPageSizeLog2()) - 1) >> Platform::getPageSizeLog2();
}

static TableInstance* createTableImpl(Compartment* compartment,
									  IR::TableType type,
									  Uptr numElements)
{
	TableInstance* table = new TableInstance(compartment, type);

	// In 64-bit, allocate enough address-space to safely access 32-bit table indices without bounds
	// checking, or 16MB (4M elements) if the host is 32-bit.
	const Uptr pageBytesLog2 = Platform::getPageSizeLog2();
	const Uptr tableMaxBytes = Uptr(U64(sizeof(TableInstance::FunctionElement)) << 32);
	const Uptr tableMaxPages = tableMaxBytes >> pageBytesLog2;

	table->baseAddress = (TableInstance::FunctionElement*)Platform::allocateVirtualPages(
		tableMaxPages + numGuardPages);
	table->endOffset = tableMaxBytes;
	if(!table->baseAddress)
	{
		delete table;
		return nullptr;
	}

	// Grow the table to the type's minimum size.
	if(growTable(table, Uptr(type.size.min)) == -1)
	{
		delete table;
		return nullptr;
	}

	// Add the table to the global array.
	{
		Lock<Platform::Mutex> tablesLock(tablesMutex);
		tables.push_back(table);
	}
	return table;
}

TableInstance* Runtime::createTable(Compartment* compartment, IR::TableType type)
{
	wavmAssert(type.size.min <= UINTPTR_MAX);
	TableInstance* table = createTableImpl(compartment, type, Uptr(type.size.min));
	if(!table) { return nullptr; }

	// Add the table to the compartment's tables IndexMap.
	{
		Lock<Platform::Mutex> compartmentLock(compartment->mutex);

		table->id = compartment->tables.add(UINTPTR_MAX, table);
		if(table->id == UINTPTR_MAX)
		{
			delete table;
			return nullptr;
		}
		compartment->runtimeData->tableBases[table->id] = table->baseAddress;
	}

	return table;
}

TableInstance* Runtime::cloneTable(TableInstance* table, Compartment* newCompartment)
{
	Lock<Platform::Mutex> elementsLock(table->elementsMutex);

	TableInstance* newTable = createTableImpl(newCompartment, table->type, table->elements.size());
	if(!newTable) { return nullptr; }

	newTable->id = table->id;
	newTable->elements = table->elements;
	memcpy(newTable->baseAddress,
		   table->baseAddress,
		   table->elements.size() * sizeof(TableInstance::FunctionElement));

	// Insert the table in the new compartment's tables array with the same index as it had in the
	// original compartment's tables IndexMap.
	{
		Lock<Platform::Mutex> compartmentLock(newCompartment->mutex);

		newTable->id = table->id;
		newCompartment->tables.insertOrFail(newTable->id, newTable);
		newCompartment->runtimeData->tableBases[newTable->id] = newTable->baseAddress;
	}

	return newTable;
}

void TableInstance::finalize()
{
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);

	wavmAssert(compartment->tables[id] == this);
	compartment->tables.removeOrFail(id);

	wavmAssert(compartment->runtimeData->tableBases[id] == baseAddress);
	compartment->runtimeData->tableBases[id] = nullptr;
}

TableInstance::~TableInstance()
{
	// Decommit all pages.
	if(elements.size() > 0)
	{
		Platform::decommitVirtualPages(
			(U8*)baseAddress,
			getNumPlatformPages(elements.size() * sizeof(TableInstance::FunctionElement)));
	}

	// Free the virtual address space.
	const Uptr pageBytesLog2 = Platform::getPageSizeLog2();
	if(endOffset > 0)
	{
		Platform::freeVirtualPages((U8*)baseAddress, (endOffset >> pageBytesLog2) + numGuardPages);
	}
	baseAddress = nullptr;

	// Remove the table from the global array.
	{
		Lock<Platform::Mutex> tablesLock(tablesMutex);
		for(Uptr tableIndex = 0; tableIndex < tables.size(); ++tableIndex)
		{
			if(tables[tableIndex] == this)
			{
				tables.erase(tables.begin() + tableIndex);
				break;
			}
		}
	}
}

bool Runtime::isAddressOwnedByTable(U8* address)
{
	// Iterate over all tables and check if the address is within the reserved address space for
	// each.
	Lock<Platform::Mutex> tablesLock(tablesMutex);
	for(auto table : tables)
	{
		U8* startAddress = (U8*)table->baseAddress;
		U8* endAddress = ((U8*)table->baseAddress) + table->endOffset;
		if(address >= startAddress && address < endAddress) { return true; }
	}
	return false;
}

Object* Runtime::setTableElement(TableInstance* table,
								 Uptr index,
								 Object* newValue,
								 MemoryInstance* intrinsicDefaultMemory,
								 TableInstance* intrinsicDefaultTable)
{
	Compartment* compartment = table->compartment;

	// If a default memory or table are provided to bind to intrinsic functions, make sure they are
	// in the same compartment as the table being modified.
	wavmAssert(!intrinsicDefaultMemory || intrinsicDefaultMemory->compartment == compartment);
	wavmAssert(!intrinsicDefaultTable || intrinsicDefaultTable->compartment == compartment);

	// Look up the new function's code pointer.
	FunctionInstance* functionInstance = asFunction(newValue);
	void* nativeFunction = functionInstance->nativeFunction;
	wavmAssert(nativeFunction);

	// If the function isn't a WASM function, generate a thunk for it.
	if(functionInstance->callingConvention != IR::CallingConvention::wasm)
	{
		nativeFunction = LLVMJIT::getIntrinsicThunk(
			nativeFunction,
			functionInstance->type,
			functionInstance->callingConvention,
			LLVMJIT::MemoryBinding{intrinsicDefaultMemory ? intrinsicDefaultMemory->id
														  : UINTPTR_MAX},
			LLVMJIT::TableBinding{intrinsicDefaultTable ? intrinsicDefaultTable->id : UINTPTR_MAX});
	}

	// Lock the table's elements array.
	Lock<Platform::Mutex> elementsLock(table->elementsMutex);

	// Verify the index is within the table's bounds.
	if(index >= table->elements.size()) { throwException(Exception::accessViolationType); }

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the above bounds check.
	const Uptr saturatedIndex = Platform::saturateToBounds(index, table->elements.size());

	// Write the new table element to both the table's elements array and its indirect function call
	// data.
	table->baseAddress[saturatedIndex].typeEncoding = functionInstance->type.getEncoding();
	table->baseAddress[saturatedIndex].value = nativeFunction;

	auto oldValue = table->elements[saturatedIndex];
	table->elements[saturatedIndex] = newValue;
	return oldValue;
}

Object* Runtime::getTableElement(TableInstance* table, Uptr index)
{
	// Verify the index is within the table's bounds.
	if(index >= table->elements.size()) { throwException(Exception::accessViolationType); }

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the above bounds check.
	const Uptr saturatedIndex = Platform::saturateToBounds(index, table->elements.size());

	// Read from the table's elements array.
	{
		Lock<Platform::Mutex> elementsLock(table->elementsMutex);
		return table->elements[saturatedIndex];
	}
}

Uptr Runtime::getTableNumElements(TableInstance* table) { return table->elements.size(); }

Iptr Runtime::growTable(TableInstance* table, Uptr numNewElements)
{
	const Uptr previousNumElements = table->elements.size();
	if(numNewElements > 0)
	{
		// If the number of elements to grow would cause the table's size to exceed its maximum,
		// return -1.
		if(numNewElements > table->type.size.max
		   || table->elements.size() > table->type.size.max - numNewElements)
		{ return -1; }

		// Try to commit pages for the new elements, and return -1 if the commit fails.
		const Uptr previousNumPlatformPages
			= getNumPlatformPages(table->elements.size() * sizeof(TableInstance::FunctionElement));
		const Uptr newNumPlatformPages = getNumPlatformPages(
			(table->elements.size() + numNewElements) * sizeof(TableInstance::FunctionElement));
		if(newNumPlatformPages != previousNumPlatformPages
		   && !Platform::commitVirtualPages(
				  (U8*)table->baseAddress
					  + (previousNumPlatformPages << Platform::getPageSizeLog2()),
				  newNumPlatformPages - previousNumPlatformPages))
		{ return -1; }

		// Also grow the table's elements array.
		{
			Lock<Platform::Mutex> elementsLock(table->elementsMutex);
			table->elements.insert(table->elements.end(), numNewElements, nullptr);
		}
	}
	return previousNumElements;
}

Iptr Runtime::shrinkTable(TableInstance* table, Uptr numElementsToShrink)
{
	const Uptr previousNumElements = table->elements.size();
	if(numElementsToShrink > 0)
	{
		// If the number of elements to shrink would cause the table's size to drop below its
		// minimum, return -1.
		if(numElementsToShrink > table->elements.size()
		   || table->elements.size() - numElementsToShrink < table->type.size.min)
		{ return -1; }

		// Shrink the table's elements array.
		{
			Lock<Platform::Mutex> elementsLock(table->elementsMutex);
			table->elements.resize(table->elements.size() - numElementsToShrink);
		}

		// Decommit the pages that were shrunk off the end of the table's indirect function call
		// data.
		const Uptr previousNumPlatformPages
			= getNumPlatformPages(previousNumElements * sizeof(TableInstance::FunctionElement));
		const Uptr newNumPlatformPages
			= getNumPlatformPages(table->elements.size() * sizeof(TableInstance::FunctionElement));
		if(newNumPlatformPages != previousNumPlatformPages)
		{
			Platform::decommitVirtualPages(
				(U8*)table->baseAddress + (newNumPlatformPages << Platform::getPageSizeLog2()),
				(previousNumPlatformPages - newNumPlatformPages) << Platform::getPageSizeLog2());
		}
	}
	return previousNumElements;
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.init",
						  void,
						  table_init,
						  U32 destOffset,
						  U32 sourceOffset,
						  U32 numElements,
						  Uptr moduleInstanceBits,
						  Uptr tableId,
						  Uptr elemSegmentIndex)
{
	ModuleInstance* moduleInstance = reinterpret_cast<ModuleInstance*>(moduleInstanceBits);
	Lock<Platform::Mutex> passiveTableSegmentsLock(moduleInstance->passiveTableSegmentsMutex);

	if(!moduleInstance->passiveTableSegments.contains(elemSegmentIndex))
	{ throwException(Exception::invalidArgumentType); }
	else
	{
		TableInstance* table = getTableFromRuntimeData(contextRuntimeData, tableId);

		const std::vector<Uptr>& passiveTableSegmentIndices
			= moduleInstance->passiveTableSegments[elemSegmentIndex];

		for(Uptr index = 0; index < numElements; ++index)
		{
			const U64 sourceIndex = U64(sourceOffset) + index;
			const U64 destIndex = U64(destOffset) + index;
			if(sourceIndex >= passiveTableSegmentIndices.size()
			   || destIndex >= table->elements.size())
			{ throwException(Exception::accessViolationType); }

			const Uptr functionIndex = passiveTableSegmentIndices[sourceIndex];
			setTableElement(table,
							Uptr(destIndex),
							moduleInstance->functions[functionIndex],
							moduleInstance->defaultMemory,
							moduleInstance->defaultTable);
		}
	}
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.drop",
						  void,
						  table_drop,
						  Uptr moduleInstanceBits,
						  Uptr elemSegmentIndex)
{
	ModuleInstance* moduleInstance = reinterpret_cast<ModuleInstance*>(moduleInstanceBits);
	Lock<Platform::Mutex> passiveTableSegmentsLock(moduleInstance->passiveTableSegmentsMutex);

	if(!moduleInstance->passiveTableSegments.contains(elemSegmentIndex))
	{ throwException(Exception::invalidArgumentType); }
	else
	{
		moduleInstance->passiveTableSegments.removeOrFail(elemSegmentIndex);
	}
}

static void copyTableElement(TableInstance* table, U64 destIndex, U64 sourceIndex)
{
	if(destIndex >= table->elements.size() || sourceIndex >= table->elements.size())
	{ throwException(Exception::accessViolationType); }

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the outside bounds check.
	sourceIndex = Platform::saturateToBounds(sourceIndex, U64(table->elements.size()));
	destIndex = Platform::saturateToBounds(destIndex, U64(table->elements.size()));

	table->baseAddress[destIndex].typeEncoding = table->baseAddress[sourceIndex].typeEncoding;
	table->baseAddress[destIndex].value = table->baseAddress[sourceIndex].value;
	table->elements[destIndex] = table->elements[sourceIndex];
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.copy",
						  void,
						  table_copy,
						  U32 destOffset,
						  U32 sourceOffset,
						  U32 numElements,
						  Uptr tableId)
{
	TableInstance* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	Lock<Platform::Mutex> elementsLock(table->elementsMutex);

	if(sourceOffset != destOffset)
	{
		const Uptr numNonOverlappingElements
			= sourceOffset < destOffset && U64(sourceOffset) + U64(numElements) > destOffset
				  ? destOffset - sourceOffset
				  : numElements;

		// If the end of the source overlaps the beginning of the destination, copy those elements
		// before they are overwritten by the second part of the copy below.
		for(Uptr index = numNonOverlappingElements; index < numElements; ++index)
		{ copyTableElement(table, U64(destOffset) + U64(index), U64(sourceOffset) + U64(index)); }

		for(Uptr index = 0; index < numNonOverlappingElements; ++index)
		{ copyTableElement(table, U64(destOffset) + U64(index), U64(sourceOffset) + U64(index)); }
	}
}
