#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"
#include "Logging/Logging.h"
#include "Runtime/Intrinsics.h"
#include "wavix.h"

DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__syscall_mmap",I32,__syscall_mmap,
	I32 address,
	I32 numBytes,
	I32 prot,
	I32 flags,
	I32 fd,
	I32 offset)
{
	const Uptr numPages = (Uptr(numBytes) + IR::numBytesPerPage - 1) >> IR::numBytesPerPageLog2;

	if(address != 0 || fd != -1)
	{
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	Iptr basePageIndex = growMemory(currentThread->process->memory,numPages);
	if(basePageIndex == -1)
	{
		return -1;
	}

	return coerce32bitAddress(basePageIndex << IR::numBytesPerPageLog2);
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_munmap",I32,__syscall_munmap,
	I32 address, I32 numBytes)
{
	traceSyscallf("munmap","(0x%08x,%u)",address,numBytes);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_mremap",I32,__syscall_mremap,
	I32 oldAddress,
	I32 oldNumBytes,
	I32 newNumBytes,
	I32 flags,
	I32 newAddress)
{
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_madvise",I32,__syscall_madvise,
	I32 address, I32 numBytes, I32 advice)
{
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_brk",I32,__syscall_brk,
	I32 address)
{
	traceSyscallf("brk","(0x%08x)",address);
	//throwException(Exception::calledUnimplementedIntrinsicType);
	return -1;
}

void staticInitializeMemory()
{
}