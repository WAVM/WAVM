#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
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

static_assert(offsetof(struct iovec, iov_base) == offsetof(IOReadBuffer, data)
				  && offsetof(struct iovec, iov_len) == offsetof(IOReadBuffer, numBytes)
				  && sizeof(((struct iovec*)nullptr)->iov_base) == sizeof(IOReadBuffer::data)
				  && sizeof(((struct iovec*)nullptr)->iov_len) == sizeof(IOReadBuffer::numBytes)
				  && sizeof(struct iovec) == sizeof(IOReadBuffer),
			  "IOReadBuffer must match iovec");

static_assert(offsetof(struct iovec, iov_base) == offsetof(IOWriteBuffer, data)
				  && offsetof(struct iovec, iov_len) == offsetof(IOWriteBuffer, numBytes)
				  && sizeof(((struct iovec*)nullptr)->iov_base) == sizeof(IOWriteBuffer::data)
				  && sizeof(((struct iovec*)nullptr)->iov_len) == sizeof(IOWriteBuffer::numBytes)
				  && sizeof(struct iovec) == sizeof(IOWriteBuffer),
			  "IOWriteBuffer must match iovec");

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
		WAVM_UNREACHABLE();
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

struct POSIXDirEntStream : DirEntStream
{
	POSIXDirEntStream(DIR* inDir) : dir(inDir) {}

	virtual void close() override
	{
		closedir(dir);
		delete this;
	}

	virtual bool getNext(DirEnt& outEntry) override
	{
		errno = 0;
		struct dirent* dirent = readdir(dir);
		if(dirent)
		{
			wavmAssert(dirent);
			outEntry.fileNumber = dirent->d_ino;
			outEntry.name = dirent->d_name;
#ifdef _DIRENT_HAVE_D_TYPE
			switch(dirent->d_type)
			{
			case DT_BLK: outEntry.type = FileType::blockDevice; break;
			case DT_CHR: outEntry.type = FileType::characterDevice; break;
			case DT_DIR: outEntry.type = FileType::directory; break;
			case DT_FIFO: outEntry.type = FileType::pipe; break;
			case DT_LNK: outEntry.type = FileType::symbolicLink; break;
			case DT_REG: outEntry.type = FileType::file; break;
			case DT_SOCK: outEntry.type = FileType::streamSocket; break;
			case DT_UNKNOWN: outEntry.type = FileType::unknown; break;

			default: WAVM_UNREACHABLE();
			};
#else
			outEntry.type = FileType::unknown;
#endif
			return true;
		}
		else
		{
			switch(errno)
			{
			case 0:
				// Reached the end of the directory.
				return false;

			case EBADF:
				Errors::fatalf("readdir returned EBADF; dir=0x%" PRIxPTR ", dirfd(dir)=%i",
							   reinterpret_cast<Uptr>(dir),
							   dirfd(dir));

			case ENOENT:
				// "The current position of the directory stream is invalid."
				return false;

			case EOVERFLOW:
				// "One of the values in the structure to be returned cannot be represented
				// correctly."
				return false;

			default: WAVM_UNREACHABLE();
			};
		}
	}

	virtual void restart() override
	{
		rewinddir(dir);
		maxValidOffset = 0;
	}

	virtual U64 tell() override
	{
		const long offset = telldir(dir);
		errorUnless(offset >= 0 && (unsigned long)offset <= UINT64_MAX);
		if(U64(offset) > maxValidOffset) { maxValidOffset = U64(offset); }
		return U64(offset);
	}

	virtual bool seek(U64 offset) override
	{
		// Don't allow seeking to higher offsets than have been returned by tell since the last
		// rewind.
		if(offset > maxValidOffset) { return false; };

		errorUnless(offset <= LONG_MAX);
		seekdir(dir, long(offset));
		return true;
	}

private:
	DIR* dir;
	U64 maxValidOffset{0};
};

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

			case EINTR:
				// POSIX close says that the fd is in an undefined state after close returns EINTR.
				// This risks leaking the fd, but assume that the interrupted close actually closed
				// the fd and return success.
				// https://www.daemonology.net/blog/2011-12-17-POSIX-close-is-broken.html
				break;

			case EIO: return CloseResult::ioError;

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
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
		default: WAVM_UNREACHABLE();
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

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
			}
		}
	}
	virtual ReadResult readv(const IOReadBuffer* buffers,
							 Uptr numBuffers,
							 Uptr* outNumBytesRead = nullptr) override
	{
		if(outNumBytesRead) { *outNumBytesRead = 0; }

		if(numBuffers == 0) { return ReadResult::success; }
		else if(numBuffers > IOV_MAX)
		{
			return ReadResult::tooManyBuffers;
		}

		// Do the read.
		ssize_t result = ::readv(fd, (const struct iovec*)buffers, numBuffers);
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
			case EFAULT: return ReadResult::badAddress;

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
			};
		}
	}
	virtual WriteResult writev(const IOWriteBuffer* buffers,
							   Uptr numBuffers,
							   Uptr* outNumBytesWritten = nullptr) override
	{
		if(outNumBytesWritten) { *outNumBytesWritten = 0; }

		if(numBuffers == 0) { return WriteResult::success; }
		else if(numBuffers > IOV_MAX)
		{
			return WriteResult::tooManyBuffers;
		}

		ssize_t result = ::writev(fd, (const struct iovec*)buffers, numBuffers);
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
			case EFAULT: return WriteResult::badAddress;

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
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
		default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
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

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
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

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
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

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
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

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
			};
		}

		getFileInfoFromStatus(fdStatus, outInfo);
		return GetInfoResult::success;
	}

	virtual OpenDirResult openDir(DirEntStream*& outStream) override
	{
		const I32 duplicateFD = dup(fd);
		if(duplicateFD < 0)
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("dup returned EBADF (fd=%i)", fd);
			case EMFILE: return OpenDirResult::outOfMemory;

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
			};
		}

		DIR* dir = fdopendir(duplicateFD);
		if(dir)
		{
			// Rewind the dir to the beginning to ensure previous seeks on the FD don't affect the
			// dirent stream.
			rewinddir(dir);

			outStream = new POSIXDirEntStream(dir);
			return OpenDirResult::success;
		}
		else
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("fdopendir returned EBADF (fd=%i)", fd);
			case ENOTDIR: return OpenDirResult::notDirectory;

			default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
			};
		}
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
	default: WAVM_UNREACHABLE();
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
	default: WAVM_UNREACHABLE();
	};

	switch(createMode)
	{
	case FileCreateMode::createAlways: flags |= O_CREAT | O_TRUNC; break;
	case FileCreateMode::createNew: flags |= O_CREAT | O_EXCL; break;
	case FileCreateMode::openAlways: flags |= O_CREAT; break;
	case FileCreateMode::openExisting: break;
	case FileCreateMode::truncateExisting: flags |= O_TRUNC; break;
	default: WAVM_UNREACHABLE();
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

	default: WAVM_UNREACHABLE();
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
		case ENOTDIR: return OpenResult::pathPrefixNotDirectory;
		case EROFS: return OpenResult::notPermitted;
		case ENOMEM: return OpenResult::outOfMemory;
		case EDQUOT: return OpenResult::outOfQuota;
		case EPERM: return OpenResult::notPermitted;
		case ELOOP: return OpenResult::tooManyLinks;

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

		default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
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
		case ENOTDIR: return GetInfoByPathResult::pathPrefixNotDirectory;
		case ELOOP: return GetInfoByPathResult::tooManyLinks;

		case EOVERFLOW:
			// The file size in bytes or the number of blocks allocated to the file or the file
			// serial number cannot be represented correctly in the structure pointed to by buf.

		default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
		};
	}

	getFileInfoFromStatus(fileStatus, outInfo);
	return GetInfoByPathResult::success;
}

OpenDirByPathResult Platform::openHostDir(const std::string& pathName, DirEntStream*& outStream)
{
	DIR* dir = opendir(pathName.c_str());

	if(dir)
	{
		outStream = new POSIXDirEntStream(dir);
		return OpenDirByPathResult::success;
	}
	else
	{
		switch(errno)
		{
		case EACCES: return OpenDirByPathResult::notPermitted;

		case ELOOP: return OpenDirByPathResult::tooManyLinks;
		case ENAMETOOLONG: return OpenDirByPathResult::nameTooLong;
		case ENOTDIR: return OpenDirByPathResult::pathPrefixNotDirectory;
		case ENOENT: return OpenDirByPathResult::notDirectory;

		case EMFILE: return OpenDirByPathResult::outOfMemory;
		case ENFILE: return OpenDirByPathResult::outOfMemory;

		default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
		};
	}
}

UnlinkFileResult Platform::unlinkHostFile(const std::string& pathName)
{
	if(!unlink(pathName.c_str())) { return UnlinkFileResult::success; }
	else
	{
		switch(errno)
		{
		case EACCES: return UnlinkFileResult::notPermitted;
		case EBUSY: return UnlinkFileResult::notPermitted;
		case ELOOP: return UnlinkFileResult::tooManyLinks;
		case ENAMETOOLONG: return UnlinkFileResult::nameTooLong;
		case ENOENT: return UnlinkFileResult::doesNotExist;
		case ENOTDIR: return UnlinkFileResult::pathPrefixNotDirectory;
		case EPERM: return UnlinkFileResult::notPermitted;
		case EROFS: return UnlinkFileResult::notPermitted;
		case EISDIR: return UnlinkFileResult::isDirectory;

		case ETXTBSY:
			// "The entry to be unlinked is the last directory entry to a pure procedure (shared
			// text) file that is being executed."
		default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
		};
	}
}

RemoveDirResult Platform::removeHostDir(const std::string& pathName)
{
	if(!unlinkat(AT_FDCWD, pathName.c_str(), AT_REMOVEDIR)) { return RemoveDirResult::success; }
	else
	{
		switch(errno)
		{
		case EACCES: return RemoveDirResult::notPermitted;
		case EBUSY: return RemoveDirResult::notPermitted;
		case ELOOP: return RemoveDirResult::tooManyLinks;
		case ENAMETOOLONG: return RemoveDirResult::nameTooLong;
		case ENOENT: return RemoveDirResult::doesNotExist;
		case ENOTDIR: return RemoveDirResult::notDirectory;
		case EPERM: return RemoveDirResult::notPermitted;
		case EROFS: return RemoveDirResult::notPermitted;
		case ENOTEMPTY: return RemoveDirResult::notEmpty;

		case ETXTBSY:
			// "The entry to be unlinked is the last directory entry to a pure procedure (shared
			// text) file that is being executed."
		default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
		};
	}
}

CreateDirResult Platform::createHostDir(const std::string& pathName)
{
	if(!mkdir(pathName.c_str(), 0666)) { return CreateDirResult::success; }
	else
	{
		switch(errno)
		{
		case EACCES: return CreateDirResult::notPermitted;
		case EEXIST: return CreateDirResult::alreadyExists;
		case ELOOP: return CreateDirResult::tooManyLinks;
		case EMLINK: return CreateDirResult::outOfLinksToParentDir;
		case ENAMETOOLONG: return CreateDirResult::nameTooLong;
		case ENOENT: return CreateDirResult::pathPrefixDoesNotExist;
		case ENOSPC: return CreateDirResult::outOfFreeSpace;
		case EDQUOT: return CreateDirResult::outOfQuota;
		case ENOTDIR: return CreateDirResult::outOfLinksToParentDir;
		case EROFS: return CreateDirResult::notPermitted;

		default: Errors::fatalfWithCallStack("Unexpected errno: %s", strerror(errno));
		};
	}
}

std::string Platform::getCurrentWorkingDirectory()
{
	const Uptr maxPathBytes = pathconf(".", _PC_PATH_MAX);
	char* buffer = (char*)alloca(maxPathBytes);
	errorUnless(getcwd(buffer, maxPathBytes) == buffer);
	return std::string(buffer);
}
