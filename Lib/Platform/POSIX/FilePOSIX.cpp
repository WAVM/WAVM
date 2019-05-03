#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/File.h"

using namespace WAVM;
using namespace WAVM::Platform;

// Instead of just reinterpreting the file descriptor as a pointer, use -fd - 1, which maps fd=0 to
// a non-null value, and fd=-1 to null.
static I32 filePtrToIndex(File* ptr) { return I32(-reinterpret_cast<Iptr>(ptr) - 1); }

static File* fileIndexToPtr(int index) { return reinterpret_cast<File*>(-Iptr(index) - 1); }

File* Platform::openFile(const std::string& pathName,
						 FileAccessMode accessMode,
						 FileCreateMode createMode)
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

	const I32 result = open(pathName.c_str(), flags, mode);
	return fileIndexToPtr(result);
}

bool Platform::closeFile(File* file) { return close(filePtrToIndex(file)) == 0; }

File* Platform::getStdFile(StdDevice device)
{
	switch(device)
	{
	case StdDevice::in: return fileIndexToPtr(0);
	case StdDevice::out: return fileIndexToPtr(1);
	case StdDevice::err: return fileIndexToPtr(2);
	default: Errors::unreachable();
	};
}

bool Platform::seekFile(File* file, I64 offset, FileSeekOrigin origin, U64* outAbsoluteOffset)
{
	I32 whence = 0;
	switch(origin)
	{
	case FileSeekOrigin::begin: whence = SEEK_SET; break;
	case FileSeekOrigin::cur: whence = SEEK_CUR; break;
	case FileSeekOrigin::end: whence = SEEK_END; break;
	default: Errors::unreachable();
	};

#ifdef __linux__
	const I64 result = lseek64(filePtrToIndex(file), offset, whence);
#else
	const I64 result = lseek(filePtrToIndex(file), reinterpret_cast<off_t>(offset), whence);
#endif
	if(outAbsoluteOffset) { *outAbsoluteOffset = U64(result); }
	return result != -1;
}

bool Platform::readFile(File* file, void* outData, Uptr numBytes, Uptr* outNumBytesRead)
{
	ssize_t result = read(filePtrToIndex(file), outData, numBytes);
	if(outNumBytesRead) { *outNumBytesRead = result; }
	return result >= 0;
}

bool Platform::writeFile(File* file, const void* data, Uptr numBytes, Uptr* outNumBytesWritten)
{
	ssize_t result = write(filePtrToIndex(file), data, numBytes);
	if(outNumBytesWritten) { *outNumBytesWritten = result; }
	return result >= 0;
}

bool Platform::flushFileWrites(File* file) { return fsync(filePtrToIndex(file)) == 0; }

std::string Platform::getCurrentWorkingDirectory()
{
	const Uptr maxPathBytes = pathconf(".", _PC_PATH_MAX);
	char* buffer = (char*)alloca(maxPathBytes);
	errorUnless(getcwd(buffer, maxPathBytes) == buffer);
	return std::string(buffer);
}
