#include "IR/IR.h"
#include "Inline/BasicTypes.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Runtime.h"
#include "errno.h"
#include "process.h"
#include "wavix.h"

using namespace WAVM::Runtime;
using namespace Wavix;

namespace Wavix {
	void staticInitializeMemory() {}
}

DEFINE_INTRINSIC_FUNCTION(wavix,
						  "__syscall_mmap",
						  I32,
						  __syscall_mmap,
						  I32 address,
						  I32 numBytes,
						  I32 prot,
						  I32 flags,
						  I32 fd,
						  I32 offset)
{
	traceSyscallf("mmap",
				  "(address=0x%08x, numBytes=%u, prot=0x%x, flags=0x%x, fd=%i, offset=%u)",
				  address,
				  numBytes,
				  prot,
				  flags,
				  fd,
				  offset);

	const Uptr numPages = (Uptr(numBytes) + IR::numBytesPerPage - 1) / IR::numBytesPerPage;

	if(address != 0 || fd != -1) { throwException(Exception::calledUnimplementedIntrinsicType); }

	MemoryInstance* memory = currentThread->process->memory;
	Iptr basePageIndex = growMemory(memory, numPages);
	if(basePageIndex == -1) { return -ErrNo::enomem; }

	return coerce32bitAddress(Uptr(basePageIndex) * IR::numBytesPerPage);
}

DEFINE_INTRINSIC_FUNCTION(wavix,
						  "__syscall_munmap",
						  I32,
						  __syscall_munmap,
						  I32 address,
						  I32 numBytes)
{
	traceSyscallf("munmap", "(address=0x%08x, numBytes=%u)", address, numBytes);

	MemoryInstance* memory = currentThread->process->memory;

	if(address & (IR::numBytesPerPage - 1) || numBytes == 0) { return -ErrNo::einval; }

	const Uptr basePageIndex = address / IR::numBytesPerPage;
	const Uptr numPages = (numBytes + IR::numBytesPerPage - 1) / IR::numBytesPerPage;

	if(basePageIndex + numPages > getMemoryMaxPages(memory)) { return -ErrNo::einval; }

	unmapMemoryPages(memory, basePageIndex, numPages);

	return 0;
}

DEFINE_INTRINSIC_FUNCTION(wavix,
						  "__syscall_mremap",
						  I32,
						  __syscall_mremap,
						  I32 oldAddress,
						  I32 oldNumBytes,
						  I32 newNumBytes,
						  I32 flags,
						  I32 newAddress)
{
	traceSyscallf(
		"mremap",
		"(oldAddress=0x%08x, oldNumBytes=%u, newNumBytes=%u, flags=0x%x, newAddress=0x%08x)",
		oldAddress,
		oldNumBytes,
		newNumBytes,
		flags,
		newAddress);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(wavix,
						  "__syscall_madvise",
						  I32,
						  __syscall_madvise,
						  I32 address,
						  I32 numBytes,
						  I32 advice)
{
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(wavix, "__syscall_brk", I32, __syscall_brk, I32 address)
{
	MemoryInstance* memory = currentThread->process->memory;

	traceSyscallf("brk", "(address=0x%08x)", address);
	// throwException(Exception::calledUnimplementedIntrinsicType);

	return coerce32bitAddress(getMemoryNumPages(memory));
}
