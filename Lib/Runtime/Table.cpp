#include <stdint.h>
#include <string.h>
#include <vector>

#include "IR/Types.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Lock.h"
#include "LLVMJIT/LLVMJIT.h"
#include "Logging/Logging.h"
#include "Platform/Intrinsic.h"
#include "Platform/Memory.h"
#include "Platform/Mutex.h"
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

static AnyFunc* makeDummyAnyFunc()
{
	AnyFunc* anyFunc = new AnyFunc;
	anyFunc->anyRef.object = nullptr;
	anyFunc->functionTypeEncoding = IR::FunctionType::Encoding{0};
	anyFunc->code[0] = 0xcc; // int3
	return anyFunc;
}

// This is used as a sentinel value for table elements that are out-of-bounds. The address of this
// AnyFunc is subtracted from every address stored in the table, so zero-initialized pages at the
// end of the array will, when re-adding this AnyFunc's address, point to this AnyFunc.
static AnyFunc* getOutOfBoundsAnyFunc()
{
	static AnyFunc* anyFunc = makeDummyAnyFunc();
	return anyFunc;
}

// A sentinel value that is used for uninitialized table elements.
static AnyFunc* getUninitializedAnyFunc()
{
	static AnyFunc* anyFunc = makeDummyAnyFunc();
	return anyFunc;
}

static Uptr anyRefToBiasedTableElementValue(AnyReferee* anyRef)
{
	return reinterpret_cast<Uptr>(anyRef) - reinterpret_cast<Uptr>(getOutOfBoundsAnyFunc());
}

static AnyReferee* biasedTableElementValueToAnyRef(Uptr biasedValue)
{
	return reinterpret_cast<AnyReferee*>(biasedValue
										 + reinterpret_cast<Uptr>(getOutOfBoundsAnyFunc()));
}

Uptr TableInstance::getReferenceBias() { return reinterpret_cast<Uptr>(getOutOfBoundsAnyFunc()); }

static TableInstance* createTableImpl(Compartment* compartment,
									  IR::TableType type,
									  Uptr numElements)
{
	TableInstance* table = new TableInstance(compartment, type);

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
		compartment->runtimeData->tableBases[table->id] = table->elements;
	}

	return table;
}

TableInstance* Runtime::cloneTable(TableInstance* table, Compartment* newCompartment)
{
	const Uptr initialNumElements = table->numElements;
	TableInstance* newTable = createTableImpl(newCompartment, table->type, initialNumElements);
	if(!newTable) { return nullptr; }

	newTable->id = table->id;
	for(Uptr elementIndex = 0;
		elementIndex < table->numElements && elementIndex < initialNumElements;
		++elementIndex)
	{
		newTable->elements[elementIndex].biasedValue
			= table->elements[elementIndex].biasedValue.load();
	}

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
	numReservedBytes = numReservedElements = 0;

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
		U8* startAddress = (U8*)table->elements;
		U8* endAddress = ((U8*)table->elements) + table->numReservedBytes;
		if(address >= startAddress && address < endAddress) { return true; }
	}
	return false;
}

static AnyReferee* setTableElementAnyRef(TableInstance* table, Uptr index, AnyReferee* anyRef)
{
	// Verify the index is within the table's bounds.
	if(index >= table->numReservedElements)
	{ throwException(Exception::tableIndexOutOfBoundsType); }

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
		{ throwException(Exception::tableIndexOutOfBoundsType); }
		if(table->elements[saturatedIndex].biasedValue.compare_exchange_weak(
			   oldBiasedValue, biasedValue, std::memory_order_seq_cst))
		{ break; }
	};

	return biasedTableElementValueToAnyRef(oldBiasedValue);
}

static AnyReferee* getTableElementAnyRef(TableInstance* table, Uptr index)
{
	// Verify the index is within the table's bounds.
	if(index >= table->numReservedElements)
	{ throwException(Exception::tableIndexOutOfBoundsType); }

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the above bounds check.
	const Uptr saturatedIndex
		= Platform::saturateToBounds(index, U64(table->numReservedElements) - 1);

	// Read the table element.
	const Uptr biasedValue = table->elements[saturatedIndex].biasedValue;
	AnyReferee* anyRef = biasedTableElementValueToAnyRef(biasedValue);

	// If the element was an out-of-bounds sentinel value, throw an out-of-bounds exception.
	if(anyRef == &getOutOfBoundsAnyFunc()->anyRef)
	{ throwException(Exception::tableIndexOutOfBoundsType); }

	return anyRef;
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
			functionInstance,
			functionInstance->type,
			functionInstance->callingConvention,
			LLVMJIT::MemoryBinding{intrinsicDefaultMemory ? intrinsicDefaultMemory->id
														  : UINTPTR_MAX},
			LLVMJIT::TableBinding{intrinsicDefaultTable ? intrinsicDefaultTable->id : UINTPTR_MAX});
	}

	// Get the pointer to the Anyfunc struct that is emitted as a prefix to the function's code.
	AnyFunc* anyFunc = (AnyFunc*)((U8*)nativeFunction - offsetof(AnyFunc, code));
	wavmAssert(anyFunc->anyRef.object == functionInstance);
	wavmAssert(IR::FunctionType{anyFunc->functionTypeEncoding} == functionInstance->type);

	// Write the table element.
	AnyReferee* oldAnyRef = setTableElementAnyRef(table, index, &anyFunc->anyRef);

	// Translate the old table element to an object pointer to return. If the old table element was
	// the uninitialized sentinel value, return null.
	if(oldAnyRef == &getUninitializedAnyFunc()->anyRef) { return nullptr; }
	else
	{
		return oldAnyRef->object;
	}
}

Object* Runtime::getTableElement(TableInstance* table, Uptr index)
{
	AnyReferee* anyRef = getTableElementAnyRef(table, index);

	// Translate the old table element to an object pointer to return. If the old table element was
	// the uninitialized sentinel value, return null. If it was an out-of-bounds sentinel value,
	// throw an exception.
	if(anyRef == &getOutOfBoundsAnyFunc()->anyRef)
	{ throwException(Exception::tableIndexOutOfBoundsType); }
	else if(anyRef == &getUninitializedAnyFunc()->anyRef)
	{
		return nullptr;
	}
	else
	{
		return anyRef->object;
	}
}

Uptr Runtime::getTableNumElements(TableInstance* table) { return table->numElements; }

Iptr Runtime::growTable(TableInstance* table, Uptr numNewElements)
{
	if(!numNewElements) { return table->numElements; }

	Lock<Platform::Mutex> resizingLock(table->resizingMutex);
	const Uptr previousNumElements = table->numElements;

	// If the number of elements to grow would cause the table's size to exceed its maximum,
	// return -1.
	if(numNewElements > table->type.size.max
	   || table->numElements > table->type.size.max - numNewElements)
	{ return -1; }

	// Try to commit pages for the new elements, and return -1 if the commit fails.
	const Uptr previousNumPlatformPages
		= getNumPlatformPages(table->numElements * sizeof(TableInstance::Element));
	const Uptr newNumPlatformPages = getNumPlatformPages((table->numElements + numNewElements)
														 * sizeof(TableInstance::Element));
	if(newNumPlatformPages != previousNumPlatformPages
	   && !Platform::commitVirtualPages(
			  (U8*)table->elements + (previousNumPlatformPages << Platform::getPageSizeLog2()),
			  newNumPlatformPages - previousNumPlatformPages))
	{ return -1; }

	// Write the uninitialized sentinel value to the new elements.
	for(Uptr elementIndex = previousNumElements;
		elementIndex < previousNumElements + numNewElements;
		++elementIndex)
	{
		table->elements[elementIndex].biasedValue
			= anyRefToBiasedTableElementValue(&getUninitializedAnyFunc()->anyRef);
	}

	table->numElements += numNewElements;
	wavmAssert(table->numElements == previousNumElements + numNewElements);

	return previousNumElements;
}

Iptr Runtime::shrinkTable(TableInstance* table, Uptr numElementsToShrink)
{
	if(!numElementsToShrink) { return table->numElements; }

	Lock<Platform::Mutex> resizingLock(table->resizingMutex);
	const Uptr previousNumElements = table->numElements;

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
		= getNumPlatformPages(previousNumElements * sizeof(TableInstance::Element));
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
			table->elements[elementIndex].biasedValue
				= anyRefToBiasedTableElementValue(&getOutOfBoundsAnyFunc()->anyRef);
		}
	}

	table->numElements = newNumElements;

	return previousNumElements;
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.get",
						  AnyReferee*,
						  table_get,
						  U32 index,
						  Uptr tableId)
{
	TableInstance* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	if(index >= table->numReservedElements)
	{ throwException(Exception::tableIndexOutOfBoundsType); }
	const Uptr saturatedIndex
		= Platform::saturateToBounds(Uptr(index), table->numReservedElements - 1);
	return reinterpret_cast<AnyReferee*>(reinterpret_cast<Uptr>(getOutOfBoundsAnyFunc())
										 + table->elements[saturatedIndex].biasedValue);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "table.set",
						  void,
						  table_set,
						  U32 index,
						  AnyReferee* value,
						  Uptr tableId)
{
	TableInstance* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	const Uptr saturatedIndex
		= Platform::saturateToBounds(Uptr(index), table->numReservedElements - 1);
	if(index >= table->numReservedElements)
	{ throwException(Exception::tableIndexOutOfBoundsType); }
	table->elements[saturatedIndex].biasedValue = anyRefToBiasedTableElementValue(value);
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
			{ throwException(Exception::tableIndexOutOfBoundsType); }

			setTableElement(table,
							Uptr(destIndex),
							(*passiveTableSegmentObjects)[sourceIndex],
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
						  Uptr functionPointerBits,
						  Uptr expectedTypeEncoding)
{
	AnyFunc* functionPointer = reinterpret_cast<AnyFunc*>(functionPointerBits);
	if(functionPointer == getOutOfBoundsAnyFunc())
	{
		Log::printf(Log::debug, "call_indirect: index %u is out-of-bounds\n", index);
		throwException(Exception::tableIndexOutOfBoundsType);
	}
	else if(functionPointer == getUninitializedAnyFunc())
	{
		Log::printf(Log::debug, "call_indirect: index %u is uninitialized\n", index);
		throwException(Exception::uninitializedTableElementType);
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
