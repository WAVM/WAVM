#include "./WASIPrivate.h"
#include "./WASITypes.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/VFS/VFS.h"

using namespace WAVM;
using namespace WAVM::WASI;
using namespace WAVM::Runtime;

namespace WAVM { namespace WASI {
	DEFINE_INTRINSIC_MODULE(wasiFile)
}}

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
	case VFS::FileType::pipe: Errors::unreachable();

	default: Errors::unreachable();
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

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_prestat_get",
						  __wasi_errno_t,
						  wasi_fd_prestat_get,
						  __wasi_fd_t fd,
						  WASIAddress prestatAddress)
{
	TRACE_SYSCALL("fd_prestat_get", "(%u, " WASIADDRESS_FORMAT ")", fd, prestatAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD || !wasiFD->isPreopened) { return TRACE_SYSCALL_RETURN(EBADF); }

	if(wasiFD->originalPath.size() > UINT32_MAX) { return TRACE_SYSCALL_RETURN(EOVERFLOW); }

	__wasi_prestat_t& prestat = memoryRef<__wasi_prestat_t>(process->memory, prestatAddress);
	prestat.pr_type = wasiFD->preopenedType;
	wavmAssert(wasiFD->preopenedType == __WASI_PREOPENTYPE_DIR);
	prestat.u.dir.pr_name_len = U32(wasiFD->originalPath.size());

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_prestat_dir_name",
						  __wasi_errno_t,
						  wasi_fd_prestat_dir_name,
						  __wasi_fd_t fd,
						  WASIAddress bufferAddress,
						  WASIAddress bufferLength)
{
	TRACE_SYSCALL(
		"fd_prestat_dir_name", "(%u, " WASIADDRESS_FORMAT ", %u)", fd, bufferAddress, bufferLength);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD || !wasiFD->isPreopened) { return TRACE_SYSCALL_RETURN(EBADF); }

	if(bufferLength != wasiFD->originalPath.size()) { return TRACE_SYSCALL_RETURN(EINVAL); }

	char* buffer = memoryArrayPtr<char>(process->memory, bufferAddress, bufferLength);
	memcpy(buffer, wasiFD->originalPath.c_str(), bufferLength);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile, "fd_close", __wasi_errno_t, wasi_fd_close, __wasi_fd_t fd)
{
	TRACE_SYSCALL("fd_close", "(%u)", fd);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD || wasiFD->isPreopened) { return TRACE_SYSCALL_RETURN(EBADF); }

	VFS::CloseResult result = wasiFD->vfd->close();
	switch(result)
	{
	case VFS::CloseResult::success:
		// If the close succeeded, remove the fd from the fds map.
		process->fds.removeOrFail(fd);
		return TRACE_SYSCALL_RETURN(ESUCCESS);

	case VFS::CloseResult::interrupted: return TRACE_SYSCALL_RETURN(EINTR);
	case VFS::CloseResult::ioError: return TRACE_SYSCALL_RETURN(EIO);

	default: Errors::unreachable();
	};
}

DEFINE_INTRINSIC_FUNCTION(wasiFile, "fd_datasync", __wasi_errno_t, wasi_fd_datasync, __wasi_fd_t fd)
{
	TRACE_SYSCALL("fd_datasync", "(%u)", fd);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_DATASYNC))
	{ return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	wasiFD->vfd->sync(VFS::SyncType::contents);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_pread",
						  __wasi_errno_t,
						  wasi_fd_pread,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  __wasi_filesize_t offset,
						  WASIAddress numBytesReadAddress)
{
	UNIMPLEMENTED_SYSCALL("fd_pread",
						  "(%u, " WASIADDRESS_FORMAT ", %u, %" PRIu64 ", " WASIADDRESS_FORMAT ")",
						  fd,
						  iovsAddress,
						  numIOVs,
						  offset,
						  numBytesReadAddress);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_pwrite",
						  __wasi_errno_t,
						  wasi_fd_pwrite,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  __wasi_filesize_t offset,
						  WASIAddress numBytesWrittenAddress)
{
	UNIMPLEMENTED_SYSCALL("fd_pwrite",
						  "(%u, " WASIADDRESS_FORMAT ", %u, %" PRIu64 ", " WASIADDRESS_FORMAT ")",
						  fd,
						  iovsAddress,
						  numIOVs,
						  offset,
						  numBytesWrittenAddress);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_read",
						  __wasi_errno_t,
						  wasi_fd_read,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  WASIAddress numBytesReadAddress)
{
	TRACE_SYSCALL("fd_read",
				  "(%u, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT ")",
				  fd,
				  iovsAddress,
				  numIOVs,
				  numBytesReadAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_READ)) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	const __wasi_iovec_t* iovs
		= memoryArrayPtr<__wasi_iovec_t>(process->memory, iovsAddress, numIOVs);
	U64 numBytesRead = 0;
	VFS::ReadResult readResult = VFS::ReadResult::success;
	for(WASIAddress iovIndex = 0; iovIndex < numIOVs; ++iovIndex)
	{
		__wasi_iovec_t iov = iovs[iovIndex];

		Uptr numBytesReadThisIO = 0;
		readResult = wasiFD->vfd->read(memoryArrayPtr<U8>(process->memory, iov.buf, iov.buf_len),
									   iov.buf_len,
									   &numBytesReadThisIO);
		if(readResult != VFS::ReadResult::success) { break; }

		numBytesRead += numBytesReadThisIO;
		if(numBytesReadThisIO < iov.buf_len) { break; }
	}

	if(numBytesRead > WASIADDRESS_MAX) { return TRACE_SYSCALL_RETURN(EOVERFLOW); }
	memoryRef<WASIAddress>(process->memory, numBytesReadAddress) = WASIAddress(numBytesRead);

	switch(readResult)
	{
	case VFS::ReadResult::success:
		return TRACE_SYSCALL_RETURN(ESUCCESS, " (numBytesRead=%" PRIu64 ")", numBytesRead);
	case VFS::ReadResult::ioError: return TRACE_SYSCALL_RETURN(EIO);
	case VFS::ReadResult::interrupted: return TRACE_SYSCALL_RETURN(EINTR);
	case VFS::ReadResult::tooManyBytes: return TRACE_SYSCALL_RETURN(EINVAL);
	case VFS::ReadResult::notPermitted: return TRACE_SYSCALL_RETURN(EPERM);
	case VFS::ReadResult::isDirectory: return TRACE_SYSCALL_RETURN(EISDIR);
	case VFS::ReadResult::outOfMemory: return TRACE_SYSCALL_RETURN(ENOMEM);

	default: Errors::unreachable();
	};
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_renumber",
						  __wasi_errno_t,
						  wasi_fd_renumber,
						  __wasi_fd_t from,
						  __wasi_fd_t to)
{
	UNIMPLEMENTED_SYSCALL("fd_renumber", "(%u, %u)", from, to);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_seek",
						  __wasi_errno_t,
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
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_SEEK)) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	VFS::SeekOrigin origin;
	switch(whence)
	{
	case __WASI_WHENCE_CUR: origin = VFS::SeekOrigin::cur; break;
	case __WASI_WHENCE_END: origin = VFS::SeekOrigin::end; break;
	case __WASI_WHENCE_SET: origin = VFS::SeekOrigin::begin; break;
	default: return TRACE_SYSCALL_RETURN(EINVAL);
	};

	U64 newOffset;
	VFS::SeekResult seekResult = wasiFD->vfd->seek(offset, origin, &newOffset);
	switch(seekResult)
	{
	case VFS::SeekResult::success:
		memoryRef<__wasi_filesize_t>(process->memory, newOffsetAddress) = newOffset;
		return TRACE_SYSCALL_RETURN(ESUCCESS);
	case VFS::SeekResult::invalidOffset: return TRACE_SYSCALL_RETURN(EINVAL);
	case VFS::SeekResult::unseekable: return TRACE_SYSCALL_RETURN(ESPIPE);

	default: Errors::unreachable();
	};
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_tell",
						  __wasi_errno_t,
						  wasi_fd_tell,
						  __wasi_fd_t fd,
						  WASIAddress offsetAddress)
{
	TRACE_SYSCALL("fd_tell", "(%u, " WASIADDRESS_FORMAT ")", fd, offsetAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_TELL)) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	U64 currentOffset;
	VFS::SeekResult seekResult = wasiFD->vfd->seek(0, VFS::SeekOrigin::cur, &currentOffset);
	switch(seekResult)
	{
	case VFS::SeekResult::success:
		memoryRef<__wasi_filesize_t>(process->memory, offsetAddress) = currentOffset;
		return TRACE_SYSCALL_RETURN(ESUCCESS);
	case VFS::SeekResult::invalidOffset: return TRACE_SYSCALL_RETURN(EOVERFLOW);
	case VFS::SeekResult::unseekable: return TRACE_SYSCALL_RETURN(ESPIPE);

	default: Errors::unreachable();
	};
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_fdstat_get",
						  __wasi_errno_t,
						  wasi_fd_fdstat_get,
						  __wasi_fd_t fd,
						  WASIAddress fdstatAddress)
{
	TRACE_SYSCALL("fd_fdstat_get", "(%u, " WASIADDRESS_FORMAT ")", fd, fdstatAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }

	VFS::FDInfo fdInfo;
	VFS::GetInfoResult result = wasiFD->vfd->getFDInfo(fdInfo);
	switch(result)
	{
	case VFS::GetInfoResult::success:
	{
		__wasi_fdstat_t& fdstat = memoryRef<__wasi_fdstat_t>(process->memory, fdstatAddress);
		fdstat.fs_filetype = asWASIFileType(fdInfo.type);
		fdstat.fs_flags = 0;

		if(fdInfo.append) { fdstat.fs_flags |= __WASI_FDFLAG_APPEND; }
		if(fdInfo.nonBlocking) { fdstat.fs_flags |= __WASI_FDFLAG_NONBLOCK; }
		switch(fdInfo.implicitSync)
		{
		case VFS::FDImplicitSync::none: break;
		case VFS::FDImplicitSync::syncContentsAfterWrite:
			fdstat.fs_flags |= __WASI_FDFLAG_DSYNC;
			break;
		case VFS::FDImplicitSync::syncContentsAndMetadataAfterWrite:
			fdstat.fs_flags |= __WASI_FDFLAG_SYNC;
			break;
		case VFS::FDImplicitSync::syncContentsAfterWriteAndBeforeRead:
			fdstat.fs_flags |= __WASI_FDFLAG_DSYNC | __WASI_FDFLAG_RSYNC;
			break;
		case VFS::FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead:
			fdstat.fs_flags |= __WASI_FDFLAG_SYNC | __WASI_FDFLAG_RSYNC;
			break;

		default: Errors::unreachable();
		}

		fdstat.fs_rights_base = wasiFD->rights;
		fdstat.fs_rights_inheriting = wasiFD->inheritingRights;

		return TRACE_SYSCALL_RETURN(ESUCCESS);
	}

	case VFS::GetInfoResult::ioError: return TRACE_SYSCALL_RETURN(EIO);

	default: Errors::unreachable();
	};
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_fdstat_set_flags",
						  __wasi_errno_t,
						  wasi_fd_fdstat_set_flags,
						  __wasi_fd_t fd,
						  __wasi_fdflags_t flags)
{
	UNIMPLEMENTED_SYSCALL("fd_fdstat_set_flags", "(%u, 0x%04x)", fd, flags);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_fdstat_set_rights",
						  __wasi_errno_t,
						  wasi_fd_fdstat_set_rights,
						  __wasi_fd_t fd,
						  __wasi_rights_t rights,
						  __wasi_rights_t inheritingRights)
{
	UNIMPLEMENTED_SYSCALL("fd_fdstat_set_rights",
						  "(%u, 0x%" PRIx64 ", 0x %" PRIx64 ") ",
						  fd,
						  rights,
						  inheritingRights);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile, "fd_sync", __wasi_errno_t, wasi_fd_sync, __wasi_fd_t fd)
{
	TRACE_SYSCALL("fd_sync", "(%u)", fd);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_SYNC)) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	wasiFD->vfd->sync(VFS::SyncType::contentsAndMetadata);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_write",
						  __wasi_errno_t,
						  wasi_fd_write,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  WASIAddress numBytesWrittenAddress)
{
	TRACE_SYSCALL("fd_write",
				  "(%u, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT ")",
				  fd,
				  iovsAddress,
				  numIOVs,
				  numBytesWrittenAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_WRITE)) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	const __wasi_ciovec_t* iovs
		= memoryArrayPtr<__wasi_ciovec_t>(process->memory, iovsAddress, numIOVs);
	U64 numBytesWritten = 0;
	VFS::WriteResult writeResult = VFS::WriteResult::success;
	for(WASIAddress iovIndex = 0; iovIndex < numIOVs; ++iovIndex)
	{
		__wasi_ciovec_t iov = iovs[iovIndex];

		Uptr numBytesWrittenThisIO = 0;
		writeResult = wasiFD->vfd->write(memoryArrayPtr<U8>(process->memory, iov.buf, iov.buf_len),
										 iov.buf_len,
										 &numBytesWrittenThisIO);
		if(writeResult != VFS::WriteResult::success) { break; }

		numBytesWritten += numBytesWrittenThisIO;
		if(numBytesWrittenThisIO < iov.buf_len) { break; }
	}

	if(numBytesWritten > WASIADDRESS_MAX) { return TRACE_SYSCALL_RETURN(EOVERFLOW); }
	memoryRef<WASIAddress>(process->memory, numBytesWrittenAddress) = WASIAddress(numBytesWritten);

	switch(writeResult)
	{
	case VFS::WriteResult::success:
		return TRACE_SYSCALL_RETURN(ESUCCESS, " (numBytesWritten=%" PRIu64 ")", numBytesWritten);
	case VFS::WriteResult::ioError: return TRACE_SYSCALL_RETURN(EIO);
	case VFS::WriteResult::interrupted: return TRACE_SYSCALL_RETURN(EINTR);
	case VFS::WriteResult::invalidArgument: return TRACE_SYSCALL_RETURN(EINVAL);
	case VFS::WriteResult::outOfMemory: return TRACE_SYSCALL_RETURN(ENOMEM);
	case VFS::WriteResult::outOfQuota: return TRACE_SYSCALL_RETURN(EDQUOT);
	case VFS::WriteResult::outOfFreeSpace: return TRACE_SYSCALL_RETURN(ENOSPC);
	case VFS::WriteResult::exceededFileSizeLimit: return TRACE_SYSCALL_RETURN(EFBIG);
	case VFS::WriteResult::notPermitted: return TRACE_SYSCALL_RETURN(EPERM);

	default: Errors::unreachable();
	};
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_advise",
						  __wasi_errno_t,
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
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_ADVISE)) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

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
		return TRACE_SYSCALL_RETURN(ESUCCESS);
	default: return TRACE_SYSCALL_RETURN(EINVAL);
	}
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_allocate",
						  __wasi_errno_t,
						  wasi_fd_allocate,
						  __wasi_fd_t fd,
						  __wasi_filesize_t offset,
						  __wasi_filesize_t numBytes)
{
	UNIMPLEMENTED_SYSCALL("fd_allocate", "(%u, %" PRIu64 ", %" PRIu64 ")", fd, offset, numBytes);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_link",
						  __wasi_errno_t,
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
						  __wasi_errno_t,
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
	default: return TRACE_SYSCALL_RETURN(EINVAL);
	};

	if(openFlags & __WASI_O_CREAT) { requiredDirRights |= __WASI_RIGHT_PATH_CREATE_FILE; }
	if(openFlags & __WASI_O_TRUNC) { requiredDirRights |= __WASI_RIGHT_PATH_FILESTAT_SET_SIZE; }

	VFS::FDImplicitSync implicitSync = VFS::FDImplicitSync::none;
	if(fdFlags & __WASI_FDFLAG_DSYNC)
	{
		implicitSync = (fdFlags & __WASI_FDFLAG_RSYNC)
						   ? VFS::FDImplicitSync::syncContentsAfterWriteAndBeforeRead
						   : VFS::FDImplicitSync::syncContentsAfterWrite;
		requiredDirInheritingRights |= __WASI_RIGHT_FD_DATASYNC;
	}
	if(fdFlags & __WASI_FDFLAG_SYNC)
	{
		implicitSync = (fdFlags & __WASI_FDFLAG_RSYNC)
						   ? VFS::FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead
						   : VFS::FDImplicitSync::syncContentsAndMetadataAfterWrite;
		requiredDirInheritingRights |= __WASI_RIGHT_FD_SYNC;
	}
	if(write && !(fdFlags & (__WASI_FDFLAG_APPEND | __WASI_O_TRUNC)))
	{ requiredDirInheritingRights |= __WASI_RIGHT_FD_SEEK; }

	WASI::FD* wasiDirFD = getFD(process, dirFD);
	if(!wasiDirFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiDirFD, requiredDirRights, requiredDirInheritingRights))
	{ return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	std::string relativePath;
	if(!readUserString(process->memory, pathAddress, numPathBytes, relativePath))
	{ return TRACE_SYSCALL_RETURN(EFAULT); }

	std::string canonicalPath;
	if(!getCanonicalPath(wasiDirFD->originalPath, relativePath, canonicalPath))
	{ return TRACE_SYSCALL_RETURN(ENOENT); }

	if(!process->fileSystem) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	VFS::FD* openedVFD = nullptr;
	VFS::OpenResult result
		= process->fileSystem->open(canonicalPath, accessMode, createMode, openedVFD, implicitSync);
	switch(result)
	{
	case VFS::OpenResult::success: break;

	case VFS::OpenResult::alreadyExists: return TRACE_SYSCALL_RETURN(EEXIST);
	case VFS::OpenResult::doesNotExist: return TRACE_SYSCALL_RETURN(ENOENT);
	case VFS::OpenResult::isDirectory: return TRACE_SYSCALL_RETURN(EISDIR);
	case VFS::OpenResult::cantSynchronize: return TRACE_SYSCALL_RETURN(EINVAL);
	case VFS::OpenResult::invalidNameCharacter: return TRACE_SYSCALL_RETURN(EACCES);
	case VFS::OpenResult::nameTooLong: return TRACE_SYSCALL_RETURN(ENAMETOOLONG);
	case VFS::OpenResult::pathUsesFileAsDirectory: return TRACE_SYSCALL_RETURN(ENOTDIR);

	case VFS::OpenResult::notPermitted: return TRACE_SYSCALL_RETURN(ENOTCAPABLE);
	case VFS::OpenResult::ioError: return TRACE_SYSCALL_RETURN(EIO);
	case VFS::OpenResult::interrupted: return TRACE_SYSCALL_RETURN(EINTR);

	case VFS::OpenResult::outOfMemory: return TRACE_SYSCALL_RETURN(ENOMEM);
	case VFS::OpenResult::outOfQuota: return TRACE_SYSCALL_RETURN(EDQUOT);
	case VFS::OpenResult::outOfFreeSpace: return TRACE_SYSCALL_RETURN(ENOSPC);

	default: Errors::unreachable();
	};

	__wasi_fd_t fd = process->fds.add(
		UINT32_MAX,
		FD{openedVFD, requestedRights, requestedInheritingRights, canonicalPath, false});
	if(fd == UINT32_MAX)
	{
		errorUnless(openedVFD->close() == VFS::CloseResult::success);
		return TRACE_SYSCALL_RETURN(EMFILE);
	}

	memoryRef<__wasi_fd_t>(process->memory, fdAddress) = fd;

	return TRACE_SYSCALL_RETURN(ESUCCESS, " (%u)", fd);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_readdir",
						  __wasi_errno_t,
						  wasi_fd_readdir,
						  __wasi_fd_t fd,
						  WASIAddress bufferAddress,
						  WASIAddress numBufferBytes,
						  __wasi_dircookie_t cookie,
						  WASIAddress outNumBufferBytesUsedAddress)
{
	UNIMPLEMENTED_SYSCALL("fd_readdir",
						  "(%u, " WASIADDRESS_FORMAT ", %u, 0x%" PRIx64 ", " WASIADDRESS_FORMAT ")",
						  fd,
						  bufferAddress,
						  numBufferBytes,
						  cookie,
						  outNumBufferBytesUsedAddress);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_readlink",
						  __wasi_errno_t,
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
						  __wasi_errno_t,
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
						  __wasi_errno_t,
						  wasi_fd_filestat_get,
						  __wasi_fd_t fd,
						  WASIAddress filestatAddress)
{
	TRACE_SYSCALL("fd_filestat_get", "(%u, " WASIADDRESS_FORMAT ")", fd, filestatAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_FILESTAT_GET, 0))
	{ return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	VFS::FileInfo fileInfo;
	VFS::GetInfoResult result = wasiFD->vfd->getFileInfo(fileInfo);
	switch(result)
	{
	case VFS::GetInfoResult::success: break;
	case VFS::GetInfoResult::ioError: return TRACE_SYSCALL_RETURN(EIO);

	default: Errors::unreachable();
	};

	__wasi_filestat_t& fileStat = memoryRef<__wasi_filestat_t>(process->memory, filestatAddress);

	fileStat.st_dev = fileInfo.deviceNumber;
	fileStat.st_ino = fileInfo.fileNumber;
	fileStat.st_filetype = asWASIFileType(fileInfo.type);
	fileStat.st_nlink = fileInfo.numLinks;
	fileStat.st_size = fileInfo.numBytes;
	fileStat.st_atim = __wasi_timestamp_t(fileInfo.lastAccessTime);
	fileStat.st_mtim = __wasi_timestamp_t(fileInfo.lastWriteTime);
	fileStat.st_ctim = __wasi_timestamp_t(fileInfo.creationTime);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_filestat_set_times",
						  __wasi_errno_t,
						  wasi_fd_filestat_set_times,
						  __wasi_fd_t fd,
						  __wasi_timestamp_t st_atim,
						  __wasi_timestamp_t st_mtim,
						  __wasi_fstflags_t fstflags)
{
	UNIMPLEMENTED_SYSCALL("fd_filestat_set_times",
						  "(%u, %" PRIu64 ", %" PRIu64 ", 0x%04x)",
						  fd,
						  st_atim,
						  st_mtim,
						  fstflags);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "fd_filestat_set_size",
						  __wasi_errno_t,
						  wasi_fd_filestat_set_size,
						  __wasi_fd_t fd,
						  __wasi_filesize_t numBytes)
{
	UNIMPLEMENTED_SYSCALL("fd_filestat_set_size", "(%u, %" PRIu64 ")", fd, numBytes);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_filestat_get",
						  __wasi_errno_t,
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
	if(!wasiDirFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiDirFD, __WASI_RIGHT_PATH_FILESTAT_GET, 0))
	{ return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	std::string relativePath;
	if(!readUserString(process->memory, pathAddress, numPathBytes, relativePath))
	{ return TRACE_SYSCALL_RETURN(EFAULT); }

	std::string canonicalPath;
	if(!getCanonicalPath(wasiDirFD->originalPath, relativePath, canonicalPath))
	{ return TRACE_SYSCALL_RETURN(ENOENT); }

	if(!process->fileSystem) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	VFS::FileInfo fileInfo;
	VFS::GetInfoByPathResult result = process->fileSystem->getInfo(canonicalPath, fileInfo);
	switch(result)
	{
	case VFS::GetInfoByPathResult::success: break;

	case VFS::GetInfoByPathResult::doesNotExist: return TRACE_SYSCALL_RETURN(ENOENT);
	case VFS::GetInfoByPathResult::invalidNameCharacter: return TRACE_SYSCALL_RETURN(EACCES);
	case VFS::GetInfoByPathResult::nameTooLong: return TRACE_SYSCALL_RETURN(ENAMETOOLONG);
	case VFS::GetInfoByPathResult::pathUsesFileAsDirectory: return TRACE_SYSCALL_RETURN(ENOTDIR);

	case VFS::GetInfoByPathResult::notPermitted: return TRACE_SYSCALL_RETURN(ENOTCAPABLE);
	case VFS::GetInfoByPathResult::ioError: return TRACE_SYSCALL_RETURN(EIO);

	default: Errors::unreachable();
	};

	__wasi_filestat_t& fileStat = memoryRef<__wasi_filestat_t>(process->memory, filestatAddress);

	fileStat.st_dev = fileInfo.deviceNumber;
	fileStat.st_ino = fileInfo.fileNumber;
	fileStat.st_filetype = asWASIFileType(fileInfo.type);
	fileStat.st_nlink = fileInfo.numLinks;
	fileStat.st_size = fileInfo.numBytes;
	fileStat.st_atim = __wasi_timestamp_t(fileInfo.lastAccessTime);
	fileStat.st_mtim = __wasi_timestamp_t(fileInfo.lastWriteTime);
	fileStat.st_ctim = __wasi_timestamp_t(fileInfo.creationTime);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_filestat_set_times",
						  __wasi_errno_t,
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
						  __wasi_errno_t,
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
						  __wasi_errno_t,
						  wasi_path_unlink_file,
						  __wasi_fd_t fd,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes)
{
	UNIMPLEMENTED_SYSCALL(
		"path_unlink_file", "(%u, " WASIADDRESS_FORMAT ", %u)", fd, pathAddress, numPathBytes);
}

DEFINE_INTRINSIC_FUNCTION(wasiFile,
						  "path_remove_directory",
						  __wasi_errno_t,
						  wasi_path_remove_directory,
						  __wasi_fd_t fd,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes)
{
	UNIMPLEMENTED_SYSCALL(
		"path_remove_directory", "(%u, " WASIADDRESS_FORMAT ", %u)", fd, pathAddress, numPathBytes);
}
