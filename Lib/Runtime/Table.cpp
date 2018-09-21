#include <stdint.h>
#include <string.h>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Intrinsic.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::Runtime;

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

static const AnyFunc* makeDummyAnyFunc()
{
	AnyFunc* anyFunc = new AnyFunc;
	anyFunc->anyRef.object = nullptr;
	anyFunc->functionTypeEncoding = IR::FunctionType::Encoding{0};
	anyFunc->code[0] = 0xcc; // int3
	return anyFunc;
}

const AnyFunc* Runtime::getOutOfBoundsAnyFunc()
{
	static const AnyFunc* anyFunc = makeDummyAnyFunc();
	return anyFunc;
}

const AnyFunc* Runtime::getUninitializedAnyFunc()
{
	static const AnyFunc* anyFunc = makeDummyAnyFunc();
	return anyFunc;
}

static Uptr anyRefToBiasedTableElementValue(const AnyReferee* anyRef)
{
	return reinterpret_cast<Uptr>(anyRef) - reinterpret_cast<Uptr>(getOutOfBoundsAnyFunc());
}

static const AnyReferee* biasedTableElementValueToAnyRef(Uptr biasedValue)
{
	return reinterpret_cast<const AnyReferee*>(biasedValue
											   + reinterpret_cast<Uptr>(getOutOfBoundsAnyFunc()));
}

static TableInstance* createTableImpl(Compartment* compartment,
									  IR::TableType type,
									  std::string&& debugName)
{
	TableInstance* table = new TableInstance(compartment, type, std::move(debugName));

	// In 64-bit, allocate enough address-space to safely access 32-bit table indices without bounds
	// checking, or 16MB (4M elements) if the host is 32-bit.
	const Uptr pageBytesLog2 = Platform::getPageSizeLog2();
	const U64 tableMaxElements = Uptr(1) << 32;
	const U64 tableMaxBytes = sizeof(TableInstance::Element) * tableMaxElements;
	const U64 tableMaxPages = tableMaxBytes >> pageBytesLog2;

	table->elements
		= (TableInstance::Element*)Platform::allocateVirtualPages(tableMaxPages + numGuardPages);
	table->numReservedBytes = tableMaxBytes;
	table->numReservedElements = tableMaxElements;
	if(!table->elements)
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

static Iptr growTableImpl(TableInstance* table, Uptr numElementsToGrow, bool initializeNewElements)
{
	if(!numElementsToGrow) { return table->numElements.load(std::memory_order_acquire); }

	Lock<Platform::Mutex> resizingLock(table->resizingMutex);

	const Uptr previousNumElements = table->numElements.load(std::memory_order_acquire);

	// If the growth would cause the table's size to exceed its maximum, return -1.
	if(numElementsToGrow > table->type.size.max
	   || previousNumElements > table->type.size.max - numElementsToGrow
	   || numElementsToGrow > IR::maxTableElems
	   || previousNumElements > IR::maxTableElems - numElementsToGrow)
	{ return -1; }

	// Try to commit pages for the new elements, and return -1 if the commit fails.
	const Uptr newNumElements = previousNumElements + numElementsToGrow;
	const Uptr previousNumPlatformPages
		= getNumPlatformPages(previousNumElements * sizeof(TableInstance::Element));
	const Uptr newNumPlatformPages
		= getNumPlatformPages(newNumElements * sizeof(TableInstance::Element));
	if(newNumPlatformPages != previousNumPlatformPages
	   && !Platform::commitVirtualPages(
			  (U8*)table->elements + (previousNumPlatformPages << Platform::getPageSizeLog2()),
			  newNumPlatformPages - previousNumPlatformPages))
	{ return -1; }

	if(initializeNewElements)
	{
		// Write the uninitialized sentinel value to the new elements.
		for(Uptr elementIndex = previousNumElements; elementIndex < newNumElements; ++elementIndex)
		{
			table->elements[elementIndex].biasedValue.store(
				anyRefToBiasedTableElementValue(&getUninitializedAnyFunc()->anyRef),
				std::memory_order_release);
		}
	}

	table->numElements.store(newNumElements, std::memory_order_release);
	return previousNumElements;
}

TableInstance* Runtime::createTable(Compartment* compartment, IR::TableType type, std::string&& debugName)
{
	wavmAssert(type.size.min <= UINTPTR_MAX);
	TableInstance* table = createTableImpl(compartment, type, std::move(debugName));
	if(!table) { return nullptr; }

	// Grow the table to the type's minimum size.
	if(growTableImpl(table, Uptr(type.size.min), true) == -1)
	{
		delete table;
		return nullptr;
	}

	// Add the table to the compartment's tables IndexMap.
	{
		Lock<Platform::Mutex> compartmentLock(compartment->mutex);

		table->id = compartment->tables.add(UINTPTR_MAX, table);
		if(table->id == UINTPTR_MAX)
		{
			delete table;
			return nullptr;
		}
		compartment->runtimeData->tableBases[table->id] = table->elements;
	}

	return table;
}

TableInstance* Runtime::cloneTable(TableInstance* table, Compartment* newCompartment)
{
	Lock<Platform::Mutex> resizingLock(table->resizingMutex);

	// Create the new table.
	const Uptr numElements = table->numElements.load(std::memory_order_acquire);
	std::string debugName = table->debugName;
	TableInstance* newTable = createTableImpl(newCompartment, table->type, std::move(debugName));
	if(!newTable) { return nullptr; }

	// Grow the table to the same size as the original, without initializing the new elements since
	// they will be written immediately following this.
	if(growTableImpl(newTable, numElements, false) == -1)
	{
		delete newTable;
		return nullptr;
	}

	// Copy the original table's elements to the new table.
	for(Uptr elementIndex = 0; elementIndex < numElements; ++elementIndex)
	{
		newTable->elements[elementIndex].biasedValue.store(
			table->elements[elementIndex].biasedValue.load(std::memory_order_acquire),
			std::memory_order_release);
	}

	resizingLock.unlock();

	// Insert the table in the new compartment's tables array with the same index as it had in the
	// original compartment's tables IndexMap.
	{
		Lock<Platform::Mutex> compartmentLock(newCompartment->mutex);

		newTable->id = table->id;
		newCompartment->tables.insertOrFail(newTable->id, newTable);
		newCompartment->runtimeData->tableBases[newTable->id] = newTable->elements;
	}

	return newTable;
}

void TableInstance::finalize()
{
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);

	wavmAssert(compartment->tables[id] == this);
	compartment->tables.removeOrFail(id);

	wavmAssert(compartment->runtimeData->tableBases[id] == elements);
	compartment->runtimeData->tableBases[id] = nullptr;
}

TableInstance::~TableInstance()
{
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

	// Decommit all pages.
	if(numElements > 0)
	{
		Platform::decommitVirtualPages(
			(U8*)elements, getNumPlatformPages(numElements * sizeof(TableInstance::Element)));
	}

	// Free the virtual address space.
	const Uptr pageBytesLog2 = Platform::getPageSizeLog2();
	if(numReservedBytes > 0)
	{
		Platform::freeVirtualPages((U8*)elements,
								   (numReservedBytes >> pageBytesLog2) + numGuardPages);
	}
	elements = nullptr;
	numElements = numReservedBytes = numReservedElements = 0;
}

bool Runtime::isAddressOwnedByTable(U8* address, TableInstance*& outTable, Uptr& outTableIndex)
{
	// Iterate over all tables and check if the address is within the reserved address space for
	// each.
	Lock<Platform::Mutex> tablesLock(tablesMutex);
	for(auto table : tables)
	{
		U8* startAddress = (U8*)table->elements;
		U8* endAddress = ((U8*)table->elements) + table->numReservedBytes;
		if(address >= startAddress && address < endAddress)
		{
			outTable = table;
			outTableIndex = (address - startAddress) / sizeof(TableInstance::Element);
			return true;
		}
	}
	return false;
}

static const AnyReferee* setTableElementAnyRef(TableInstance* table,
											   Uptr index,
											   const AnyReferee* anyRef)
{
	// Verify the index is within the table's bounds.
	if(index >= table->numReservedElements)
	{ throwException(Exception::outOfBoundsTableAccessType, {asAnyRef(table), U64(index)}); }

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the above bounds check.
	const Uptr saturatedIndex
		= Platform::saturateToBounds(index, U64(table->numReservedElements) - 1);

	// Compute the biased value to store in the table.
	const Uptr biasedValue = anyRefToBiasedTableElementValue(anyRef);

	// Atomically replace the table element, throwing an out-of-bounds exception before the write if
	// the element being replaced is an out-of-bounds sentinel value.
	Uptr oldBiasedValue = table->elements[saturatedIndex].biasedValue;
	while(true)
	{
		if(biasedTableElementValueToAnyRef(oldBiasedValue) == &getOutOfBoundsAnyFunc()->anyRef)
		{ throwException(Exception::outOfBoundsTableAccessType, {asAnyRef(table), U64(index)}); }
		if(table->elements[saturatedIndex].biasedValue.compare_exchange_weak(
			   oldBiasedValue, biasedValue, std::memory_order_acq_rel))
		{ break; }
	};

	return biasedTableElementValueToAnyRef(oldBiasedValue);
}

static const AnyReferee* getTableElementAnyRef(TableInstance* table, Uptr index)
{
	// Verify the index is within the table's bounds.
	if(index >= table->numReservedElements)
	{ throwException(Exception::outOfBoundsTableAccessType, {asAnyRef(table), U64(index)}); }

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the above bounds check.
	const Uptr saturatedIndex
		= Platform::saturateToBounds(index, U64(table->numReservedElements) - 1);

	// Read the table element.
	const Uptr biasedValue
		= table->elements[saturatedIndex].biasedValue.load(std::memory_order_acquire);
	const AnyReferee* anyRef = biasedTableElementValueToAnyRef(biasedValue);

	// If the element was an out-of-bounds sentinel value, throw an out-of-bounds exception.
	if(anyRef == &getOutOfBoundsAnyFunc()->anyRef)
	{ throwException(Exception::outOfBoundsTableAccessType, {asAnyRef(table), U64(index)}); }

	return anyRef;
}

const AnyReferee* Runtime::setTableElement(TableInstance* table,
										   Uptr index,
										   const AnyReferee* newValue)
{
	// If the new value is null, write the uninitialized sentinel value instead.
	if(!newValue) { newValue = &getUninitializedAnyFunc()->anyRef; }

	// Write the table element.
	const AnyReferee* oldAnyRef = setTableElementAnyRef(table, index, newValue);

	// If the old table element was the uninitialized sentinel value, return null.
	return oldAnyRef == &getUninitializedAnyFunc()->anyRef ? nullptr : oldAnyRef;
}

const AnyReferee* Runtime::getTableElement(TableInstance* table, Uptr index)
{
	const AnyReferee* anyRef = getTableElementAnyRef(table, index);

	// If the old table element was the uninitialized sentinel value, return null.
	return anyRef == &getUninitializedAnyFunc()->anyRef ? nullptr : anyRef;
}

Uptr Runtime::getTableNumElements(TableInstance* table)
{
	return table->numElements.load(std::memory_order_acquire);
}

Iptr Runtime::growTable(TableInstance* table, Uptr numNewElements)
{
	return growTableImpl(table, numNewElements, true);
}

Iptr Runtime::shrinkTable(TableInstance* table, Uptr numElementsToShrink)
{
	if(!numElementsToShrink) { return table->numElements.load(std::memory_order_acquire); }

	Lock<Platform::Mutex> resizingLock(table->resizingMutex);

	const Uptr previousNumElements = table->numElements.load(std::memory_order_acquire);

	// If the number of elements to shrink would cause the table's size to drop below its minimum,
	// return -1.
	if(numElementsToShrink > previousNumElements
	   || previousNumElements - numElementsToShrink < table->type.size.min)
	{ return -1; }

	// Decommit the pages that were shrunk off the end of the table's indirect function call data.
	const Uptr newNumElements = previousNumElements - numElementsToShrink;
	const Uptr previousNumPlatformPages
		= getNumPlatformPages(previousNumElements * sizeof(TableInstance::Element));
	const Uptr newNumPlatformPages
		= getNumPlatformPages(newNumElements * sizeof(TableInstance::Element));
	if(newNumPlatformPages != previousNumPlatformPages)
	{
		Platform::decommitVirtualPages(
			(U8*)table->elements + (newNumPlatformPages << Platform::getPageSizeLog2()),
			(previousNumPlatformPages - newNumPlatformPages) << Platform::getPageSizeLog2());
	}

	// Write the out-of-bounds sentinel value to any removed elements that are between the new end
	// of the table and the first decommitted page.
	const Uptr numCommittedIndices
		= (newNumPlatformPages << Platform::getPageSizeLog2()) / sizeof(TableInstance::Element);
	if(numCommittedIndices > newNumElements)
	{
		for(Uptr elementIndex = numCommittedIndices - 1; elementIndex > newNumElements;
			--elementIndex)
		{
			table->elements[elementIndex].biasedValue.store(
				anyRefToBiasedTableElementValue(&getOutOfBoundsAnyFunc()->anyRef),
				std::memory_order_release);
		}
	}

	table->numElements.store(newNumElements, std::memory_order_release);
	return previousNumElements;
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.get",
						  const AnyReferee*,
						  table_get,
						  U32 index,
						  Uptr tableId)
{
	TableInstance* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	return getTableElement(table, index);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.set",
						  void,
						  table_set,
						  U32 index,
						  const AnyReferee* value,
						  Uptr tableId)
{
	TableInstance* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	setTableElement(table, index, value);
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
		// Copy the passive table segment shared_ptr, and unlock the mutex. It's important to
		// explicitly unlock the mutex before calling setTableElement, as setTableElement trigger a
		// signal that will unwind the stack without calling the Lock destructor.
		std::shared_ptr<const std::vector<Object*>> passiveTableSegmentObjects
			= moduleInstance->passiveTableSegments[elemSegmentIndex];
		passiveTableSegmentsLock.unlock();

		TableInstance* table = getTableFromRuntimeData(contextRuntimeData, tableId);

		for(Uptr index = 0; index < numElements; ++index)
		{
			const U64 sourceIndex = U64(sourceOffset) + index;
			const U64 destIndex = U64(destOffset) + index;
			if(sourceIndex >= passiveTableSegmentObjects->size())
			{
				throwException(Exception::outOfBoundsElemSegmentAccessType,
							   {asAnyRef(moduleInstance), U64(elemSegmentIndex), sourceIndex});
			}

			setTableElement(
				table, Uptr(destIndex), asAnyRef((*passiveTableSegmentObjects)[sourceIndex]));
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

	if(sourceOffset != destOffset)
	{
		const Uptr numNonOverlappingElements
			= sourceOffset < destOffset && U64(sourceOffset) + U64(numElements) > destOffset
				  ? destOffset - sourceOffset
				  : numElements;

		// If the end of the source overlaps the beginning of the destination, copy those elements
		// before they are overwritten by the second part of the copy below.
		for(Uptr index = numNonOverlappingElements; index < numElements; ++index)
		{
			setTableElementAnyRef(table,
								  U64(destOffset) + U64(index),
								  getTableElementAnyRef(table, U64(sourceOffset) + U64(index)));
		}

		for(Uptr index = 0; index < numNonOverlappingElements; ++index)
		{
			setTableElementAnyRef(table,
								  U64(destOffset) + U64(index),
								  getTableElementAnyRef(table, U64(sourceOffset) + U64(index)));
		}
	}
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "callIndirectFail",
						  void,
						  callIndirectFail,
						  U32 index,
						  Uptr tableId,
						  Uptr functionPointerBits,
						  Uptr expectedTypeEncoding)
{
	TableInstance* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	AnyFunc* functionPointer = reinterpret_cast<AnyFunc*>(functionPointerBits);
	if(functionPointer == getOutOfBoundsAnyFunc())
	{
		Log::printf(Log::debug, "call_indirect: index %u is out-of-bounds\n", index);
		throwException(Exception::outOfBoundsTableAccessType, {asAnyRef(table), U64(index)});
	}
	else if(functionPointer == getUninitializedAnyFunc())
	{
		Log::printf(Log::debug, "call_indirect: index %u is uninitialized\n", index);
		throwException(Exception::uninitializedTableElementType, {asAnyRef(table), U64(index)});
	}
	else
	{
		IR::FunctionType expectedSignature{IR::FunctionType::Encoding{expectedTypeEncoding}};
		std::string ipDescription = "<unknown>";
		describeInstructionPointer(reinterpret_cast<Uptr>(functionPointer->code), ipDescription);
		Log::printf(Log::debug,
					"call_indirect: index %u has signature %s (%s), but was expecting %s\n",
					index,
					asString(IR::FunctionType{functionPointer->functionTypeEncoding}).c_str(),
					ipDescription.c_str(),
					asString(expectedSignature).c_str());
		throwException(Exception::indirectCallSignatureMismatchType);
	}
}
