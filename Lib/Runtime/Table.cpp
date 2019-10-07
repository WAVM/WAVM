#include <stdint.h>
#include <string.h>
#include <vector>
#include "RuntimePrivate.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Intrinsic.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Platform/RWMutex.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

using namespace WAVM;
using namespace WAVM::Runtime;

namespace WAVM { namespace Runtime {
	WAVM_DEFINE_INTRINSIC_MODULE(wavmIntrinsicsTable)
}}

// Global lists of tables; used to query whether an address is reserved by one of them.
static Platform::RWMutex tablesMutex;
static std::vector<Table*> tables;

static constexpr Uptr numGuardPages = 1;

static Uptr getNumPlatformPages(Uptr numBytes)
{
	return (numBytes + Platform::getBytesPerPage() - 1) >> Platform::getBytesPerPageLog2();
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

static Table* createTableImpl(Compartment* compartment,
							  IR::TableType type,
							  std::string&& debugName,
							  ResourceQuotaRefParam resourceQuota)
{
	Table* table = new Table(compartment, type, std::move(debugName), resourceQuota);

	// In 64-bit, allocate enough address-space to safely access 32-bit table indices without bounds
	// checking, or 16MB (4M elements) if the host is 32-bit.
	const Uptr pageBytesLog2 = Platform::getBytesPerPageLog2();
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
		Platform::RWMutex::ExclusiveLock tablesLock(tablesMutex);
		tables.push_back(table);
	}
	return table;
}

static bool growTableImpl(Table* table,
						  Uptr numElementsToGrow,
						  Uptr* outOldNumElements,
						  bool initializeNewElements,
						  Runtime::Object* initializeToElement = getUninitializedElement())
{
	Uptr oldNumElements;
	if(!numElementsToGrow) { oldNumElements = table->numElements.load(std::memory_order_acquire); }
	else
	{
		// Check the table element quota.
		if(table->resourceQuota && !table->resourceQuota->tableElems.allocate(numElementsToGrow))
		{ return false; }

		Platform::RWMutex::ExclusiveLock resizingLock(table->resizingMutex);

		oldNumElements = table->numElements.load(std::memory_order_acquire);

		// If the growth would cause the table's size to exceed its maximum, return -1.
		if(numElementsToGrow > table->type.size.max
		   || oldNumElements > table->type.size.max - numElementsToGrow
		   || numElementsToGrow > IR::maxTableElems
		   || oldNumElements > IR::maxTableElems - numElementsToGrow)
		{
			if(table->resourceQuota) { table->resourceQuota->tableElems.free(numElementsToGrow); }
			return false;
		}

		// Try to commit pages for the new elements, and return -1 if the commit fails.
		const Uptr newNumElements = oldNumElements + numElementsToGrow;
		const Uptr previousNumPlatformPages
			= getNumPlatformPages(oldNumElements * sizeof(Table::Element));
		const Uptr newNumPlatformPages
			= getNumPlatformPages(newNumElements * sizeof(Table::Element));
		if(newNumPlatformPages != previousNumPlatformPages
		   && !Platform::commitVirtualPages(
			   (U8*)table->elements + (previousNumPlatformPages << Platform::getBytesPerPageLog2()),
			   newNumPlatformPages - previousNumPlatformPages))
		{
			if(table->resourceQuota) { table->resourceQuota->tableElems.free(numElementsToGrow); }
			return false;
		}

		if(initializeNewElements)
		{
			// Write the uninitialized sentinel value to the new elements.
			const Uptr biasedTableInitElement
				= objectToBiasedTableElementValue(initializeToElement);
			for(Uptr elementIndex = oldNumElements; elementIndex < newNumElements; ++elementIndex)
			{
				table->elements[elementIndex].biasedValue.store(biasedTableInitElement,
																std::memory_order_release);
			}
		}

		table->numElements.store(newNumElements, std::memory_order_release);
	}

	if(outOldNumElements) { *outOldNumElements = oldNumElements; }
	return true;
}

Table* Runtime::createTable(Compartment* compartment,
							IR::TableType type,
							Object* element,
							std::string&& debugName,
							ResourceQuotaRefParam resourceQuota)
{
	WAVM_ASSERT(type.size.min <= UINTPTR_MAX);
	Table* table = createTableImpl(compartment, type, std::move(debugName), resourceQuota);
	if(!table) { return nullptr; }

	// If element is null, use the uninitialized element sentinel instead.
	if(!element) { element = getUninitializedElement(); }
	else
	{
		WAVM_ERROR_UNLESS(isSubtype(asReferenceType(getExternType(element)), type.elementType));
	}

	// Grow the table to the type's minimum size.
	if(!growTableImpl(table, Uptr(type.size.min), nullptr, true, element))
	{
		delete table;
		return nullptr;
	}

	// Add the table to the compartment's tables IndexMap.
	{
		Platform::RWMutex::ExclusiveLock compartmentLock(compartment->mutex);

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
	Platform::RWMutex::ExclusiveLock resizingLock(table->resizingMutex);

	// Create the new table.
	const Uptr numElements = table->numElements.load(std::memory_order_acquire);
	std::string debugName = table->debugName;
	Table* newTable
		= createTableImpl(newCompartment, table->type, std::move(debugName), table->resourceQuota);
	if(!newTable) { return nullptr; }

	// Grow the table to the same size as the original, without initializing the new elements since
	// they will be written immediately following this.
	if(!growTableImpl(newTable, numElements, nullptr, false))
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
		Platform::RWMutex::ExclusiveLock compartmentLock(newCompartment->mutex);

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
		WAVM_ASSERT_RWMUTEX_IS_EXCLUSIVELY_LOCKED_BY_CURRENT_THREAD(compartment->mutex);

		WAVM_ASSERT(compartment->tables[id] == this);
		compartment->tables.removeOrFail(id);

		WAVM_ASSERT(compartment->runtimeData->tableBases[id] == elements);
		compartment->runtimeData->tableBases[id] = nullptr;
	}

	// Remove the table from the global array.
	{
		Platform::RWMutex::ExclusiveLock tablesLock(tablesMutex);
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
	const Uptr pageBytesLog2 = Platform::getBytesPerPageLog2();
	if(numReservedBytes > 0)
	{
		Platform::freeVirtualPages((U8*)elements,
								   (numReservedBytes >> pageBytesLog2) + numGuardPages);
	}

	// Free the allocated quota.
	if(resourceQuota) { resourceQuota->tableElems.free(numElements); }
}

bool Runtime::isAddressOwnedByTable(U8* address, Table*& outTable, Uptr& outTableIndex)
{
	// Iterate over all tables and check if the address is within the reserved address space for
	// each.
	Platform::RWMutex::ShareableLock tablesLock(tablesMutex);
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
	WAVM_ASSERT(object);

	// Verify the index is within the table's bounds.
	if(index >= table->numReservedElements)
	{ throwException(ExceptionTypes::outOfBoundsTableAccess, {table, U64(index)}); }

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the above bounds check.
	const Uptr saturatedIndex = branchlessMin(index, U64(table->numReservedElements) - 1);

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

static Object* getTableElementNonNull(const Table* table, Uptr index)
{
	// Verify the index is within the table's bounds.
	if(index >= table->numReservedElements)
	{
		throwException(ExceptionTypes::outOfBoundsTableAccess,
					   {const_cast<Table*>(table), U64(index)});
	}

	// Use a saturated index to access the table data to ensure that it's harmless for the CPU to
	// speculate past the above bounds check.
	const Uptr saturatedIndex = branchlessMin(index, U64(table->numReservedElements) - 1);

	// Read the table element.
	const Uptr biasedValue
		= table->elements[saturatedIndex].biasedValue.load(std::memory_order_acquire);
	Object* object = biasedTableElementValueToObject(biasedValue);

	// If the element was an out-of-bounds sentinel value, throw an out-of-bounds exception.
	if(object == getOutOfBoundsElement())
	{
		throwException(ExceptionTypes::outOfBoundsTableAccess,
					   {const_cast<Table*>(table), U64(index)});
	}

	WAVM_ASSERT(object);
	return object;
}

Object* Runtime::setTableElement(Table* table, Uptr index, Object* newValue)
{
	WAVM_ASSERT(!newValue || isInCompartment(newValue, table->compartment));

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

Object* Runtime::getTableElement(const Table* table, Uptr index)
{
	Object* object = nullptr;
	Runtime::unwindSignalsAsExceptions(
		[table, index, &object] { object = getTableElementNonNull(table, index); });

	// If the old table element was the uninitialized sentinel value, return null.
	return object == getUninitializedElement() ? nullptr : object;
}

Uptr Runtime::getTableNumElements(const Table* table)
{
	return table->numElements.load(std::memory_order_acquire);
}

IR::TableType Runtime::getTableType(const Table* table) { return table->type; }

bool Runtime::growTable(Table* table,
						Uptr numElementsToGrow,
						Uptr* outOldNumElements,
						Object* initialElement)
{
	return growTableImpl(table, numElementsToGrow, outOldNumElements, true, initialElement);
}

void Runtime::initElemSegment(ModuleInstance* moduleInstance,
							  Uptr elemSegmentIndex,
							  const IR::ElemSegment::Contents* contents,
							  Table* table,
							  Uptr destOffset,
							  Uptr sourceOffset,
							  Uptr numElems)
{
	Uptr numSourceIndices;
	switch(contents->encoding)
	{
	case IR::ElemSegment::Encoding::expr: numSourceIndices = contents->elemExprs.size(); break;
	case IR::ElemSegment::Encoding::index: numSourceIndices = contents->elemIndices.size(); break;
	default: WAVM_UNREACHABLE();
	};

	for(Uptr index = 0; index < numElems; ++index)
	{
		Uptr sourceIndex = sourceOffset + index;
		const Uptr destIndex = destOffset + index;

		if(sourceIndex >= numSourceIndices || sourceIndex < sourceOffset)
		{
			throwException(ExceptionTypes::outOfBoundsElemSegmentAccess,
						   {asObject(moduleInstance), U64(elemSegmentIndex), sourceIndex});
		}
		sourceIndex = branchlessMin(sourceIndex, numSourceIndices);

		// Decode the element value.
		Object* elemObject = nullptr;
		switch(contents->encoding)
		{
		case IR::ElemSegment::Encoding::expr: {
			const IR::ElemExpr& elemExpr = contents->elemExprs[sourceIndex];
			switch(elemExpr.type)
			{
			case IR::ElemExpr::Type::ref_null: elemObject = nullptr; break;
			case IR::ElemExpr::Type::ref_func:
				elemObject = asObject(moduleInstance->functions[elemExpr.index]);
				break;
			default: WAVM_UNREACHABLE();
			}
			break;
		}
		case IR::ElemSegment::Encoding::index: {
			const Uptr externIndex = contents->elemIndices[sourceIndex];
			switch(contents->externKind)
			{
			case IR::ExternKind::function:
				elemObject = asObject(moduleInstance->functions[externIndex]);
				break;
			case IR::ExternKind::table:
				elemObject = asObject(moduleInstance->tables[externIndex]);
				break;
			case IR::ExternKind::memory:
				elemObject = asObject(moduleInstance->memories[externIndex]);
				break;
			case IR::ExternKind::global:
				elemObject = asObject(moduleInstance->globals[externIndex]);
				break;
			case IR::ExternKind::exceptionType:
				elemObject = asObject(moduleInstance->exceptionTypes[externIndex]);
				break;

			case IR::ExternKind::invalid:
			default: WAVM_UNREACHABLE();
			}
			break;
		}
		default: WAVM_UNREACHABLE();
		};

		setTableElement(table, destIndex, elemObject);
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsTable,
							   "table.grow",
							   I32,
							   table_grow,
							   Object* initialValue,
							   U32 deltaNumElements,
							   Uptr tableId)
{
	Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	Uptr oldNumElements = 0;
	if(!growTable(table,
				  deltaNumElements,
				  &oldNumElements,
				  initialValue ? initialValue : getUninitializedElement()))
	{ return -1; }
	WAVM_ASSERT(oldNumElements <= INT32_MAX);
	return I32(oldNumElements);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsTable, "table.size", U32, table_size, Uptr tableId)
{
	Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	const Uptr numTableElements = getTableNumElements(table);
	WAVM_ASSERT(numTableElements <= UINT32_MAX);
	return U32(numTableElements);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsTable,
							   "table.get",
							   Object*,
							   table_get,
							   U32 index,
							   Uptr tableId)
{
	Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);
	return getTableElement(table, index);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsTable,
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

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsTable,
							   "table.init",
							   void,
							   table_init,
							   U32 destIndex,
							   U32 sourceIndex,
							   U32 numElems,
							   Uptr moduleInstanceId,
							   Uptr tableId,
							   Uptr elemSegmentIndex)
{
	ModuleInstance* moduleInstance
		= getModuleInstanceFromRuntimeData(contextRuntimeData, moduleInstanceId);
	Table* table = getTableFromRuntimeData(contextRuntimeData, tableId);

	Platform::RWMutex::ShareableLock elemSegmentsLock(moduleInstance->elemSegmentsMutex);
	if(!moduleInstance->elemSegments[elemSegmentIndex])
	{ throwException(ExceptionTypes::invalidArgument); }
	else
	{
		// Make a copy of the shared_ptr to the segment contents and unlock the elem segments mutex.
		std::shared_ptr<IR::ElemSegment::Contents> contents
			= moduleInstance->elemSegments[elemSegmentIndex];
		elemSegmentsLock.unlock();

		initElemSegment(moduleInstance,
						elemSegmentIndex,
						contents.get(),
						table,
						destIndex,
						sourceIndex,
						numElems);
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsTable,
							   "elem.drop",
							   void,
							   elem_drop,
							   Uptr moduleInstanceId,
							   Uptr elemSegmentIndex)
{
	ModuleInstance* moduleInstance
		= getModuleInstanceFromRuntimeData(contextRuntimeData, moduleInstanceId);
	Platform::RWMutex::ExclusiveLock elemSegmentsLock(moduleInstance->elemSegmentsMutex);

	if(!moduleInstance->elemSegments[elemSegmentIndex])
	{ throwException(ExceptionTypes::invalidArgument); }
	else
	{
		moduleInstance->elemSegments[elemSegmentIndex].reset();
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsTable,
							   "table.copy",
							   void,
							   table_copy,
							   U32 destOffset,
							   U32 sourceOffset,
							   U32 numElements,
							   Uptr destTableId,
							   Uptr sourceTableId)
{
	Runtime::unwindSignalsAsExceptions([=] {
		Table* destTable = getTableFromRuntimeData(contextRuntimeData, destTableId);
		Table* sourceTable = getTableFromRuntimeData(contextRuntimeData, sourceTableId);

		if(sourceOffset < destOffset)
		{
			// When copying to higher indices, copy the elements in descending order to ensure that
			// source elements may only be overwritten after they have been copied.
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

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsTable,
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

	Runtime::unwindSignalsAsExceptions([=] {
		for(Uptr index = 0; index < numElements; ++index)
		{ setTableElementNonNull(destTable, U64(destOffset) + U64(index), value); }
	});
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsTable,
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
		Log::printf(Log::debug,
					"call_indirect: index %u has signature %s (%s), but was expecting %s\n",
					index,
					asString(IR::FunctionType{function->encodedType}).c_str(),
					function->mutableData->debugName.c_str(),
					asString(expectedSignature).c_str());
		throwException(ExceptionTypes::indirectCallSignatureMismatch);
	}
}
