#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	// Global lists of tables; used to query whether an address is reserved by one of them.
	static Platform::Mutex tablesMutex;
	static std::vector<TableInstance*> tables;

	enum { numGuardPages = 1 };

	static Uptr getNumPlatformPages(Uptr numBytes)
	{
		return (numBytes + (Uptr(1)<<Platform::getPageSizeLog2()) - 1) >> Platform::getPageSizeLog2();
	}

	TableInstance* createTable(Compartment* compartment,TableType type)
	{
		TableInstance* table = new TableInstance(compartment,type);

		// In 64-bit, allocate enough address-space to safely access 32-bit table indices without bounds checking, or 16MB (4M elements) if the host is 32-bit.
		const Uptr pageBytesLog2 = Platform::getPageSizeLog2();
		const Uptr tableMaxBytes = Uptr(U64(sizeof(TableInstance::FunctionElement)) << 32);
		const Uptr tableMaxPages = tableMaxBytes >> pageBytesLog2;
		
		table->baseAddress = (TableInstance::FunctionElement*)Platform::allocateVirtualPages(tableMaxPages + numGuardPages);
		table->endOffset = tableMaxBytes;
		if(!table->baseAddress) { delete table; return nullptr; }
		
		// Grow the table to the type's minimum size.
		wavmAssert(type.size.min <= UINTPTR_MAX);
		if(growTable(table,Uptr(type.size.min)) == -1) { delete table; return nullptr; }

		// Add the table to the compartment.
		if(compartment)
		{
			Platform::Lock compartmentLock(compartment->mutex);

			if(compartment->tables.size() >= maxTables) { delete table; return nullptr; }

			table->id = compartment->tables.size();
			compartment->tables.push_back(table);
			compartment->runtimeData->tables[table->id] = table->baseAddress;
		}

		// Add the table to the global array.
		{
			Platform::Lock tablesLock(tablesMutex);
			tables.push_back(table);
		}
		return table;
	}
	
	TableInstance* cloneTable(TableInstance* table,Compartment* newCompartment)
	{
		Platform::Lock elementsLock(table->elementsMutex);
		TableInstance* newTable = createTable(newCompartment,table->type);
		growTable(newTable,table->elements.size());
		newTable->elements = table->elements;
		memcpy(
			newTable->baseAddress,
			table->baseAddress,
			table->elements.size() * sizeof(TableInstance::FunctionElement));
		return newTable;
	}

	void TableInstance::finalize()
	{
		Platform::Lock compartmentLock(compartment->mutex);
		wavmAssert(compartment->tables[id] == this);
		wavmAssert(compartment->runtimeData->tables[id] == baseAddress);
		compartment->tables[id] = nullptr;
		compartment->runtimeData->tables[id] = nullptr;
	}

	TableInstance::~TableInstance()
	{
		// Decommit all pages.
		if(elements.size() > 0) { Platform::decommitVirtualPages((U8*)baseAddress,getNumPlatformPages(elements.size() * sizeof(TableInstance::FunctionElement))); }

		// Free the virtual address space.
		const Uptr pageBytesLog2 = Platform::getPageSizeLog2();
		if(endOffset > 0)
		{
			Platform::freeVirtualPages((U8*)baseAddress,(endOffset >> pageBytesLog2) + numGuardPages);
		}
		baseAddress = nullptr;
		
		// Remove the table from the global array.
		{
			Platform::Lock tablesLock(tablesMutex);
			for(Uptr tableIndex = 0;tableIndex < tables.size();++tableIndex)
			{
				if(tables[tableIndex] == this) { tables.erase(tables.begin() + tableIndex); break; }
			}
		}
	}

	bool isAddressOwnedByTable(U8* address)
	{
		// Iterate over all tables and check if the address is within the reserved address space for each.
		Platform::Lock tablesLock(tablesMutex);
		for(auto table : tables)
		{
			U8* startAddress = (U8*)table->baseAddress;
			U8* endAddress = ((U8*)table->baseAddress) + table->endOffset;
			if(address >= startAddress && address < endAddress) { return true; }
		}
		return false;
	}

	Object* setTableElement(TableInstance* table,Uptr index,Object* newValue)
	{
		// Look up the new function's code pointer.
		FunctionInstance* functionInstance = asFunction(newValue);
		void* nativeFunction = functionInstance->nativeFunction;
		wavmAssert(nativeFunction);

		// If the function isn't a WASM function, generate a thunk for it.
		if(functionInstance->callingConvention != CallingConvention::wasm)
		{
			nativeFunction = LLVMJIT::getIntrinsicThunk(
				nativeFunction,
				functionInstance->type,
				functionInstance->callingConvention);
		}

		// Lock the table's elements array.
		Platform::Lock elementsLock(table->elementsMutex);

		// Verify the index is within the table's bounds.
		if(index >= table->elements.size()) { throwException(Exception::accessViolationType); }
		
		// Use a saturated index to access the table data to ensure that it's harmless for the CPU to speculate past
		// the above bounds check.
		const Uptr saturatedIndex = Platform::saturateToBounds(index,table->elements.size());

		// Write the new table element to both the table's elements array and its indirect function call data.
		table->baseAddress[saturatedIndex].type = functionInstance->type;
		table->baseAddress[saturatedIndex].value = nativeFunction;

		auto oldValue = table->elements[saturatedIndex];
		table->elements[saturatedIndex] = newValue;
		return oldValue;
	}

	Object* getTableElement(TableInstance* table,Uptr index)
	{
		// Verify the index is within the table's bounds.
		if(index >= table->elements.size()) { throwException(Exception::accessViolationType); }

		// Use a saturated index to access the table data to ensure that it's harmless for the CPU to speculate past
		// the above bounds check.
		const Uptr saturatedIndex = Platform::saturateToBounds(index,table->elements.size());

		// Read from the table's elements array.
		{
			Platform::Lock elementsLock(table->elementsMutex);
			return table->elements[saturatedIndex];
		}
	}

	Uptr getTableNumElements(TableInstance* table)
	{
		return table->elements.size();
	}

	Iptr growTable(TableInstance* table,Uptr numNewElements)
	{
		const Uptr previousNumElements = table->elements.size();
		if(numNewElements > 0)
		{
			// If the number of elements to grow would cause the table's size to exceed its maximum, return -1.
			if(numNewElements > table->type.size.max || table->elements.size() > table->type.size.max - numNewElements) { return -1; }
			
			// Try to commit pages for the new elements, and return -1 if the commit fails.
			const Uptr previousNumPlatformPages = getNumPlatformPages(table->elements.size() * sizeof(TableInstance::FunctionElement));
			const Uptr newNumPlatformPages = getNumPlatformPages((table->elements.size()+numNewElements) * sizeof(TableInstance::FunctionElement));
			if(newNumPlatformPages != previousNumPlatformPages
			&& !Platform::commitVirtualPages(
				(U8*)table->baseAddress + (previousNumPlatformPages << Platform::getPageSizeLog2()),
				newNumPlatformPages - previousNumPlatformPages
				))
			{
				return -1;
			}

			// Also grow the table's elements array.
			{
				Platform::Lock elementsLock(table->elementsMutex);
				table->elements.insert(table->elements.end(),numNewElements,nullptr);
			}
		}
		return previousNumElements;
	}

	Iptr shrinkTable(TableInstance* table,Uptr numElementsToShrink)
	{
		const Uptr previousNumElements = table->elements.size();
		if(numElementsToShrink > 0)
		{
			// If the number of elements to shrink would cause the table's size to drop below its minimum, return -1.
			if(numElementsToShrink > table->elements.size()
			|| table->elements.size() - numElementsToShrink < table->type.size.min) { return -1; }

			// Shrink the table's elements array.
			{
				Platform::Lock elementsLock(table->elementsMutex);
				table->elements.resize(table->elements.size() - numElementsToShrink);
			}
			
			// Decommit the pages that were shrunk off the end of the table's indirect function call data.
			const Uptr previousNumPlatformPages = getNumPlatformPages(previousNumElements * sizeof(TableInstance::FunctionElement));
			const Uptr newNumPlatformPages = getNumPlatformPages(table->elements.size() * sizeof(TableInstance::FunctionElement));
			if(newNumPlatformPages != previousNumPlatformPages)
			{
				Platform::decommitVirtualPages(
					(U8*)table->baseAddress + (newNumPlatformPages << Platform::getPageSizeLog2()),
					(previousNumPlatformPages - newNumPlatformPages) << Platform::getPageSizeLog2()
					);
			}
		}
		return previousNumElements;
	}
}