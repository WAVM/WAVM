#include "Core/Core.h"
#include "Core/Platform.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	DEFINE_INTRINSIC_FUNCTION0(wasm_intrinsics,memory_size,I32)
	{
		return vmSbrk(0);
	}

	DEFINE_INTRINSIC_FUNCTION0(wasm_intrinsics,page_size,I32)
	{
		return 1<<Platform::getPreferredVirtualPageSizeLog2();
	}

	DEFINE_INTRINSIC_FUNCTION1(wasm_intrinsics,resize_memory,Void,I32,deltaBytes)
	{
		// Verify that deltaBytes is a multiple of the page size.
		auto pageSize = 1<<Platform::getPreferredVirtualPageSizeLog2();
		if(deltaBytes & (pageSize - 1))
		{
			throw;
		}

		if(vmSbrk(deltaBytes) == (uint32)-1)
		{
			throw;
		}
	}

	void initWebAssemblyIntrinsics()
	{
		// Align the memory size to the page size.
		auto pageSize = 1<<Platform::getPreferredVirtualPageSizeLog2();
		auto unalignedMemorySize = vmSbrk(0);
		vmSbrk(((unalignedMemorySize + pageSize - 1) & ~(pageSize - 1)) - unalignedMemorySize);
	}
}
