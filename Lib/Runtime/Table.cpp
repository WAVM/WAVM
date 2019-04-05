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
static std::vector<Table*> tables;

enum
{
	numGuardPages = 1
};

static Uptr getNumPlatformPages(Uptr numBytes)
{
	return (numBytes + (Uptr(1) << Platform::getPageSizeLog2()) - 1) >> Platform::getPageSizeLog2();
}

static Function* makeDummyFunction(const char* debugName)
{
	FunctionMutableData* functionMutableData = new FunctionMutableData(debugName);
	Function* function
		= new Function(functionMutableData, UINTPTR_MAX, IR::FunctionType::Encoding{0});
	functionMutableData->function = function;
	return function;
}

Object* Runtime::getOutOfBoundsElement()
{
	static Function* function = makeDummyFunction("out-of-bounds table element");
	return asObject(function);
}

static Object* getUninitializedElement()
{
	static Function* function = makeDummyFunction("uninitialized table element");
	return asObject(function);
}

static Uptr objectToBiasedTableElementValue(Object* object)
{
	return reinterpret_cast<Uptr>(object) - reinterpret_cast<Uptr>(getOutOfBoundsElement());
}

static Object* biasedTableElementValueToObject(Uptr biasedValue)
{
	return reinterpret_cast<Object*>(biasedValue + reinterpret_cast<Uptr>(getOutOfBoundsElement()));
}

static Table* createTableImpl(Compartment* compartment, IR::TableType type, std::string&& debugName)
{
	Table* table = new Table(compartment, type, std::move(debugName));

	// In 64-bit, allocate enough address-space to safely access 32-bit table indices without bounds
	// checking, or 16MB (4M elements) if the host is 32-bit.
	const Uptr pageBytesLog2 = Platform::getPageSizeLog2();
	const U64 tableMaxElements = Uptr(1) << 32;
	const U64 tableMaxBytes = sizeof(Table::Element) * tableMaxElements;
	const U64 tableMaxPages = tableMaxBytes >> pageBytesLog2;

	table->elements
		= (Table::Element*)Platform::allocateVirtualPages(tableMaxPages + numGuardPages);
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

static Iptr growTableImpl(Table* table,
						  Uptr numElementsToGrow,
						  bool initializeNewElements,
						  Runtime::Object* initializeToElement = getUninitializedElement())
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
		= getNumPlatformPages(previousNumElements * sizeof(Table::Element));
	const Uptr newNumPlatformPages = getNumPlatformPages(newNumElements * sizeof(Table::Element));
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
				objectToBiasedTableElementValue(initializeToElement), std::memory_order_release);
		}
	}

	table->numElements.store(newNumElements, std::memory_order_release);
	return previousNumElements;
}

Table* Runtime::createTable(Compartment* compartment, IR::TableType type, std::string&& debugName)
{
	wavmAssert(type.size.min <= UINTPTR_MAX);
	Table* table = createTableImpl(compartment, type, std::move(debugName));
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

Table* Runtime::cloneTable(Table* table, Compartment* newCompartment)
{
	Lock<Platform::Mutex> resizingLock(table->resizingMutex);

	// Create the new table.
	const Uptr numElements = table->numElements.load(std::memory_order_acquire);
	std::string debugName = table->debugName;
	Table* newTable = createTableImpl(newCompartment, table->type, std::move(debugName));
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

Table::~Table()
{
	if(id != UINTPTR_MAX)
	{
		wavmAssertMutexIsLockedByCurrentThread(compartment->mutex);

		wavmAssert(compartment->tables[id] == this);
		compartment->tables.removeOrFail(id);

		wavmAssert(compartment->runtimeData->tableBases[id] == elements);
		compartment->runtimeData->tableBases[id] = nullptr;
	}

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

bool Runtime::isAddressOwnedByTable(U8* address, Table*& outTable, Uptr& outTableIndex)
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
			outTableIndex = (address - startAddress) / sizeof(Table::Element);
			return true;
		}
	}
	return false;
}

static Object* setTableElementNonNull(Table* table, Uptr index, Object* object)
{
	wavmAssert(object);

	// Verify the index is within the table's bounds.
	if(index >= table->numReservedElements)
	{ throwException(ExceptionTypes::outOfBoundsTableAccess, {table, U64(index)}); }

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the above bounds check.
	const Uptr saturatedIndex
		= Platform::saturateToBounds(index, U64(table->numReservedElements) - 1);

	// Compute the biased value to store in the table.
	const Uptr biasedValue = objectToBiasedTableElementValue(object);

	// Atomically replace the table element, throwing an out-of-bounds exception before the write if
	// the element being replaced is an out-of-bounds sentinel value.
	Uptr oldBiasedValue = table->elements[saturatedIndex].biasedValue;
	while(true)
	{
		if(biasedTableElementValueToObject(oldBiasedValue) == getOutOfBoundsElement())
		{ throwException(ExceptionTypes::outOfBoundsTableAccess, {table, U64(index)}); }
		if(table->elements[saturatedIndex].biasedValue.compare_exchange_weak(
			   oldBiasedValue, biasedValue, std::memory_order_acq_rel))
		{ break; }
	};

	return biasedTableElementValueToObject(oldBiasedValue);
}

static Object* getTableElementNonNull(Table* table, Uptr index)
{
	// Verify the index is within the table's bounds.
	if(index >= table->numReservedElements)
	{ throwException(ExceptionTypes::outOfBoundsTableAccess, {table, U64(index)}); }

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the above bounds check.
	const Uptr saturatedIndex
		= Platform::saturateToBounds(index, U64(table->numReservedElements) - 1);

	// Read the table element.
	const Uptr biasedValue
		= table->elements[saturatedIndex].biasedValue.load(std::memory_order_acquire);
	Object* object = biasedTableElementValueToObject(biasedValue);

	// If the element was an out-of-bounds sentinel value, throw an out-of-bounds exception.
	if(object == getOutOfBoundsElement())
	{ throwException(ExceptionTypes::outOfBoundsTableAccess, {table, U64(index)}); }

	wavmAssert(object);
	return object;
}

Object* Runtime::setTableElement(Table* table, Uptr index, Object* newValue)
{
	wavmAssert(!newValue || isInCompartment(newValue, table->compartment));

	// If the new value is null, write the uninitialized sentinel value instead.
	if(!newValue) { newValue = getUninitializedElement(); }

	// Write the table element.
	Object* oldObject = nullptr;
	Runtime::unwindSignalsAsExceptions([table, index, newValue, &oldObject] {
		oldObject = setTableElementNonNull(table, index, newValue);
	});

	// If the old table element was the uninitialized sentinel value, return null.
	return oldObject == getUninitializedElement() ? nullptr : oldObject;
}

Object* Runtime::getTableElement(Table* table, Uptr index)
{
	Object* object = nullptr;
	Runtime::unwindSignalsAsExceptions(
		[table, index, &object] { object = getTableElementNonNull(table, index); });

	// If the old table element was the uninitialized sentinel value, return null.
	return object == getUninitializedElement() ? nullptr : object;
}

Uptr Runtime::getTableNumElements(Table* table)
{
	return table->numElements.load(std::memory_order_acquire);
}

Iptr Runtime::growTable(Table* table, Uptr numNewElements, Object* initialElement)
{
	return growTableImpl(table, numNewElements, true, initialElement);
}

Iptr Runtime::shrinkTable(Table* table, Uptr numElementsToShrink)
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
		= getNumPlatformPages(previousNumElements * sizeof(Table::Element));
	const Uptr newNumPlatformPages = getNumPlatformPages(newNumElements * sizeof(Table::Element));
	if(newNumPlatformPages != previousNumPlatformPages)
	{
		Platform::decommitVirtualPages(
			(U8*)table->elements + (newNumPlatformPages << Platform::getPageSizeLog2()),
			(previousNumPlatformPages - newNumPlatformPages) << Platform::getPageSizeLog2());
	}

	// Write the out-of-bounds sentinel value to any removed elements that are between the new end
	// of the table and the first decommitted page.
	const Uptr numCommittedIndices
		= (newNumPlatformPages << Platform::getPageSizeLog2()) / sizeof(Table::Element);
	if(numCommittedIndices > newNumElements)
	{
		for(Uptr elementIndex = numCommittedIndices - 1; elementIndex > newNumElements;
			--elementIndex)
		{
			table->elements[elementIndex].biasedValue.store(
				objectToBiasedTableElementValue(getOutOfBoundsElement()),
				std::memory_order_release);
		}
	}

	table->numElements.store(newNumElements, std::memory_order_release);
	return previousNumElements;
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.grow",
						  U32,
						  table_grow,
						  Object* initialValue,
						  U32 deltaNumElements,
						  Uptr tableId)
{
	Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	const Iptr numTableElements = growTable(
		table, deltaNumElements, initialValue ? initialValue : getUninitializedElement());
	wavmAssert(numTableElements <= INT32_MAX);
	return I32(numTableElements);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "table.size", U32, table_size, Uptr tableId)
{
	Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	const Uptr numTableElements = getTableNumElements(table);
	wavmAssert(numTableElements <= UINT32_MAX);
	return U32(numTableElements);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "table.get", Object*, table_get, U32 index, Uptr tableId)
{
	Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	return getTableElement(table, index);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.set",
						  void,
						  table_set,
						  U32 index,
						  Object* value,
						  Uptr tableId)
{
	Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	setTableElement(table, index, value);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.init",
						  void,
						  table_init,
						  U32 destOffset,
						  U32 sourceOffset,
						  U32 numElements,
						  Uptr moduleInstanceId,
						  Uptr tableId,
						  Uptr elemSegmentIndex)
{
	ModuleInstance* moduleInstance
		= getModuleInstanceFromRuntimeData(contextRuntimeData, moduleInstanceId);
	Lock<Platform::Mutex> passiveElemSegmentsLock(moduleInstance->passiveElemSegmentsMutex);

	if(!moduleInstance->passiveElemSegments.contains(elemSegmentIndex))
	{ throwException(ExceptionTypes::invalidArgument); }
	else
	{
		const std::shared_ptr<const std::vector<Object*>>& passiveElemSegmentObjects
			= moduleInstance->passiveElemSegments[elemSegmentIndex];

		Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);

		if(!numElements)
		{
			// WebAssembly expects 0-sized inits to still trap for out-of-bounds adddresses.
			if(sourceOffset > passiveElemSegmentObjects->size())
			{
				throwException(ExceptionTypes::outOfBoundsElemSegmentAccess,
							   {asObject(moduleInstance), U64(elemSegmentIndex), sourceOffset});
			}
			else if(destOffset > getTableNumElements(table))
			{
				throwException(ExceptionTypes::outOfBoundsTableAccess, {table, U64(destOffset)});
			}
		}
		else
		{
			for(Uptr index = 0; index < numElements; ++index)
			{
				const U64 sourceIndex = U64(sourceOffset) + index;
				const U64 destIndex = U64(destOffset) + index;
				if(sourceIndex >= passiveElemSegmentObjects->size())
				{
					throwException(ExceptionTypes::outOfBoundsElemSegmentAccess,
								   {asObject(moduleInstance), U64(elemSegmentIndex), sourceIndex});
				}

				setTableElement(
					table, Uptr(destIndex), asObject((*passiveElemSegmentObjects)[sourceIndex]));
			}
		}
	}
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "elem.drop",
						  void,
						  elem_drop,
						  Uptr moduleInstanceId,
						  Uptr elemSegmentIndex)
{
	ModuleInstance* moduleInstance
		= getModuleInstanceFromRuntimeData(contextRuntimeData, moduleInstanceId);
	Lock<Platform::Mutex> passiveElemSegmentsLock(moduleInstance->passiveElemSegmentsMutex);

	if(!moduleInstance->passiveElemSegments.contains(elemSegmentIndex))
	{ throwException(ExceptionTypes::invalidArgument); }
	else
	{
		moduleInstance->passiveElemSegments.removeOrFail(elemSegmentIndex);
	}
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.copy",
						  void,
						  table_copy,
						  U32 destOffset,
						  U32 sourceOffset,
						  U32 numElements,
						  Uptr sourceTableId,
						  Uptr destTableId)
{
	Runtime::unwindSignalsAsExceptions([=] {
		Table* sourceTable = getTableFromRuntimeData(contextRuntimeData, sourceTableId);
		Table* destTable = getTableFromRuntimeData(contextRuntimeData, destTableId);

		if(!numElements)
		{
			// WebAssembly expects 0-sized copies to still trap for out-of-bounds addresses.
			if(sourceOffset > getTableNumElements(sourceTable))
			{
				throwException(ExceptionTypes::outOfBoundsTableAccess,
							   {sourceTable, U64(sourceOffset)});
			}
			else if(destOffset > getTableNumElements(destTable))
			{
				throwException(ExceptionTypes::outOfBoundsTableAccess,
							   {destTable, U64(destOffset)});
			}
		}
		else if(sourceOffset < destOffset && U64(sourceOffset) + U64(numElements) > destOffset)
		{
			// If the end of the source range overlaps the beginning of the destination range,
			// copy the elements in reverse order so source elements are copied before they are
			// overwritten.
			for(Uptr index = 0; index < numElements; ++index)
			{
				setTableElementNonNull(
					destTable,
					U64(destOffset) + U64(numElements - index - 1),
					getTableElementNonNull(sourceTable,
										   U64(sourceOffset) + U64(numElements - index - 1)));
			}
		}
		else
		{
			for(Uptr index = 0; index < numElements; ++index)
			{
				setTableElementNonNull(
					destTable,
					U64(destOffset) + U64(index),
					getTableElementNonNull(sourceTable, U64(sourceOffset) + U64(index)));
			}
		}
	});
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.fill",
						  void,
						  table_fill,
						  U32 destOffset,
						  Runtime::Object* value,
						  U32 numElements,
						  Uptr destTableId)
{
	Table* destTable = getTableFromRuntimeData(contextRuntimeData, destTableId);

	// If the value is null, write the uninitialized sentinel value instead.
	if(!value) { value = getUninitializedElement(); }

	if(!numElements)
	{
		// WebAssembly expects 0-sized fills to still trap for out-of-bounds addresses.
		if(destOffset > getTableNumElements(destTable))
		{ throwException(ExceptionTypes::outOfBoundsTableAccess, {destTable, U64(destOffset)}); }
	}
	else
	{
		Runtime::unwindSignalsAsExceptions([=] {
			for(Uptr index = 0; index < numElements; ++index)
			{ setTableElementNonNull(destTable, U64(destOffset) + U64(index), value); }
		});
	}
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "callIndirectFail",
						  void,
						  callIndirectFail,
						  U32 index,
						  Uptr tableId,
						  Function* function,
						  Uptr expectedTypeEncoding)
{
	Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	if(asObject(function) == getOutOfBoundsElement())
	{
		Log::printf(Log::debug, "call_indirect: index %u is out-of-bounds\n", index);
		throwException(ExceptionTypes::outOfBoundsTableAccess, {table, U64(index)});
	}
	else if(asObject(function) == getUninitializedElement())
	{
		Log::printf(Log::debug, "call_indirect: index %u is uninitialized\n", index);
		throwException(ExceptionTypes::uninitializedTableElement, {table, U64(index)});
	}
	else
	{
		IR::FunctionType expectedSignature{IR::FunctionType::Encoding{expectedTypeEncoding}};
		std::string ipDescription = "<unknown>";
		describeInstructionPointer(reinterpret_cast<Uptr>(function->code), ipDescription);
		Log::printf(Log::debug,
					"call_indirect: index %u has signature %s (%s), but was expecting %s\n",
					index,
					asString(IR::FunctionType{function->encodedType}).c_str(),
					ipDescription.c_str(),
					asString(expectedSignature).c_str());
		throwException(ExceptionTypes::indirectCallSignatureMismatch);
	}
}
