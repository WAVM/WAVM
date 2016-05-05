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
			causeException(Exception::Cause::AccessViolation);
		}
		return (uint32)address;
	}

	DEFINE_INTRINSIC_FUNCTION0(wasm_intrinsics,current_memory,current_memory,I32)
	{
		return coerce32bitAddress(vmGrowMemory(0)) >> AST::numBytesPerPageLog2;
	}

	DEFINE_INTRINSIC_FUNCTION1(wasm_intrinsics,grow_memory,grow_memory,I32,I32,deltaPages)
	{
		return (uint32)vmGrowMemory((size_t)deltaPages << AST::numBytesPerPageLog2) >> AST::numBytesPerPageLog2;
	}

	DEFINE_INTRINSIC_FUNCTION1(spectest,print,print,Void,I32,a)
	{
		std::cout << a << " : I32" << std::endl;
	}

	DEFINE_INTRINSIC_FUNCTION1(spectest,print,print,Void,I64,a)
	{
		std::cout << a << " : I64" << std::endl;
	}

	DEFINE_INTRINSIC_FUNCTION1(spectest,print,print,Void,F32,a)
	{
		std::cout << a << " : F32" << std::endl;
	}
	
	DEFINE_INTRINSIC_FUNCTION1(spectest,print,print,Void,F64,a)
	{
		std::cout << a << " : F64" << std::endl;
	}

	void initWebAssemblyIntrinsics()
	{
		// Align the memory size to the page size.
		uint64_t pageSize = AST::numBytesPerPage;
		auto unalignedMemorySize = vmGrowMemory(0);
		vmGrowMemory(((unalignedMemorySize + pageSize - 1) & ~(pageSize - 1)) - unalignedMemorySize);
	}
}
