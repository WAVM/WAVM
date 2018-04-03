#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"
#include "Logging/Logging.h"
#include "Runtime/Intrinsics.h"
#include "wavix.h"

DEFINE_INTRINSIC_FUNCTION6(wavix,__syscall_mmap,__syscall_mmap,i32,
	i32,address,
	i32,numBytes,
	i32,prot,
	i32,flags,
	i32,fd,
	i32,offset)
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

DEFINE_INTRINSIC_FUNCTION2(wavix,__syscall_munmap,__syscall_munmap,i32,i32,address,i32,numBytes)
{
	traceSyscallf("munmap","(0x%08x,%u)",address,numBytes);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION5(wavix,__syscall_mremap,__syscall_mremap,i32,
	i32,oldAddress,
	i32,oldNumBytes,
	i32,newNumBytes,
	i32,flags,
	i32,newAddress)
{
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION3(wavix,__syscall_madvise,__syscall_madvise,i32,i32,address,i32,numBytes,i32,advice)
{
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION1(wavix,__syscall_brk,__syscall_brk,i32,i32,address)
{
	traceSyscallf("brk","(0x%08x)",address);
	//throwException(Exception::calledUnimplementedIntrinsicType);
	return -1;
}

void staticInitializeMemory()
{
}