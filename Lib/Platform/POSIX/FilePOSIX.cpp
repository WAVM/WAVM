#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"

using namespace WAVM;
using namespace WAVM::Platform;
using namespace WAVM::VFS;

static FileType getFileTypeFromMode(mode_t mode)
{
	switch(mode & S_IFMT)
	{
	case S_IFBLK: return FileType::blockDevice;
	case S_IFCHR: return FileType::characterDevice;
	case S_IFIFO: return FileType::pipe;
	case S_IFREG: return FileType::file;
	case S_IFDIR: return FileType::directory;
	case S_IFLNK: return FileType::symbolicLink;
	case S_IFSOCK:
		// return FileType::datagramSocket;
		// return FileType::streamSocket;
		Errors::unreachable();
		break;
	default: return FileType::unknown;
	};
}

static I128 timeToNS(time_t time)
{
	// time_t might be a long, and I128 doesn't have a long constructor, so coerce it to an integer
	// type first.
	const U64 timeInt = U64(time);
	return assumeNoOverflow(I128(timeInt) * 1000000000);
}

static void getFileInfoFromStatus(const struct stat& status, FileInfo& outInfo)
{
	outInfo.deviceNumber = status.st_dev;
	outInfo.fileNumber = status.st_ino;
	outInfo.type = getFileTypeFromMode(status.st_mode);
	outInfo.numLinks = status.st_nlink;
	outInfo.numBytes = status.st_size;
	outInfo.lastAccessTime = timeToNS(status.st_atime);
	outInfo.lastWriteTime = timeToNS(status.st_mtime);
	outInfo.creationTime = timeToNS(status.st_ctime);
}

struct POSIXFD : FD
{
	const I32 fd;

	POSIXFD(I32 inFD) : fd(inFD) {}

	virtual CloseResult close() override
	{
		wavmAssert(fd >= 0);
		if(::close(fd))
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("close returned EBADF (fd=%i)", fd);

			case EINTR: return CloseResult::interrupted;
			case EIO: return CloseResult::ioError;
			};
		}
		delete this;
		return CloseResult::success;
	}

	virtual SeekResult seek(I64 offset,
							SeekOrigin origin,
							U64* outAbsoluteOffset = nullptr) override
	{
		I32 whence = 0;
		switch(origin)
		{
		case SeekOrigin::begin: whence = SEEK_SET; break;
		case SeekOrigin::cur: whence = SEEK_CUR; break;
		case SeekOrigin::end: whence = SEEK_END; break;
		default: Errors::unreachable();
		};

#ifdef __linux__
		const I64 result = lseek64(fd, offset, whence);
#else
		const I64 result = lseek(fd, reinterpret_cast<off_t>(offset), whence);
#endif
		if(result != -1)
		{
			if(outAbsoluteOffset) { *outAbsoluteOffset = U64(result); }
			return SeekResult::success;
		}
		else
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("lseek returned EBADF (fd=%i)", fd);
			case EOVERFLOW:
				Errors::fatalf("lseek(%i, %" PRIi64 ", %u) returned EOVERFLOW", fd, offset, whence);

			case EINVAL: return SeekResult::invalidOffset;
			case ESPIPE: return SeekResult::unseekable;

			default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
			}
		}
	}
	virtual ReadResult read(void* outData, Uptr numBytes, Uptr* outNumBytesRead = nullptr) override
	{
		ssize_t result = ::read(fd, outData, numBytes);
		if(result != -1)
		{
			if(outNumBytesRead) { *outNumBytesRead = result; }
			return ReadResult::success;
		}
		else
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("read returned EBADF (fd=%i)", fd);

			case EIO: return ReadResult::ioError;
			case EINTR: return ReadResult::interrupted;
			case EISDIR: return ReadResult::isDirectory;

			default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
			};
		}
	}
	virtual WriteResult write(const void* data,
							  Uptr numBytes,
							  Uptr* outNumBytesWritten = nullptr) override
	{
		ssize_t result = ::write(fd, data, numBytes);
		if(result != -1)
		{
			if(outNumBytesWritten) { *outNumBytesWritten = result; }
			return WriteResult::success;
		}
		else
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("write returned EBADF (fd=%i)", fd);

			case EIO: return WriteResult::ioError;
			case EINTR: return WriteResult::interrupted;
			case EDQUOT: return WriteResult::outOfQuota;
			case ENOSPC: return WriteResult::outOfFreeSpace;
			case EFBIG: return WriteResult::exceededFileSizeLimit;
			case EPERM: return WriteResult::notPermitted;

			default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
			};
		}
	}
	virtual SyncResult sync(SyncType syncType) override
	{
#ifdef __APPLE__
		I32 result = fsync(fd);
#else
		I32 result;
		switch(syncType)
		{
		case SyncType::contents: result = fdatasync(fd); break;
		case SyncType::contentsAndMetadata: result = fsync(fd); break;
		default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
		}
#endif

		if(result == 0) { return SyncResult::success; }
		else
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("fdatasync returned EBADF (fd=%i)", fd);

			case EIO: return SyncResult::ioError;
			case EINTR: return SyncResult::interrupted;
			case EINVAL: return SyncResult::notSupported;

			default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
			};
		}
	}
	virtual GetInfoResult getFDInfo(FDInfo& outInfo) override
	{
		struct stat fdStatus;
		if(fstat(fd, &fdStatus) != 0)
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("fstat return EBADF (fd=%i)", fd);

			case EIO: return GetInfoResult::ioError;
			case EOVERFLOW:
				// EOVERFLOW: The file size in bytes or the number of blocks allocated to the file
				// or the file serial number cannot be represented correctly in the structure
				// pointed to by buf.

			default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
			};
		}

		outInfo.type = getFileTypeFromMode(fdStatus.st_mode);
		outInfo.append = false;
		outInfo.nonBlocking = false;

		I32 fdFlags = fcntl(fd, F_GETFL);
		if(fdFlags < 0)
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("fcntl returned EBADF (fd=%i)", fd);

			default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
			};
		}

		if(fdFlags & O_SYNC)
		{
#ifdef O_RSYNC
			outInfo.implicitSync
				= fdFlags & O_RSYNC ? FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead
									: FDImplicitSync::syncContentsAndMetadataAfterWrite;
#else
			outInfo.implicitSync = FDImplicitSync::syncContentsAndMetadataAfterWrite;
#endif
		}
		else if(fdFlags & O_DSYNC)
		{
#ifdef O_RSYNC
			outInfo.implicitSync = fdFlags & O_RSYNC
									   ? FDImplicitSync::syncContentsAfterWriteAndBeforeRead
									   : FDImplicitSync::syncContentsAfterWrite;
#else
			outInfo.implicitSync = FDImplicitSync::syncContentsAfterWrite;
#endif
		}
		else
		{
			outInfo.implicitSync = FDImplicitSync::none;
		}

		return GetInfoResult::success;
	}

	virtual GetInfoResult getFileInfo(FileInfo& outInfo) override
	{
		struct stat fdStatus;

		if(fstat(fd, &fdStatus))
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("fstat returned EBADF (fd=%i)", fd);

			case EIO: return GetInfoResult::ioError;

			case EOVERFLOW:
				// The file size in bytes or the number of blocks allocated to the file or the file
				// serial number cannot be represented correctly in the structure pointed to by buf.

			default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
			};
		}

		getFileInfoFromStatus(fdStatus, outInfo);
		return GetInfoResult::success;
	}
};

struct POSIXStdFD : POSIXFD
{
	POSIXStdFD(I32 inFD) : POSIXFD(inFD) {}

	virtual CloseResult close() override
	{
		// The stdio FDs are shared, so don't close them.
		return CloseResult::success;
	}
};

FD* Platform::getStdFD(StdDevice device)
{
	static POSIXStdFD* stdinVFD = new POSIXStdFD(0);
	static POSIXStdFD* stdoutVFD = new POSIXStdFD(1);
	static POSIXStdFD* stderrVFD = new POSIXStdFD(2);
	switch(device)
	{
	case StdDevice::in: return stdinVFD; break;
	case StdDevice::out: return stdoutVFD; break;
	case StdDevice::err: return stderrVFD; break;
	default: Errors::unreachable();
	};
}

OpenResult Platform::openHostFile(const std::string& pathName,
								  FileAccessMode accessMode,
								  FileCreateMode createMode,
								  FD*& outFD,
								  FDImplicitSync implicitSync)
{
	U32 flags = 0;
	switch(accessMode)
	{
	case FileAccessMode::none: flags = O_RDONLY; break;
	case FileAccessMode::readOnly: flags = O_RDONLY; break;
	case FileAccessMode::writeOnly: flags = O_WRONLY; break;
	case FileAccessMode::readWrite: flags = O_RDWR; break;
	default: Errors::unreachable();
	};

	switch(createMode)
	{
	case FileCreateMode::createAlways: flags |= O_CREAT | O_TRUNC; break;
	case FileCreateMode::createNew: flags |= O_CREAT | O_EXCL; break;
	case FileCreateMode::openAlways: flags |= O_CREAT; break;
	case FileCreateMode::openExisting: break;
	case FileCreateMode::truncateExisting: flags |= O_TRUNC; break;
	default: Errors::unreachable();
	};

	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	switch(implicitSync)
	{
	case FDImplicitSync::none: break;
	case FDImplicitSync::syncContentsAfterWrite: flags |= O_DSYNC; break;
	case FDImplicitSync::syncContentsAndMetadataAfterWrite: flags |= O_SYNC; break;

#ifdef __APPLE__
	// Apple doesn't support O_RSYNC.
	case FDImplicitSync::syncContentsAfterWriteAndBeforeRead:
		Errors::fatal(
			"FDImplicitSync::syncContentsAfterWriteAndBeforeRead is not yet implemented on Apple "
			"platforms.");
	case FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead:
		Errors::fatal(
			"FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead is not yet implemented "
			"on Apple platforms.");
#else
	case FDImplicitSync::syncContentsAfterWriteAndBeforeRead: flags |= O_DSYNC | O_RSYNC; break;
	case FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead:
		flags |= O_SYNC | O_RSYNC;
		break;
#endif

	default: Errors::unreachable();
	}

	const I32 fd = open(pathName.c_str(), flags, mode);

	if(fd != -1)
	{
		outFD = new POSIXFD(fd);
		return OpenResult::success;
	}
	else
	{
		switch(errno)
		{
		case EACCES: return OpenResult::notPermitted;
		case EEXIST: return OpenResult::alreadyExists;
		case EINTR: return OpenResult::interrupted;
		case EINVAL: return OpenResult::cantSynchronize;
		case EIO: return OpenResult::ioError;
		case EISDIR: return OpenResult::isDirectory;
		case EMFILE: return OpenResult::outOfMemory;
		case ENAMETOOLONG: return OpenResult::nameTooLong;
		case ENFILE: return OpenResult::outOfMemory;
		case ENOENT: return OpenResult::doesNotExist;
		case ENOSPC: return OpenResult::outOfFreeSpace;
		case ENOTDIR: return OpenResult::pathUsesFileAsDirectory;
		case EROFS: return OpenResult::notPermitted;
		case ENOMEM: return OpenResult::outOfMemory;
		case EDQUOT: return OpenResult::outOfQuota;
		case EPERM: return OpenResult::notPermitted;

		case ELOOP:
			// A loop exists in symbolic links encountered during resolution of the path argument.
			// More than {SYMLOOP_MAX} symbolic links were encountered during resolution of the path
			// argument.
		case ENOSR:
			// The path argument names a STREAMS-based file and the system is unable to allocate a
			// STREAM.
		case ENXIO:
			// O_NONBLOCK is set, the named file is a FIFO, O_WRONLY is set, and no process has the
			// file open for reading.
			// The named file is a character special or block special file, and the device
			// associated with this special file does not exist.
		case EOVERFLOW:
			// The named file is a regular file and the size of the file cannot be represented
			// correctly in an object of type off_t.
		case EAGAIN:
			// The path argument names the slave side of a pseudo-terminal device that is locked.
		case ETXTBSY:
			// The file is a pure procedure (shared text) file that is being executed and oflag is
			// O_WRONLY or O_RDWR.

		default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
		};
	}
}

GetInfoByPathResult Platform::getHostFileInfo(const std::string& pathName, VFS::FileInfo& outInfo)
{
	struct stat fileStatus;

	if(stat(pathName.c_str(), &fileStatus))
	{
		switch(errno)
		{
		case EACCES: return GetInfoByPathResult::notPermitted;
		case EIO: return GetInfoByPathResult::ioError;
		case ENAMETOOLONG: return GetInfoByPathResult::nameTooLong;
		case ENOENT: return GetInfoByPathResult::doesNotExist;
		case ENOTDIR: return GetInfoByPathResult::pathUsesFileAsDirectory;

		case ELOOP:
			// A loop exists in symbolic links encountered during resolution of the path argument.
			// More than {SYMLOOP_MAX} symbolic links were encountered during resolution of the path
			// argument.

		case EOVERFLOW:
			// The file size in bytes or the number of blocks allocated to the file or the file
			// serial number cannot be represented correctly in the structure pointed to by buf.

		default: Errors::fatalf("Unexpected errno: %s", strerror(errno));
		};
	}

	getFileInfoFromStatus(fileStatus, outInfo);
	return GetInfoByPathResult::success;
}

std::string Platform::getCurrentWorkingDirectory()
{
	const Uptr maxPathBytes = pathconf(".", _PC_PATH_MAX);
	char* buffer = (char*)alloca(maxPathBytes);
	errorUnless(getcwd(buffer, maxPathBytes) == buffer);
	return std::string(buffer);
}
