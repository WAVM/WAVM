#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"

using namespace WAVM;
using namespace WAVM::Platform;
using namespace WAVM::VFS;

struct POSIXFD : FD
{
	const I32 fd;
	const FDFlags flags;

	POSIXFD(I32 inFD, FDFlags inFlags) : fd(inFD), flags(inFlags) {}

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
			case ESPIPE: return SeekResult::notSupported;

			default: Errors::unreachable();
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

			default: Errors::unreachable();
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

			default: Errors::unreachable();
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
		default: Errors::unreachable();
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

			default: Errors::unreachable();
			};
		}
	}
	virtual GetFDInfoResult getInfo(FDInfo& outInfo) override
	{
		struct stat fdStatus;
		if(fstat(fd, &fdStatus) != 0)
		{
			switch(errno)
			{
			case EBADF: Errors::fatalf("fstat return EBADF (fd=%i)", fd);

			case EIO: return GetFDInfoResult::ioError;
			case EOVERFLOW:
				// EOVERFLOW: The file size in bytes or the number of blocks allocated to the file
				// or the file serial number cannot be represented correctly in the structure
				// pointed to by buf.
				Errors::unreachable();

			default: Errors::unreachable();
			};
		}

		switch(fdStatus.st_mode & S_IFMT)
		{
		case S_IFBLK: outInfo.type = FDType::blockDevice; break;
		case S_IFCHR: outInfo.type = FDType::characterDevice; break;
		case S_IFREG: outInfo.type = FDType::file; break;
		case S_IFDIR: outInfo.type = FDType::directory; break;
		case S_IFLNK: outInfo.type = FDType::symbolicLink; break;
		case S_IFSOCK:
			// outInfo.type = FDType::datagramSocket;
			// outInfo.type = FDType::streamSocket;
			Errors::unreachable();
			break;
		default: outInfo.type = FDType::unknown;
		};

		outInfo.flags = flags;

		return GetFDInfoResult::success;
	}
};

FD* Platform::getStdFD(StdDevice device)
{
	I32 fd;
	switch(device)
	{
	case StdDevice::in: fd = 0; break;
	case StdDevice::out: fd = 1; break;
	case StdDevice::err: fd = 2; break;
	default: Errors::unreachable();
	};
	return new POSIXFD(fd, FDFlags{false, false, FDImplicitSync::none});
}

FD* Platform::openHostFile(const std::string& pathName,
						   FileAccessMode accessMode,
						   FileCreateMode createMode,
						   FDImplicitSync implicitSync)
{
	U32 flags = 0;
	mode_t mode = 0;
	switch(accessMode)
	{
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

	switch(createMode)
	{
	case FileCreateMode::createAlways:
	case FileCreateMode::createNew:
	case FileCreateMode::openAlways: mode = S_IRWXU; break;
	default: break;
	};

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

	;

	return fd == -1 ? nullptr : new POSIXFD(fd, FDFlags{false, false, implicitSync});
}

std::string Platform::getCurrentWorkingDirectory()
{
	const Uptr maxPathBytes = pathconf(".", _PC_PATH_MAX);
	char* buffer = (char*)alloca(maxPathBytes);
	errorUnless(getcwd(buffer, maxPathBytes) == buffer);
	return std::string(buffer);
}
