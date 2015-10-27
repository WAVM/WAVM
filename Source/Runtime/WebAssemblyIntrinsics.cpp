#include "Core/Core.h"
#include "Core/Platform.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"

#include <limits.h>
#include <iostream>

namespace Runtime
{
	static uint32 coerce32bitAddress(uintptr_t address)
	{
		if(address >= UINT_MAX)
		{
			throw;
		}
		return (uint32)address;
	}

	DEFINE_INTRINSIC_FUNCTION0(wasm_intrinsics,memory_size,I32)
	{
		return coerce32bitAddress(vmGrowMemory(0));
	}

	DEFINE_INTRINSIC_FUNCTION0(wasm_intrinsics,page_size,I32)
	{
		return 1<<Platform::getPreferredVirtualPageSizeLog2();
	}

	DEFINE_INTRINSIC_FUNCTION1(wasm_intrinsics,grow_memory,Void,I32,deltaBytes)
	{
		// Verify that deltaBytes is a multiple of the page size.
		auto pageSize = 1<<Platform::getPreferredVirtualPageSizeLog2();
		if(deltaBytes & (pageSize - 1))
		{
			throw;
		}

		vmGrowMemory((size_t)deltaBytes);
	}

	DEFINE_INTRINSIC_FUNCTION1(stdio,print,Void,I32,a)
	{
		std::cout << a << " : I32" << std::endl;
	}

	DEFINE_INTRINSIC_FUNCTION1(stdio,print,Void,I64,a)
	{
		std::cout << a << " : I64" << std::endl;
	}

	void initWebAssemblyIntrinsics()
	{
		// Align the memory size to the page size.
		auto pageSize = 1<<Platform::getPreferredVirtualPageSizeLog2();
		auto unalignedMemorySize = vmGrowMemory(0);
		vmGrowMemory(((unalignedMemorySize + pageSize - 1) & ~(pageSize - 1)) - unalignedMemorySize);
	}
}
