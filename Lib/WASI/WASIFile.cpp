#include "./WASIPrivate.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Time.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/RWMutex.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/VFS/VFS.h"
#include "WAVM/WASI/WASIABI64.h"

using namespace WAVM;
using namespace WAVM::WASI;
using namespace WAVM::Runtime;
using namespace WAVM::VFS;

namespace WAVM { namespace WASI {
	WAVM_DEFINE_INTRINSIC_MODULE(wasiFile)
}}

static __wasi_errno_t asWASIErrNo(VFS::Result result)
{
	switch(result)
	{
	case Result::success: return __WASI_ESUCCESS;
	case Result::ioPending: return __WASI_EINPROGRESS;
	case Result::ioDeviceError: return __WASI_EIO;
	case Result::interruptedBySignal: return __WASI_EINTR;
	case Result::interruptedByCancellation: return __WASI_EINTR;
	case Result::wouldBlock: return __WASI_EAGAIN;
	case Result::inaccessibleBuffer: return __WASI_EFAULT;
	case Result::invalidOffset: return __WASI_EINVAL;
	case Result::notSeekable: return __WASI_ESPIPE;
	case Result::notPermitted: return __WASI_EPERM;
	case Result::notAccessible: return __WASI_EACCES;
	case Result::notSynchronizable: return __WASI_EINVAL;
	case Result::tooManyBufferBytes: return __WASI_EINVAL;
	case Result::notEnoughBufferBytes: return __WASI_EINVAL;
	case Result::tooManyBuffers: return __WASI_EINVAL;
	case Result::notEnoughBits: return __WASI_EOVERFLOW;
	case Result::exceededFileSizeLimit: return __WASI_EFBIG;
	case Result::outOfSystemFDs: return __WASI_ENFILE;
	case Result::outOfProcessFDs: return __WASI_EMFILE;
	case Result::outOfMemory: return __WASI_ENOMEM;
	case Result::outOfQuota: return __WASI_EDQUOT;
	case Result::outOfFreeSpace: return __WASI_ENOSPC;
	case Result::outOfLinksToParentDir: return __WASI_EMLINK;
	case Result::invalidNameCharacter: return __WASI_EACCES;
	case Result::nameTooLong: return __WASI_ENAMETOOLONG;
	case Result::tooManyLinksInPath: return __WASI_ELOOP;
	case Result::alreadyExists: return __WASI_EEXIST;
	case Result::doesNotExist: return __WASI_ENOENT;
	case Result::isDirectory: return __WASI_EISDIR;
	case Result::isNotDirectory: return __WASI_ENOTDIR;
	case Result::isNotEmpty: return __WASI_ENOTEMPTY;
	case Result::brokenPipe: return __WASI_EPIPE;
	case Result::missingDevice: return __WASI_ENXIO;
	case Result::busy: return __WASI_EBUSY;
	case Result::notSupported: return __WASI_ENOTSUP;

	default: WAVM_UNREACHABLE();
	};
}

struct LockedFDE
{
	__wasi_errno_t error;

	// Only set if result==_WASI_ESUCCESS:
	Platform::RWMutex::Lock fdeLock;
	std::shared_ptr<FDE> fde;

	LockedFDE(__wasi_errno_t inError) : error(inError) {}
	LockedFDE(const std::shared_ptr<FDE>& inFDE,
			  Platform::RWMutex::LockShareability lockShareability)
	: error(__WASI_ESUCCESS), fdeLock(inFDE->mutex, lockShareability), fde(inFDE)
	{
	}
};

static LockedFDE getLockedFDE(Process* process,
							  __wasi_fd_t fd,
							  __wasi_rights_t requiredRights,
							  __wasi_rights_t requiredInheritingRights,
							  Platform::RWMutex::LockShareability lockShareability
							  = Platform::RWMutex::shareable)
{
	// Shareably lock the fdMap mutex.
	Platform::RWMutex::ShareableLock fdsLock(process->fdMapMutex);

	// Check that the fdMap contains a FDE for the given FD.
	if(fd < process->fdMap.getMinIndex() || fd > process->fdMap.getMaxIndex())
	{
		return LockedFDE(__WASI_EBADF);
	}
	std::shared_ptr<FDE>* fde = process->fdMap.get(fd);
	if(!fde) { return LockedFDE(__WASI_EBADF); }

	// Check that the FDE has the required rights.
	if(((*fde)->rights & requiredRights) != requiredRights
	   || ((*fde)->inheritingRights & requiredInheritingRights) != requiredInheritingRights)
	{
		return LockedFDE(__WASI_ENOTCAPABLE);
	}

	TRACE_SYSCALL_FLOW("Locked FDE: %s", (*fde)->originalPath.c_str());

	// Lock the FDE and return a reference to it.
	return LockedFDE(*fde, lockShareability);
}

static __wasi_filetype_t asWASIFileType(FileType type)
{
	switch(type)
	{
	case FileType::unknown: return __WASI_FILETYPE_UNKNOWN;
	case FileType::blockDevice: return __WASI_FILETYPE_BLOCK_DEVICE;
	case FileType::characterDevice: return __WASI_FILETYPE_CHARACTER_DEVICE;
	case FileType::directory: return __WASI_FILETYPE_DIRECTORY;
	case FileType::file: return __WASI_FILETYPE_REGULAR_FILE;
	case FileType::datagramSocket: return __WASI_FILETYPE_SOCKET_DGRAM;
	case FileType::streamSocket: return __WASI_FILETYPE_SOCKET_STREAM;
	case FileType::symbolicLink: return __WASI_FILETYPE_SYMBOLIC_LINK;
	case FileType::pipe: return __WASI_FILETYPE_UNKNOWN;

	default: WAVM_UNREACHABLE();
	};
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
		while(componentStart < relativePath.size() && relativePath[componentStart] == '/')
		{
			++componentStart;
		}

		Uptr nextPathSeparator = relativePath.find_first_of('/', componentStart);

		if(nextPathSeparator == std::string::npos) { nextPathSeparator = relativePath.size(); }

		if(nextPathSeparator != componentStart)
		{
			std::string component
				= relativePath.substr(componentStart, nextPathSeparator - componentStart);

			if(component == "..")
			{
				if(!relativePathComponents.size()) { return false; }
				else { relativePathComponents.pop_back(); }
			}
			else if(component != ".") { relativePathComponents.push_back(component); }

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

WASI::FDE::~FDE()
{
	// The FDE should have been closed before it is destroyed.
	WAVM_ERROR_UNLESS(!vfd);
}

Result WASI::FDE::close()
{
	WAVM_ASSERT(vfd);

	Result result = vfd->close();
	vfd = nullptr;

	if(dirEntStream)
	{
		dirEntStream->close();
		dirEntStream = nullptr;
	}

	return result;
}

static std::string describeSeekWhence(U32 whence)
{
	switch(whence)
	{
	case __WASI_WHENCE_CUR: return "__WASI_WHENCE_CUR"; break;
	case __WASI_WHENCE_END: return "__WASI_WHENCE_END"; break;
	case __WASI_WHENCE_SET: return "__WASI_WHENCE_SET"; break;
	default: return std::to_string(whence);
	};
}

static VFDFlags translateWASIVFDFlags(__wasi_fdflags_t fdFlags, __wasi_rights_t& outRequiredRights)
{
	VFDFlags result;
	if(fdFlags & __WASI_FDFLAG_DSYNC)
	{
		result.syncLevel = (fdFlags & __WASI_FDFLAG_RSYNC)
							   ? VFDSync::contentsAfterWriteAndBeforeRead
							   : VFDSync::contentsAfterWrite;
		outRequiredRights |= __WASI_RIGHT_FD_DATASYNC;
	}
	if(fdFlags & __WASI_FDFLAG_SYNC)
	{
		result.syncLevel = (fdFlags & __WASI_FDFLAG_RSYNC)
							   ? VFDSync::contentsAndMetadataAfterWriteAndBeforeRead
							   : VFDSync::contentsAndMetadataAfterWrite;
		outRequiredRights |= __WASI_RIGHT_FD_SYNC;
	}
	if(fdFlags & __WASI_FDFLAG_NONBLOCK) { result.nonBlocking = true; }
	if(fdFlags & __WASI_FDFLAG_APPEND) { result.append = true; }
	return result;
}

static Uptr truncatingMemcpy(void* dest, const void* source, Uptr numSourceBytes, Uptr numDestBytes)
{
	Uptr numBytes = numSourceBytes;
	if(numBytes > numDestBytes) { numBytes = numDestBytes; }
	if(numBytes > 0) { memcpy(dest, source, numBytes); }
	return numBytes;
}

#include "DefineIntrinsicsI32.h"
#include "WASIFile.h"
#if UINT32_MAX < SIZE_MAX
#include "DefineIntrinsicsI64.h"
#include "WASIFile.h"
#endif
