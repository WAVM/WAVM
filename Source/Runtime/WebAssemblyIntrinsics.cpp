#include "Core/Core.h"
#include "Core/Platform.h"
#include "Intrinsics.h"
#include "Runtime.h"

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
		if(vmSbrk(deltaBytes) == (uint32)-1)
		{
			throw;
		}
	}

	void initWebAssemblyIntrinsics()
	{
		// For the moment this function only serves the purpose of helping the compiler realize the module is used,
		// so it can't just throw away the code.
	}
}
