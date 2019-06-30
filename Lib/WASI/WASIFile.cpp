#include "./WASIPrivate.h"
#include "./WASITypes.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/VFS/VFS.h"

using namespace WAVM;
using namespace WAVM::WASI;
using namespace WAVM::Runtime;

namespace WAVM { namespace WASI {
	DEFINE_INTRINSIC_MODULE(wasiFile)
}}

static __wasi_errno_t asWASIErrNo(VFS::Result result)
{
	switch(result)
	{
	case VFS::Result::success: return __WASI_ESUCCESS;
	case VFS::Result::ioPending: WAVM_UNREACHABLE();
	case VFS::Result::ioDeviceError: return __WASI_EIO;
	case VFS::Result::interruptedBySignal: return __WASI_EINTR;
	case VFS::Result::interruptedByCancellation: return __WASI_EINTR;
	case VFS::Result::wouldBlock: return __WASI_EAGAIN;
	case VFS::Result::inaccessibleBuffer: return __WASI_EFAULT;
	case VFS::Result::invalidOffset: return __WASI_EINVAL;
	case VFS::Result::notSeekable: return __WASI_ESPIPE;
	case VFS::Result::notPermitted: return __WASI_EPERM;
	case VFS::Result::notAccessible: return __WASI_EACCES;
	case VFS::Result::notSynchronizable: return __WASI_EINVAL;
	case VFS::Result::tooManyBufferBytes: return __WASI_EINVAL;
	case VFS::Result::notEnoughBufferBytes: return __WASI_EINVAL;
	case VFS::Result::tooManyBuffers: return __WASI_EINVAL;
	case VFS::Result::notEnoughBits: return __WASI_EOVERFLOW;
	case VFS::Result::exceededFileSizeLimit: return __WASI_EFBIG;
	case VFS::Result::outOfSystemFDs: return __WASI_ENFILE;
	case VFS::Result::outOfProcessFDs: return __WASI_EMFILE;
	case VFS::Result::outOfMemory: return __WASI_ENOMEM;
	case VFS::Result::outOfQuota: return __WASI_EDQUOT;
	case VFS::Result::outOfFreeSpace: return __WASI_ENOSPC;
	case VFS::Result::outOfLinksToParentDir: return __WASI_EMLINK;
	case VFS::Result::invalidNameCharacter: return __WASI_EACCES;
	case VFS::Result::nameTooLong: return __WASI_ENAMETOOLONG;
	case VFS::Result::tooManyLinksInPath: return __WASI_ELOOP;
	case VFS::Result::alreadyExists: return __WASI_EEXIST;
	case VFS::Result::doesNotExist: return __WASI_ENOENT;
	case VFS::Result::isDirectory: return __WASI_EISDIR;
	case VFS::Result::isNotDirectory: return __WASI_ENOTDIR;
	case VFS::Result::isNotEmpty: return __WASI_ENOTEMPTY;
	case VFS::Result::brokenPipe: return __WASI_EPIPE;
	case VFS::Result::missingDevice: return __WASI_ENXIO;
	case VFS::Result::busy: return __WASI_EBUSY;

	default: WAVM_UNREACHABLE();
	};
}

static WASI::FD* getFD(Process* process, __wasi_fd_t fd)
{
	if(fd < process->fds.getMinIndex() || fd > process->fds.getMaxIndex()) { return nullptr; }
	return process->fds.get(fd);
}

static bool checkFDRights(WASI::FD* wasiFD,
						  __wasi_rights_t requiredRights,
						  __wasi_rights_t requiredInheritingRights = 0)
{
	return (wasiFD->rights & requiredRights) == requiredRights
		   && (wasiFD->inheritingRights & requiredInheritingRights) == requiredInheritingRights;
}

static __wasi_filetype_t asWASIFileType(VFS::FileType type)
{
	switch(type)
	{
	case VFS::FileType::unknown: return __WASI_FILETYPE_UNKNOWN;
	case VFS::FileType::blockDevice: return __WASI_FILETYPE_BLOCK_DEVICE;
	case VFS::FileType::characterDevice: return __WASI_FILETYPE_CHARACTER_DEVICE;
	case VFS::FileType::directory: return __WASI_FILETYPE_DIRECTORY;
	case VFS::FileType::file: return __WASI_FILETYPE_REGULAR_FILE;
	case VFS::FileType::datagramSocket: return __WASI_FILETYPE_SOCKET_DGRAM;
	case VFS::FileType::streamSocket: return __WASI_FILETYPE_SOCKET_STREAM;
	case VFS::FileType::symbolicLink: return __WASI_FILETYPE_SYMBOLIC_LINK;
	case VFS::FileType::pipe: WAVM_UNREACHABLE();

	default: WAVM_UNREACHABLE();
	};
}

static bool readUserString(Memory* memory,
						   WASIAddress stringAddress,
						   WASIAddress numStringBytes,
						   std::string& outString)
{
	outString = "";

	bool succeeded = true;
	catchRuntimeExceptions(
		[&] {
			char* stringBytes = memoryArrayPtr<char>(memory, stringAddress, numStringBytes);
			for(Uptr index = 0; index < numStringBytes; ++index)
			{ outString += stringBytes[index]; }
		},
		[&succeeded](Exception* exception) {
			errorUnless(getExceptionType(exception) == ExceptionTypes::outOfBoundsMemoryAccess);
			Log::printf(Log::debug,
						"Caught runtime exception while reading string at address 0x%" PRIx64,
						getExceptionArgument(exception, 1).i64);
			destroyException(exception);

			succeeded = false;
		});

	return succeeded;
}

static bool getCanonicalPath(const std::string& basePath,
							 const std::string& relativePath,
							 std::string& outAbsolutePath)
{
	outAbsolutePath = basePath;
	if(outAbsolutePath.back() == '/') { outAbsolutePath.pop_back(); }

	std::vector<std::string> relativePathComponents;

	Uptr componentStart = 0;
	while(componentStart < relativePath.size())
	{
		Uptr nextPathSeparator = relativePath.find_first_of("\\/", componentStart, 2);

		if(nextPathSeparator == std::string::npos) { nextPathSeparator = relativePath.size(); }

		if(nextPathSeparator != componentStart)
		{
			std::string component
				= relativePath.substr(componentStart, nextPathSeparator - componentStart);

			if(component == "..")
			{
				if(!relativePathComponents.size()) { return false; }
				else
				{
					relativePathComponents.pop_back();
				}
			}
			else if(component != ".")
			{
				relativePathComponents.push_back(component);
			}

			componentStart = nextPathSeparator + 1;
		}
	};

	for(const std::string& component : relativePathComponents)
	{
		outAbsolutePath += '/';
		outAbsolutePath += component;
	}

	return true;
}

static VFS::FDFlags translateWASIFDFlags(__wasi_fdflags_t fdFlags,
										 __wasi_rights_t& outRequiredRights)
{
	VFS::FDFlags result;
	if(fdFlags & __WASI_FDFLAG_DSYNC)
	{
		result.implicitSync = (fdFlags & __WASI_FDFLAG_RSYNC)
								  ? VFS::FDImplicitSync::syncContentsAfterWriteAndBeforeRead
								  : VFS::FDImplicitSync::syncContentsAfterWrite;
		outRequiredRights |= __WASI_RIGHT_FD_DATASYNC;
	}
	if(fdFlags & __WASI_FDFLAG_SYNC)
	{
		result.implicitSync
			= (fdFlags & __WASI_FDFLAG_RSYNC)
				  ? VFS::FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead
				  : VFS::FDImplicitSync::syncContentsAndMetadataAfterWrite;
		outRequiredRights |= __WASI_RIGHT_FD_SYNC;
	}
	if(fdFlags & __WASI_FDFLAG_NONBLOCK) { result.nonBlocking = true; }
	if(fdFlags & __WASI_FDFLAG_APPEND) { result.append = true; }
	return result;
}

VFS::Result WASI::FD::close() const
{
	VFS::Result result = vfd->close();
	if(result == VFS::Result::success && dirEntStream) { dirEntStream->close(); }
	return result;
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_prestat_get",
						  __wasi_errno_return_t,
						  wasi_fd_prestat_get,
						  __wasi_fd_t fd,
						  WASIAddress prestatAddress)
{
	TRACE_SYSCALL("fd_prestat_get", "(%u, " WASIADDRESS_FORMAT ")", fd, prestatAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD || !wasiFD->isPreopened) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }

	if(wasiFD->originalPath.size() > UINT32_MAX) { return TRACE_SYSCALL_RETURN(__WASI_EOVERFLOW); }

	__wasi_prestat_t& prestat = memoryRef<__wasi_prestat_t>(process->memory, prestatAddress);
	prestat.pr_type = wasiFD->preopenedType;
	wavmAssert(wasiFD->preopenedType == __WASI_PREOPENTYPE_DIR);
	prestat.u.dir.pr_name_len = U32(wasiFD->originalPath.size());

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_prestat_dir_name",
						  __wasi_errno_return_t,
						  wasi_fd_prestat_dir_name,
						  __wasi_fd_t fd,
						  WASIAddress bufferAddress,
						  WASIAddress bufferLength)
{
	TRACE_SYSCALL(
		"fd_prestat_dir_name", "(%u, " WASIADDRESS_FORMAT ", %u)", fd, bufferAddress, bufferLength);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD || !wasiFD->isPreopened) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }

	if(bufferLength != wasiFD->originalPath.size()) { return TRACE_SYSCALL_RETURN(__WASI_EINVAL); }

	char* buffer = memoryArrayPtr<char>(process->memory, bufferAddress, bufferLength);
	memcpy(buffer, wasiFD->originalPath.c_str(), bufferLength);

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_close",
						  __wasi_errno_return_t,
						  wasi_fd_close,
						  __wasi_fd_t fd)
{
	TRACE_SYSCALL("fd_close", "(%u)", fd);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD || wasiFD->isPreopened) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }

	const VFS::Result result = wasiFD->close();

	if(result == VFS::Result::success)
	{
		// If the close succeeded, remove the fd from the fds map.
		process->fds.removeOrFail(fd);
	}

	return TRACE_SYSCALL_RETURN(asWASIErrNo(result));
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_datasync",
						  __wasi_errno_return_t,
						  wasi_fd_datasync,
						  __wasi_fd_t fd)
{
	TRACE_SYSCALL("fd_datasync", "(%u)", fd);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_DATASYNC))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	return TRACE_SYSCALL_RETURN(asWASIErrNo(wasiFD->vfd->sync(VFS::SyncType::contents)));
}

static __wasi_errno_t readImpl(Process* process,
							   __wasi_fd_t fd,
							   WASIAddress iovsAddress,
							   I32 numIOVs,
							   const __wasi_filesize_t* offset,
							   Uptr& outNumBytesRead)
{
	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return __WASI_EBADF; }
	if(!checkFDRights(wasiFD, (offset ? __WASI_RIGHT_FD_SEEK : 0) | __WASI_RIGHT_FD_READ))
	{ return __WASI_ENOTCAPABLE; }

	if(numIOVs < 0 || numIOVs > __WASI_IOV_MAX) { return __WASI_EINVAL; }

	// Allocate memory for the VFS::IOReadBuffers.
	VFS::IOReadBuffer* vfsReadBuffers
		= (VFS::IOReadBuffer*)malloc(numIOVs * sizeof(VFS::IOReadBuffer));

	// Catch any out-of-bounds memory access exceptions that are thrown.
	__wasi_errno_t result = __WASI_ESUCCESS;
	Runtime::catchRuntimeExceptions(
		[&] {
			// Translate the IOVs to VFS::IOReadBuffers.
			const __wasi_iovec_t* iovs
				= memoryArrayPtr<__wasi_iovec_t>(process->memory, iovsAddress, numIOVs);
			U64 numBufferBytes = 0;
			for(I32 iovIndex = 0; iovIndex < numIOVs; ++iovIndex)
			{
				__wasi_iovec_t iov = iovs[iovIndex];
				vfsReadBuffers[iovIndex].data
					= memoryArrayPtr<U8>(process->memory, iov.buf, iov.buf_len);
				vfsReadBuffers[iovIndex].numBytes = iov.buf_len;
				numBufferBytes += iov.buf_len;
			}
			if(numBufferBytes > WASIADDRESS_MAX) { result = __WASI_EOVERFLOW; }
			else
			{
				// Do the read.
				result = asWASIErrNo(
					offset ? wasiFD->vfd->preadv(vfsReadBuffers, numIOVs, *offset, &outNumBytesRead)
						   : wasiFD->vfd->readv(vfsReadBuffers, numIOVs, &outNumBytesRead));
			}
		},
		[&](Exception* exception) {
			// If we catch an out-of-bounds memory exception, return EFAULT.
			errorUnless(getExceptionType(exception) == ExceptionTypes::outOfBoundsMemoryAccess);
			Log::printf(Log::debug,
						"Caught runtime exception while reading memory at address 0x%" PRIx64,
						getExceptionArgument(exception, 1).i64);
			destroyException(exception);
			result = __WASI_EFAULT;
		});

	// Free the VFS read buffers.
	free(vfsReadBuffers);

	return result;
}

static __wasi_errno_t writeImpl(Process* process,
								__wasi_fd_t fd,
								WASIAddress iovsAddress,
								I32 numIOVs,
								const __wasi_filesize_t* offset,
								Uptr& outNumBytesWritten)
{
	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return __WASI_EBADF; }
	if(!checkFDRights(wasiFD, (offset ? __WASI_RIGHT_FD_SEEK : 0) | __WASI_RIGHT_FD_WRITE))
	{ return __WASI_ENOTCAPABLE; }

	if(numIOVs < 0 || numIOVs > __WASI_IOV_MAX) { return __WASI_EINVAL; }

	// Allocate memory for the VFS::IOWriteBuffers.
	VFS::IOWriteBuffer* vfsWriteBuffers
		= (VFS::IOWriteBuffer*)malloc(numIOVs * sizeof(VFS::IOWriteBuffer));

	// Catch any out-of-bounds memory access exceptions that are thrown.
	__wasi_errno_t result = __WASI_ESUCCESS;
	Runtime::catchRuntimeExceptions(
		[&] {
			// Translate the IOVs to VFS::IOWriteBuffers
			const __wasi_ciovec_t* iovs
				= memoryArrayPtr<__wasi_ciovec_t>(process->memory, iovsAddress, numIOVs);
			U64 numBufferBytes = 0;
			for(I32 iovIndex = 0; iovIndex < numIOVs; ++iovIndex)
			{
				__wasi_ciovec_t iov = iovs[iovIndex];
				vfsWriteBuffers[iovIndex].data
					= memoryArrayPtr<const U8>(process->memory, iov.buf, iov.buf_len);
				vfsWriteBuffers[iovIndex].numBytes = iov.buf_len;
				numBufferBytes += iov.buf_len;
			}
			if(numBufferBytes > WASIADDRESS_MAX) { result = __WASI_EOVERFLOW; }
			else
			{
				// Do the writes.
				result = asWASIErrNo(
					offset ? wasiFD->vfd->pwritev(
								 vfsWriteBuffers, numIOVs, *offset, &outNumBytesWritten)
						   : wasiFD->vfd->writev(vfsWriteBuffers, numIOVs, &outNumBytesWritten));
			}
		},
		[&](Exception* exception) {
			// If we catch an out-of-bounds memory exception, return EFAULT.
			errorUnless(getExceptionType(exception) == ExceptionTypes::outOfBoundsMemoryAccess);
			Log::printf(Log::debug,
						"Caught runtime exception while reading memory at address 0x%" PRIx64,
						getExceptionArgument(exception, 1).i64);
			destroyException(exception);
			result = __WASI_EFAULT;
		});

	// Free the VFS write buffers.
	free(vfsWriteBuffers);

	return result;
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_pread",
						  __wasi_errno_return_t,
						  wasi_fd_pread,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  __wasi_filesize_t offset,
						  WASIAddress numBytesReadAddress)
{
	TRACE_SYSCALL("fd_pread",
				  "(%u, " WASIADDRESS_FORMAT ", %u, %" PRIu64 ", " WASIADDRESS_FORMAT ")",
				  fd,
				  iovsAddress,
				  numIOVs,
				  offset,
				  numBytesReadAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	Uptr numBytesRead = 0;
	const __wasi_errno_t result
		= readImpl(process, fd, iovsAddress, numIOVs, &offset, numBytesRead);

	// Write the number of bytes read to memory.
	wavmAssert(numBytesRead <= WASIADDRESS_MAX);
	memoryRef<WASIAddress>(process->memory, numBytesReadAddress) = WASIAddress(numBytesRead);

	return TRACE_SYSCALL_RETURN(result, " (numBytesRead=%" PRIuPTR ")", numBytesRead);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_pwrite",
						  __wasi_errno_return_t,
						  wasi_fd_pwrite,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  __wasi_filesize_t offset,
						  WASIAddress numBytesWrittenAddress)
{
	TRACE_SYSCALL("fd_pwrite",
				  "(%u, " WASIADDRESS_FORMAT ", %u, %" PRIu64 ", " WASIADDRESS_FORMAT ")",
				  fd,
				  iovsAddress,
				  numIOVs,
				  offset,
				  numBytesWrittenAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	Uptr numBytesWritten = 0;
	const __wasi_errno_t result
		= writeImpl(process, fd, iovsAddress, numIOVs, &offset, numBytesWritten);

	// Write the number of bytes written to memory.
	wavmAssert(numBytesWritten <= WASIADDRESS_MAX);
	memoryRef<WASIAddress>(process->memory, numBytesWrittenAddress) = WASIAddress(numBytesWritten);

	return TRACE_SYSCALL_RETURN(result, " (numBytesWritten=%" PRIuPTR ")", numBytesWritten);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_read",
						  __wasi_errno_return_t,
						  wasi_fd_read,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  I32 numIOVs,
						  WASIAddress numBytesReadAddress)
{
	TRACE_SYSCALL("fd_read",
				  "(%u, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT ")",
				  fd,
				  iovsAddress,
				  numIOVs,
				  numBytesReadAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	Uptr numBytesRead = 0;
	const __wasi_errno_t result
		= readImpl(process, fd, iovsAddress, numIOVs, nullptr, numBytesRead);

	// Write the number of bytes read to memory.
	wavmAssert(numBytesRead <= WASIADDRESS_MAX);
	memoryRef<WASIAddress>(process->memory, numBytesReadAddress) = WASIAddress(numBytesRead);

	return TRACE_SYSCALL_RETURN(result, " (numBytesRead=%" PRIuPTR ")", numBytesRead);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_write",
						  __wasi_errno_return_t,
						  wasi_fd_write,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  I32 numIOVs,
						  WASIAddress numBytesWrittenAddress)
{
	TRACE_SYSCALL("fd_write",
				  "(%u, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT ")",
				  fd,
				  iovsAddress,
				  numIOVs,
				  numBytesWrittenAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	Uptr numBytesWritten = 0;
	const __wasi_errno_t result
		= writeImpl(process, fd, iovsAddress, numIOVs, nullptr, numBytesWritten);

	// Write the number of bytes written to memory.
	wavmAssert(numBytesWritten <= WASIADDRESS_MAX);
	memoryRef<WASIAddress>(process->memory, numBytesWrittenAddress) = WASIAddress(numBytesWritten);

	return TRACE_SYSCALL_RETURN(result, " (numBytesWritten=%" PRIuPTR ")", numBytesWritten);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_renumber",
						  __wasi_errno_return_t,
						  wasi_fd_renumber,
						  __wasi_fd_t fromFD,
						  __wasi_fd_t toFD)
{
	TRACE_SYSCALL("fd_renumber", "(%u, %u)", fromFD, toFD);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFromFD = getFD(process, fromFD);
	WASI::FD* wasiToFD = getFD(process, toFD);
	if(!wasiFromFD || !wasiToFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(wasiFromFD->isPreopened || wasiToFD->isPreopened)
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTSUP); }

	VFS::Result result = wasiToFD->close();
	if(result != VFS::Result::success) { return TRACE_SYSCALL_RETURN(asWASIErrNo(result)); }

	process->fds.removeOrFail(toFD);
	process->fds.insertOrFail(toFD, *wasiFromFD);
	process->fds.removeOrFail(fromFD);

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_seek",
						  __wasi_errno_return_t,
						  wasi_fd_seek,
						  __wasi_fd_t fd,
						  __wasi_filedelta_t offset,
						  U32 whence,
						  WASIAddress newOffsetAddress)
{
	TRACE_SYSCALL("fd_seek",
				  "(%u, %" PRIi64 ", %u, " WASIADDRESS_FORMAT ")",
				  fd,
				  offset,
				  whence,
				  newOffsetAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_SEEK))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	VFS::SeekOrigin origin;
	switch(whence)
	{
	case __WASI_WHENCE_CUR: origin = VFS::SeekOrigin::cur; break;
	case __WASI_WHENCE_END: origin = VFS::SeekOrigin::end; break;
	case __WASI_WHENCE_SET: origin = VFS::SeekOrigin::begin; break;
	default: return TRACE_SYSCALL_RETURN(__WASI_EINVAL);
	};

	U64 newOffset;
	const VFS::Result result = wasiFD->vfd->seek(offset, origin, &newOffset);
	if(result != VFS::Result::success) { return TRACE_SYSCALL_RETURN(asWASIErrNo(result)); }

	memoryRef<__wasi_filesize_t>(process->memory, newOffsetAddress) = newOffset;
	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_tell",
						  __wasi_errno_return_t,
						  wasi_fd_tell,
						  __wasi_fd_t fd,
						  WASIAddress offsetAddress)
{
	TRACE_SYSCALL("fd_tell", "(%u, " WASIADDRESS_FORMAT ")", fd, offsetAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_TELL))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	U64 currentOffset;
	const VFS::Result result = wasiFD->vfd->seek(0, VFS::SeekOrigin::cur, &currentOffset);
	if(result != VFS::Result::success) { return TRACE_SYSCALL_RETURN(asWASIErrNo(result)); }

	memoryRef<__wasi_filesize_t>(process->memory, offsetAddress) = currentOffset;
	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_fdstat_get",
						  __wasi_errno_return_t,
						  wasi_fd_fdstat_get,
						  __wasi_fd_t fd,
						  WASIAddress fdstatAddress)
{
	TRACE_SYSCALL("fd_fdstat_get", "(%u, " WASIADDRESS_FORMAT ")", fd, fdstatAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }

	VFS::FDInfo fdInfo;
	const VFS::Result result = wasiFD->vfd->getFDInfo(fdInfo);
	if(result != VFS::Result::success) { return TRACE_SYSCALL_RETURN(asWASIErrNo(result)); }

	__wasi_fdstat_t& fdstat = memoryRef<__wasi_fdstat_t>(process->memory, fdstatAddress);
	fdstat.fs_filetype = asWASIFileType(fdInfo.type);
	fdstat.fs_flags = 0;

	if(fdInfo.flags.append) { fdstat.fs_flags |= __WASI_FDFLAG_APPEND; }
	if(fdInfo.flags.nonBlocking) { fdstat.fs_flags |= __WASI_FDFLAG_NONBLOCK; }
	switch(fdInfo.flags.implicitSync)
	{
	case VFS::FDImplicitSync::none: break;
	case VFS::FDImplicitSync::syncContentsAfterWrite: fdstat.fs_flags |= __WASI_FDFLAG_DSYNC; break;
	case VFS::FDImplicitSync::syncContentsAndMetadataAfterWrite:
		fdstat.fs_flags |= __WASI_FDFLAG_SYNC;
		break;
	case VFS::FDImplicitSync::syncContentsAfterWriteAndBeforeRead:
		fdstat.fs_flags |= __WASI_FDFLAG_DSYNC | __WASI_FDFLAG_RSYNC;
		break;
	case VFS::FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead:
		fdstat.fs_flags |= __WASI_FDFLAG_SYNC | __WASI_FDFLAG_RSYNC;
		break;

	default: WAVM_UNREACHABLE();
	}

	fdstat.fs_rights_base = wasiFD->rights;
	fdstat.fs_rights_inheriting = wasiFD->inheritingRights;

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_fdstat_set_flags",
						  __wasi_errno_return_t,
						  wasi_fd_fdstat_set_flags,
						  __wasi_fd_t fd,
						  __wasi_fdflags_t flags)
{
	TRACE_SYSCALL("fd_fdstat_set_flags", "(%u, 0x%04x)", fd, flags);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	__wasi_rights_t requiredRights;
	VFS::FDFlags vfsFDFlags = translateWASIFDFlags(flags, requiredRights);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_FDSTAT_SET_FLAGS | requiredRights))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	const VFS::Result result = wasiFD->vfd->setFDFlags(vfsFDFlags);
	return TRACE_SYSCALL_RETURN(asWASIErrNo(result));
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_fdstat_set_rights",
						  __wasi_errno_return_t,
						  wasi_fd_fdstat_set_rights,
						  __wasi_fd_t fd,
						  __wasi_rights_t rights,
						  __wasi_rights_t inheritingRights)
{
	TRACE_SYSCALL("fd_fdstat_set_rights",
				  "(%u, 0x%" PRIx64 ", 0x %" PRIx64 ") ",
				  fd,
				  rights,
				  inheritingRights);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, rights, inheritingRights))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	// Narrow the FD's rights.
	wasiFD->rights = rights;
	wasiFD->inheritingRights = inheritingRights;

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile, "fd_sync", __wasi_errno_return_t, wasi_fd_sync, __wasi_fd_t fd)
{
	TRACE_SYSCALL("fd_sync", "(%u)", fd);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_SYNC))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	wasiFD->vfd->sync(VFS::SyncType::contentsAndMetadata);

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_advise",
						  __wasi_errno_return_t,
						  wasi_fd_advise,
						  __wasi_fd_t fd,
						  __wasi_filesize_t offset,
						  __wasi_filesize_t numBytes,
						  __wasi_advice_t advice)
{
	TRACE_SYSCALL(
		"fd_advise", "(%u, %" PRIu64 ", %" PRIu64 ", 0x%02x)", fd, offset, numBytes, advice);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_ADVISE))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	switch(advice)
	{
	case __WASI_ADVICE_DONTNEED:
	case __WASI_ADVICE_NOREUSE:
	case __WASI_ADVICE_NORMAL:
	case __WASI_ADVICE_RANDOM:
	case __WASI_ADVICE_SEQUENTIAL:
	case __WASI_ADVICE_WILLNEED:
		// It's safe to ignore the advice, so just return success for now.
		// TODO: do something with the advice!
		return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
	default: return TRACE_SYSCALL_RETURN(__WASI_EINVAL);
	}
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_allocate",
						  __wasi_errno_return_t,
						  wasi_fd_allocate,
						  __wasi_fd_t fd,
						  __wasi_filesize_t offset,
						  __wasi_filesize_t numBytes)
{
	UNIMPLEMENTED_SYSCALL("fd_allocate", "(%u, %" PRIu64 ", %" PRIu64 ")", fd, offset, numBytes);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_link",
						  __wasi_errno_return_t,
						  wasi_path_link,
						  __wasi_fd_t oldFD,
						  __wasi_lookupflags_t oldFlags,
						  WASIAddress oldPathAddress,
						  WASIAddress numOldPathBytes,
						  __wasi_fd_t newFD,
						  WASIAddress newPathAddress,
						  WASIAddress numNewPathBytes)
{
	UNIMPLEMENTED_SYSCALL("path_link",
						  "(%u, 0x%08x, " WASIADDRESS_FORMAT ", %u, %u, " WASIADDRESS_FORMAT
						  ", %u)",
						  oldFD,
						  oldFlags,
						  oldPathAddress,
						  numOldPathBytes,
						  newFD,
						  newPathAddress,
						  numNewPathBytes);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_open",
						  __wasi_errno_return_t,
						  wasi_path_open,
						  __wasi_fd_t dirFD,
						  __wasi_lookupflags_t dirFlags,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes,
						  __wasi_oflags_t openFlags,
						  __wasi_rights_t requestedRights,
						  __wasi_rights_t requestedInheritingRights,
						  __wasi_fdflags_t fdFlags,
						  WASIAddress fdAddress)
{
	TRACE_SYSCALL("path_open",
				  "(%u, 0x%08x, " WASIADDRESS_FORMAT ", %u, 0x%04x, 0x%" PRIx64 ", 0x%" PRIx64
				  ", 0x%04x, " WASIADDRESS_FORMAT ")",
				  dirFD,
				  dirFlags,
				  pathAddress,
				  numPathBytes,
				  openFlags,
				  requestedRights,
				  requestedInheritingRights,
				  fdFlags,
				  fdAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	__wasi_rights_t requiredDirRights = __WASI_RIGHT_PATH_OPEN;
	__wasi_rights_t requiredDirInheritingRights = requestedRights | requestedInheritingRights;

	const bool read = requestedRights & (__WASI_RIGHT_FD_READ | __WASI_RIGHT_FD_READDIR);
	const bool write = requestedRights
					   & (__WASI_RIGHT_FD_DATASYNC | __WASI_RIGHT_FD_WRITE
						  | __WASI_RIGHT_FD_ALLOCATE | __WASI_RIGHT_FD_FILESTAT_SET_SIZE);
	const VFS::FileAccessMode accessMode
		= read && write ? VFS::FileAccessMode::readWrite
						: read ? VFS::FileAccessMode::readOnly
							   : write ? VFS::FileAccessMode::writeOnly : VFS::FileAccessMode::none;

	VFS::FileCreateMode createMode = VFS::FileCreateMode::openExisting;
	switch(openFlags & (__WASI_O_CREAT | __WASI_O_EXCL | __WASI_O_TRUNC))
	{
	case __WASI_O_CREAT | __WASI_O_EXCL: createMode = VFS::FileCreateMode::createNew; break;
	case __WASI_O_CREAT | __WASI_O_TRUNC: createMode = VFS::FileCreateMode::createAlways; break;
	case __WASI_O_CREAT: createMode = VFS::FileCreateMode::openAlways; break;
	case __WASI_O_TRUNC: createMode = VFS::FileCreateMode::truncateExisting; break;
	case 0:
		createMode = VFS::FileCreateMode::openExisting;
		break;

		// Undefined oflag combinations
	case __WASI_O_CREAT | __WASI_O_EXCL | __WASI_O_TRUNC:
	case __WASI_O_EXCL | __WASI_O_TRUNC:
	case __WASI_O_EXCL:
	default: return TRACE_SYSCALL_RETURN(__WASI_EINVAL);
	};

	if(openFlags & __WASI_O_CREAT) { requiredDirRights |= __WASI_RIGHT_PATH_CREATE_FILE; }
	if(openFlags & __WASI_O_TRUNC) { requiredDirRights |= __WASI_RIGHT_PATH_FILESTAT_SET_SIZE; }

	VFS::FDFlags vfsFDFlags = translateWASIFDFlags(fdFlags, requiredDirInheritingRights);
	if(write && !(fdFlags & __WASI_FDFLAG_APPEND) && !(openFlags & __WASI_O_TRUNC))
	{ requiredDirInheritingRights |= __WASI_RIGHT_FD_SEEK; }

	WASI::FD* wasiDirFD = getFD(process, dirFD);
	if(!wasiDirFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiDirFD, requiredDirRights, requiredDirInheritingRights))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	std::string relativePath;
	if(!readUserString(process->memory, pathAddress, numPathBytes, relativePath))
	{ return TRACE_SYSCALL_RETURN(__WASI_EFAULT); }

	std::string canonicalPath;
	if(!getCanonicalPath(wasiDirFD->originalPath, relativePath, canonicalPath))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	if(!process->fileSystem) { return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	VFS::FD* openedVFD = nullptr;
	const VFS::Result result
		= process->fileSystem->open(canonicalPath, accessMode, createMode, openedVFD, vfsFDFlags);
	if(result != VFS::Result::success) { return TRACE_SYSCALL_RETURN(asWASIErrNo(result)); }

	__wasi_fd_t fd = process->fds.add(
		UINT32_MAX,
		FD(openedVFD, requestedRights, requestedInheritingRights, std::move(canonicalPath)));
	if(fd == UINT32_MAX)
	{
		errorUnless(openedVFD->close() == VFS::Result::success);
		return TRACE_SYSCALL_RETURN(__WASI_EMFILE);
	}

	memoryRef<__wasi_fd_t>(process->memory, fdAddress) = fd;

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS, " (%u)", fd);
}

static Uptr truncatingMemcpy(void* dest, const void* source, Uptr numSourceBytes, Uptr numDestBytes)
{
	Uptr numBytes = numSourceBytes;
	if(numBytes > numDestBytes) { numBytes = numDestBytes; }
	if(numBytes > 0) { memcpy(dest, source, numBytes); }
	return numBytes;
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_readdir",
						  __wasi_errno_return_t,
						  wasi_fd_readdir,
						  __wasi_fd_t dirFD,
						  WASIAddress bufferAddress,
						  WASIAddress numBufferBytes,
						  __wasi_dircookie_t firstCookie,
						  WASIAddress outNumBufferBytesUsedAddress)
{
	TRACE_SYSCALL("fd_readdir",
				  "(%u, " WASIADDRESS_FORMAT ", %u, 0x%" PRIx64 ", " WASIADDRESS_FORMAT ")",
				  dirFD,
				  bufferAddress,
				  numBufferBytes,
				  firstCookie,
				  outNumBufferBytesUsedAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiDirFD = getFD(process, dirFD);
	if(!wasiDirFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiDirFD, __WASI_RIGHT_FD_READDIR, 0))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	// If this is the first time readdir was called, open a DirEntStream for the FD.
	if(!wasiDirFD->dirEntStream)
	{
		if(firstCookie != __WASI_DIRCOOKIE_START) { return TRACE_SYSCALL_RETURN(__WASI_EINVAL); }

		const VFS::Result result = wasiDirFD->vfd->openDir(wasiDirFD->dirEntStream);
		if(result != VFS::Result::success) { return TRACE_SYSCALL_RETURN(asWASIErrNo(result)); }
	}
	else if(wasiDirFD->dirEntStream->tell() != firstCookie)
	{
		if(!wasiDirFD->dirEntStream->seek(firstCookie))
		{ return TRACE_SYSCALL_RETURN(__WASI_EINVAL); }
	}

	U8* buffer = memoryArrayPtr<U8>(process->memory, bufferAddress, numBufferBytes);
	Uptr numBufferBytesUsed = 0;

	while(numBufferBytesUsed < numBufferBytes)
	{
		VFS::DirEnt dirEnt;
		if(!wasiDirFD->dirEntStream->getNext(dirEnt)) { break; }

		errorUnless(dirEnt.name.size() <= UINT32_MAX);

		__wasi_dirent_t wasiDirEnt;
		wasiDirEnt.d_next = wasiDirFD->dirEntStream->tell();
		wasiDirEnt.d_ino = dirEnt.fileNumber;
		wasiDirEnt.d_namlen = U32(dirEnt.name.size());
		wasiDirEnt.d_type = asWASIFileType(dirEnt.type);

		numBufferBytesUsed += truncatingMemcpy(buffer + numBufferBytesUsed,
											   &wasiDirEnt,
											   sizeof(wasiDirEnt),
											   numBufferBytes - numBufferBytesUsed);

		numBufferBytesUsed += truncatingMemcpy(buffer + numBufferBytesUsed,
											   dirEnt.name.c_str(),
											   dirEnt.name.size(),
											   numBufferBytes - numBufferBytesUsed);
	};

	wavmAssert(numBufferBytesUsed <= numBufferBytes);
	memoryRef<WASIAddress>(process->memory, outNumBufferBytesUsedAddress)
		= WASIAddress(numBufferBytesUsed);

	return TRACE_SYSCALL_RETURN(
		__WASI_ESUCCESS, "(numBufferBytesUsed=%" PRIuPTR ")", numBufferBytesUsed);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_readlink",
						  __wasi_errno_return_t,
						  wasi_path_readlink,
						  __wasi_fd_t fd,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes,
						  WASIAddress bufferAddress,
						  WASIAddress numBufferBytes,
						  WASIAddress outNumBufferBytesUsedAddress)
{
	UNIMPLEMENTED_SYSCALL("path_readlink",
						  "(%u, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT
						  ", %u, " WASIADDRESS_FORMAT ")",
						  fd,
						  pathAddress,
						  numPathBytes,
						  bufferAddress,
						  numBufferBytes,
						  outNumBufferBytesUsedAddress);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_rename",
						  __wasi_errno_return_t,
						  wasi_path_rename,
						  __wasi_fd_t oldFD,
						  WASIAddress oldPathAddress,
						  WASIAddress numOldPathBytes,
						  __wasi_fd_t newFD,
						  WASIAddress newPathAddress,
						  WASIAddress numNewPathBytes)
{
	UNIMPLEMENTED_SYSCALL("path_rename",
						  "(%u, " WASIADDRESS_FORMAT ", %u, %u, " WASIADDRESS_FORMAT ", %u)",
						  oldFD,
						  oldPathAddress,
						  numOldPathBytes,
						  newFD,
						  newPathAddress,
						  numNewPathBytes);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_filestat_get",
						  __wasi_errno_return_t,
						  wasi_fd_filestat_get,
						  __wasi_fd_t fd,
						  WASIAddress filestatAddress)
{
	TRACE_SYSCALL("fd_filestat_get", "(%u, " WASIADDRESS_FORMAT ")", fd, filestatAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_FILESTAT_GET, 0))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	VFS::FileInfo fileInfo;
	const VFS::Result result = wasiFD->vfd->getFileInfo(fileInfo);
	if(result != VFS::Result::success) { return TRACE_SYSCALL_RETURN(asWASIErrNo(result)); }

	__wasi_filestat_t& fileStat = memoryRef<__wasi_filestat_t>(process->memory, filestatAddress);

	fileStat.st_dev = fileInfo.deviceNumber;
	fileStat.st_ino = fileInfo.fileNumber;
	fileStat.st_filetype = asWASIFileType(fileInfo.type);
	fileStat.st_nlink = fileInfo.numLinks;
	fileStat.st_size = fileInfo.numBytes;
	fileStat.st_atim = __wasi_timestamp_t(fileInfo.lastAccessTime);
	fileStat.st_mtim = __wasi_timestamp_t(fileInfo.lastWriteTime);
	fileStat.st_ctim = __wasi_timestamp_t(fileInfo.creationTime);

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_filestat_set_times",
						  __wasi_errno_return_t,
						  wasi_fd_filestat_set_times,
						  __wasi_fd_t fd,
						  __wasi_timestamp_t lastAccessTime,
						  __wasi_timestamp_t lastWriteTime,
						  __wasi_fstflags_t flags)
{
	TRACE_SYSCALL("fd_filestat_set_times",
				  "(%u, %" PRIu64 ", %" PRIu64 ", 0x%04x)",
				  fd,
				  lastAccessTime,
				  lastWriteTime,
				  flags);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_FILESTAT_SET_TIMES, 0))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	I128 now = Platform::getRealtimeClock();

	bool setLastAccessTime = false;
	I128 lastAccessTimeI128;
	if(flags & __WASI_FILESTAT_SET_ATIM)
	{
		lastAccessTimeI128 = I128(lastAccessTime);
		setLastAccessTime = true;
	}
	else if(flags & __WASI_FILESTAT_SET_ATIM_NOW)
	{
		lastAccessTimeI128 = now;
		setLastAccessTime = true;
	}

	bool setLastWriteTime = false;
	I128 lastWriteTimeI128;
	if(flags & __WASI_FILESTAT_SET_MTIM)
	{
		lastWriteTimeI128 = I128(lastWriteTime);
		setLastWriteTime = true;
	}
	else if(flags & __WASI_FILESTAT_SET_MTIM_NOW)
	{
		lastWriteTimeI128 = now;
		setLastWriteTime = true;
	}

	const VFS::Result result = wasiFD->vfd->setFileTimes(
		setLastAccessTime, lastAccessTimeI128, setLastWriteTime, lastWriteTimeI128);

	return TRACE_SYSCALL_RETURN(asWASIErrNo(result));
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_filestat_set_size",
						  __wasi_errno_return_t,
						  wasi_fd_filestat_set_size,
						  __wasi_fd_t fd,
						  __wasi_filesize_t numBytes)
{
	TRACE_SYSCALL("fd_filestat_set_size", "(%u, %" PRIu64 ")", fd, numBytes);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_FILESTAT_SET_SIZE, 0))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	return TRACE_SYSCALL_RETURN(asWASIErrNo(wasiFD->vfd->setFileSize(numBytes)));
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_filestat_get",
						  __wasi_errno_return_t,
						  wasi_path_filestat_get,
						  __wasi_fd_t dirFD,
						  __wasi_lookupflags_t flags,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes,
						  WASIAddress filestatAddress)
{
	TRACE_SYSCALL("path_filestat_get",
				  "(%u, 0x%08x, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT ")",
				  dirFD,
				  flags,
				  pathAddress,
				  numPathBytes,
				  filestatAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiDirFD = getFD(process, dirFD);
	if(!wasiDirFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiDirFD, __WASI_RIGHT_PATH_FILESTAT_GET, 0))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	std::string relativePath;
	if(!readUserString(process->memory, pathAddress, numPathBytes, relativePath))
	{ return TRACE_SYSCALL_RETURN(__WASI_EFAULT); }

	std::string canonicalPath;
	if(!getCanonicalPath(wasiDirFD->originalPath, relativePath, canonicalPath))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	if(!process->fileSystem) { return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	VFS::FileInfo fileInfo;
	const VFS::Result result = process->fileSystem->getInfo(canonicalPath, fileInfo);
	if(result != VFS::Result::success) { return TRACE_SYSCALL_RETURN(asWASIErrNo(result)); }

	__wasi_filestat_t& fileStat = memoryRef<__wasi_filestat_t>(process->memory, filestatAddress);

	fileStat.st_dev = fileInfo.deviceNumber;
	fileStat.st_ino = fileInfo.fileNumber;
	fileStat.st_filetype = asWASIFileType(fileInfo.type);
	fileStat.st_nlink = fileInfo.numLinks;
	fileStat.st_size = fileInfo.numBytes;
	fileStat.st_atim = __wasi_timestamp_t(fileInfo.lastAccessTime);
	fileStat.st_mtim = __wasi_timestamp_t(fileInfo.lastWriteTime);
	fileStat.st_ctim = __wasi_timestamp_t(fileInfo.creationTime);

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_filestat_set_times",
						  __wasi_errno_return_t,
						  wasi_path_filestat_set_times,
						  __wasi_fd_t fd,
						  __wasi_lookupflags_t flags,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes,
						  __wasi_timestamp_t st_atim,
						  __wasi_timestamp_t st_mtim,
						  __wasi_fstflags_t fstflags)
{
	UNIMPLEMENTED_SYSCALL("path_filestat_set_times",
						  "(%u, 0x%08x, " WASIADDRESS_FORMAT ", %u, %" PRIu64 ", %" PRIu64
						  ", 0x%04x)",
						  fd,
						  flags,
						  pathAddress,
						  numPathBytes,
						  st_atim,
						  st_mtim,
						  fstflags);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_symlink",
						  __wasi_errno_return_t,
						  wasi_path_symlink,
						  WASIAddress oldPathAddress,
						  WASIAddress numOldPathBytes,
						  __wasi_fd_t fd,
						  WASIAddress newPathAddress,
						  WASIAddress numNewPathBytes)
{
	UNIMPLEMENTED_SYSCALL("path_symlink",
						  "(" WASIADDRESS_FORMAT ", %u, %u, " WASIADDRESS_FORMAT ", %u)",
						  oldPathAddress,
						  numOldPathBytes,
						  fd,
						  newPathAddress,
						  numNewPathBytes);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_unlink_file",
						  __wasi_errno_return_t,
						  wasi_path_unlink_file,
						  __wasi_fd_t dirFD,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes)
{
	TRACE_SYSCALL(
		"path_unlink_file", "(%u, " WASIADDRESS_FORMAT ", %u)", dirFD, pathAddress, numPathBytes);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiDirFD = getFD(process, dirFD);
	if(!wasiDirFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiDirFD, __WASI_RIGHT_PATH_UNLINK_FILE, 0))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	std::string relativePath;
	if(!readUserString(process->memory, pathAddress, numPathBytes, relativePath))
	{ return TRACE_SYSCALL_RETURN(__WASI_EFAULT); }

	std::string canonicalPath;
	if(!getCanonicalPath(wasiDirFD->originalPath, relativePath, canonicalPath))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	if(!process->fileSystem) { return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	VFS::Result result = process->fileSystem->unlinkFile(canonicalPath);
	return TRACE_SYSCALL_RETURN(result == VFS::Result::isDirectory ? __WASI_EPERM
																   : asWASIErrNo(result));
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_remove_directory",
						  __wasi_errno_return_t,
						  wasi_path_remove_directory,
						  __wasi_fd_t dirFD,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes)
{
	TRACE_SYSCALL("path_remove_directory",
				  "(%u, " WASIADDRESS_FORMAT ", %u)",
				  dirFD,
				  pathAddress,
				  numPathBytes);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiDirFD = getFD(process, dirFD);
	if(!wasiDirFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiDirFD, __WASI_RIGHT_PATH_UNLINK_FILE, 0))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	std::string relativePath;
	if(!readUserString(process->memory, pathAddress, numPathBytes, relativePath))
	{ return TRACE_SYSCALL_RETURN(__WASI_EFAULT); }

	std::string canonicalPath;
	if(!getCanonicalPath(wasiDirFD->originalPath, relativePath, canonicalPath))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	if(!process->fileSystem) { return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	const VFS::Result result = process->fileSystem->removeDir(canonicalPath);
	return TRACE_SYSCALL_RETURN(asWASIErrNo(result));
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_create_directory",
						  __wasi_errno_return_t,
						  wasi_path_create_directory,
						  __wasi_fd_t dirFD,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes)
{
	TRACE_SYSCALL("path_create_directory",
				  "(%u, " WASIADDRESS_FORMAT ", %u)",
				  dirFD,
				  pathAddress,
				  numPathBytes);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiDirFD = getFD(process, dirFD);
	if(!wasiDirFD) { return TRACE_SYSCALL_RETURN(__WASI_EBADF); }
	if(!checkFDRights(wasiDirFD, __WASI_RIGHT_PATH_UNLINK_FILE, 0))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	std::string relativePath;
	if(!readUserString(process->memory, pathAddress, numPathBytes, relativePath))
	{ return TRACE_SYSCALL_RETURN(__WASI_EFAULT); }

	std::string canonicalPath;
	if(!getCanonicalPath(wasiDirFD->originalPath, relativePath, canonicalPath))
	{ return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	if(!process->fileSystem) { return TRACE_SYSCALL_RETURN(__WASI_ENOTCAPABLE); }

	const VFS::Result result = process->fileSystem->createDir(canonicalPath);
	return TRACE_SYSCALL_RETURN(asWASIErrNo(result));
}
