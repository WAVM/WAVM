#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Inline/Unicode.h"
#include "WAVM/Platform/Event.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"
#include "WindowsPrivate.h"

#define NOMINMAX
#include <Windows.h>

using namespace WAVM;
using namespace WAVM::Platform;
using namespace WAVM::VFS;

static GetInfoResult getFileType(HANDLE handle, FileType& outType)
{
	const DWORD windowsFileType = GetFileType(handle);
	if(windowsFileType == FILE_TYPE_UNKNOWN)
	{
		switch(GetLastError())
		{
		case ERROR_SUCCESS: break;

		case ERROR_INVALID_HANDLE:
			Errors::fatalf("GetFileType returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR ")",
						   reinterpret_cast<Uptr>(handle));

		default: WAVM_UNREACHABLE();
		}
	}

	switch(windowsFileType)
	{
	case FILE_TYPE_CHAR: outType = FileType::characterDevice; break;
	case FILE_TYPE_PIPE: outType = FileType::pipe; break;
	case FILE_TYPE_DISK:
	{
		FILE_BASIC_INFO fileBasicInfo;
		if(!GetFileInformationByHandleEx(
			   handle, FileBasicInfo, &fileBasicInfo, sizeof(fileBasicInfo)))
		{
			switch(GetLastError())
			{
			case ERROR_INVALID_HANDLE:
				Errors::fatalf(
					"GetFileInformationByHandleEx returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR
					")",
					reinterpret_cast<Uptr>(handle));

			default: WAVM_UNREACHABLE();
			};
		}

		outType = fileBasicInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY ? FileType::directory
																		  : FileType::file;
		break;
	}

	default: outType = FileType::unknown;
	};

	return GetInfoResult::success;
}

static GetInfoResult getFileInfoByHandle(HANDLE handle, FileInfo& outInfo)
{
	BY_HANDLE_FILE_INFORMATION windowsFileInfo;
	if(!GetFileInformationByHandle(handle, &windowsFileInfo))
	{
		switch(GetLastError())
		{
		case ERROR_INVALID_HANDLE:
			Errors::fatalf(
				"GetFileInformationByHandleEx returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR ")",
				reinterpret_cast<Uptr>(handle));

		default: WAVM_UNREACHABLE();
		};
	}

	outInfo.deviceNumber = windowsFileInfo.dwVolumeSerialNumber;
	outInfo.fileNumber
		= windowsFileInfo.nFileIndexLow | (U64(windowsFileInfo.nFileIndexHigh) << 32);

	const GetInfoResult getTypeResult = getFileType(handle, outInfo.type);
	if(getTypeResult != GetInfoResult::success) { return getTypeResult; }

	outInfo.numLinks = windowsFileInfo.nNumberOfLinks;
	outInfo.numBytes = windowsFileInfo.nFileSizeLow | (U64(windowsFileInfo.nFileSizeHigh) << 32);

	outInfo.lastAccessTime = fileTimeToWAVMRealTime(windowsFileInfo.ftLastAccessTime);
	outInfo.lastWriteTime = fileTimeToWAVMRealTime(windowsFileInfo.ftLastWriteTime);
	outInfo.creationTime = fileTimeToWAVMRealTime(windowsFileInfo.ftCreationTime);

	return GetInfoResult::success;
}

struct WindowsFD : FD
{
	WindowsFD(HANDLE inHandle, FDImplicitSync inImplicitSync)
	: handle(inHandle), implicitSync(inImplicitSync)
	{
		wavmAssert(handle != INVALID_HANDLE_VALUE);
	}

	virtual CloseResult close() override
	{
		if(CloseHandle(handle))
		{
			delete this;
			return CloseResult::success;
		}
		else
		{
			switch(GetLastError())
			{
			case ERROR_INVALID_HANDLE:
				Errors::fatalf("CloseHandle returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR ")",
							   reinterpret_cast<Uptr>(handle));

			default: WAVM_UNREACHABLE();
			}
		}
	}

	virtual SeekResult seek(I64 offset,
							SeekOrigin origin,
							U64* outAbsoluteOffset = nullptr) override
	{
		DWORD windowsOrigin;
		switch(origin)
		{
		case SeekOrigin::begin: windowsOrigin = FILE_BEGIN; break;
		case SeekOrigin::cur: windowsOrigin = FILE_CURRENT; break;
		case SeekOrigin::end: windowsOrigin = FILE_END; break;
		default: WAVM_UNREACHABLE();
		}

		LONG offsetHigh = LONG((offset >> 32) & 0xffffffff);
		LONG result = SetFilePointer(handle, LONG(offset & 0xffffffff), &offsetHigh, windowsOrigin);
		if(result != INVALID_SET_FILE_POINTER)
		{
			if(outAbsoluteOffset) { *outAbsoluteOffset = (U64(offsetHigh) << 32) | result; }
			return SeekResult::success;
		}
		else
		{
			switch(GetLastError())
			{
			case ERROR_INVALID_HANDLE:
				Errors::fatalf("SetFilePointer returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR ")",
							   reinterpret_cast<Uptr>(handle));

			case ERROR_NEGATIVE_SEEK: return SeekResult::invalidOffset;

			case ERROR_INVALID_PARAMETER:
				// "If an application calls SetFilePointer with distance to move values that result
				// in a position not sector-aligned and a handle that is opened with
				// FILE_FLAG_NO_BUFFERING, the function fails, and GetLastError returns
				// ERROR_INVALID_PARAMETER."

			default: WAVM_UNREACHABLE();
			};
		}
	}
	virtual ReadResult read(void* outData, Uptr numBytes, Uptr* outNumBytesRead = nullptr) override
	{
		if(outNumBytesRead) { *outNumBytesRead = 0; }
		if(numBytes > Uptr(UINT32_MAX)) { return ReadResult::invalidArgument; }

		switch(implicitSync)
		{
		case FDImplicitSync::syncContentsAfterWriteAndBeforeRead:
		case FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead:
			FlushFileBuffers(handle);
			break;

		default: break;
		};

		DWORD windowsNumBytesRead = 0;
		if(ReadFile(handle, outData, U32(numBytes), &windowsNumBytesRead, nullptr))
		{
			if(outNumBytesRead) { *outNumBytesRead = Uptr(windowsNumBytesRead); }
			return ReadResult::success;
		}
		else
		{
			switch(GetLastError())
			{
			case ERROR_INVALID_HANDLE:
				Errors::fatalf("ReadFile returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR ")",
							   reinterpret_cast<Uptr>(handle));

			case ERROR_NOT_ENOUGH_QUOTA:
				// "The ReadFile function may fail with ERROR_NOT_ENOUGH_QUOTA, which means the
				// calling process's buffer could not be page-locked."
				return ReadResult::outOfMemory;

			case ERROR_ACCESS_DENIED: return ReadResult::notPermitted;
			case ERROR_IO_DEVICE: return ReadResult::ioError;

			case ERROR_IO_PENDING:
			case ERROR_OPERATION_ABORTED:
			case ERROR_NOT_ENOUGH_MEMORY:
			case ERROR_INVALID_USER_BUFFER:
				// Async I/O result codes that we shouldn't see.

			case ERROR_INSUFFICIENT_BUFFER:
				// "If ReadFile attempts to read from a mailslot that has a buffer that is too
				// small, the function returns FALSE and GetLastError returns
				// ERROR_INSUFFICIENT_BUFFER."

			case ERROR_BROKEN_PIPE:
				// "If an anonymous pipe is being used and the write handle has been closed, when
				// ReadFile attempts to read using the pipe's corresponding read handle, the
				// function returns FALSE and GetLastError returns ERROR_BROKEN_PIPE. "

			default: WAVM_UNREACHABLE();
			};
		}
	}
	virtual WriteResult write(const void* data,
							  Uptr numBytes,
							  Uptr* outNumBytesWritten = nullptr) override
	{
		if(outNumBytesWritten) { *outNumBytesWritten = 0; }
		if(numBytes > Uptr(UINT32_MAX)) { return WriteResult::invalidArgument; }

		DWORD windowsNumBytesWritten = 0;
		if(WriteFile(handle, data, U32(numBytes), &windowsNumBytesWritten, nullptr))
		{
			if(implicitSync != FDImplicitSync::none) { FlushFileBuffers(handle); }

			if(outNumBytesWritten) { *outNumBytesWritten = Uptr(windowsNumBytesWritten); }
			return WriteResult::success;
		}
		else
		{
			switch(GetLastError())
			{
			case ERROR_INVALID_HANDLE:
				Errors::fatalf("ReadFile returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR ")",
							   reinterpret_cast<Uptr>(handle));

			case ERROR_NOT_ENOUGH_QUOTA:
				// "The WriteFile function may fail with ERROR_NOT_ENOUGH_QUOTA, which means the
				// calling process's buffer could not be page-locked."
				return WriteResult::outOfMemory;

			case ERROR_ACCESS_DENIED: return WriteResult::notPermitted;
			case ERROR_IO_DEVICE: return WriteResult::ioError;

			case ERROR_IO_PENDING:
			case ERROR_OPERATION_ABORTED:
			case ERROR_NOT_ENOUGH_MEMORY:
			case ERROR_INVALID_USER_BUFFER:
				// Async I/O result codes that we shouldn't see.

			case ERROR_BROKEN_PIPE:
				// "If an anonymous pipe is being used and the read handle has been closed, when
				// WriteFile attempts to write using the pipe's corresponding write handle, the
				// function returns FALSE and GetLastError returns ERROR_BROKEN_PIPE."

			default: WAVM_UNREACHABLE();
			};
		}
	}
	virtual SyncResult sync(SyncType syncType) override
	{
		if(FlushFileBuffers(handle)) { return SyncResult::success; }
		else
		{
			switch(GetLastError())
			{
			case ERROR_INVALID_HANDLE:
				Errors::fatalf("FlushFileBuffers returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR
							   ")",
							   reinterpret_cast<Uptr>(handle));

			case ERROR_IO_DEVICE: return SyncResult::ioError;

			default: WAVM_UNREACHABLE();
			};
		}
	}

	virtual GetInfoResult getFDInfo(FDInfo& outInfo) override
	{
		const GetInfoResult getTypeResult = getFileType(handle, outInfo.type);
		if(getTypeResult != GetInfoResult::success) { return getTypeResult; }

		outInfo.append = false;
		outInfo.nonBlocking = false;
		outInfo.implicitSync = implicitSync;
		return GetInfoResult::success;
	}
	virtual GetInfoResult getFileInfo(FileInfo& outInfo) override
	{
		return getFileInfoByHandle(handle, outInfo);
	}

private:
	HANDLE handle;
	FDImplicitSync implicitSync;
};

struct WindowsStdFD : WindowsFD
{
	WindowsStdFD(HANDLE inHandle, FDImplicitSync inImplicitSync)
	: WindowsFD(inHandle, inImplicitSync)
	{
	}

	virtual CloseResult close()
	{
		// The stdio FDs are shared, so don't close them.
		return CloseResult::success;
	}
};

FD* Platform::getStdFD(StdDevice device)
{
	static WindowsStdFD* stdinVFD
		= new WindowsStdFD(GetStdHandle(STD_INPUT_HANDLE), FDImplicitSync::none);
	static WindowsStdFD* stdoutVFD
		= new WindowsStdFD(GetStdHandle(STD_OUTPUT_HANDLE), FDImplicitSync::none);
	static WindowsStdFD* stderrVFD
		= new WindowsStdFD(GetStdHandle(STD_ERROR_HANDLE), FDImplicitSync::none);

	switch(device)
	{
	case StdDevice::in: return stdinVFD;
	case StdDevice::out: return stdoutVFD;
	case StdDevice::err: return stderrVFD;
	default: WAVM_UNREACHABLE();
	};
}

OpenResult Platform::openHostFile(const std::string& pathName,
								  FileAccessMode accessMode,
								  FileCreateMode createMode,
								  FD*& outFD,
								  FDImplicitSync implicitSync)
{
	// Translate the path from UTF-8 to UTF-16.
	const U8* pathNameStart = (const U8*)pathName.c_str();
	const U8* pathNameEnd = pathNameStart + pathName.size();
	std::wstring pathNameW;
	if(Unicode::transcodeUTF8ToUTF16(pathNameStart, pathNameEnd, pathNameW) != pathNameEnd)
	{ return OpenResult::invalidNameCharacter; }

	// Map the VFS accessMode/createMode to the appropriate Windows CreateFile arguments.
	DWORD desiredAccess = 0;
	DWORD shareMode = FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE;
	DWORD creationDisposition = 0;
	DWORD flagsAndAttributes = FILE_FLAG_BACKUP_SEMANTICS;

	switch(accessMode)
	{
	case FileAccessMode::none: desiredAccess = 0; break;
	case FileAccessMode::readOnly: desiredAccess = GENERIC_READ; break;
	case FileAccessMode::writeOnly: desiredAccess = GENERIC_WRITE; break;
	case FileAccessMode::readWrite: desiredAccess = GENERIC_READ | GENERIC_WRITE; break;
	default: WAVM_UNREACHABLE();
	};

	switch(createMode)
	{
	case FileCreateMode::createAlways: creationDisposition = CREATE_ALWAYS; break;
	case FileCreateMode::createNew: creationDisposition = CREATE_NEW; break;
	case FileCreateMode::openAlways: creationDisposition = OPEN_ALWAYS; break;
	case FileCreateMode::openExisting: creationDisposition = OPEN_EXISTING; break;
	case FileCreateMode::truncateExisting: creationDisposition = TRUNCATE_EXISTING; break;
	default: WAVM_UNREACHABLE();
	};

	// Try to open the file.
	HANDLE handle = CreateFileW(pathNameW.c_str(),
								desiredAccess,
								shareMode,
								nullptr,
								creationDisposition,
								flagsAndAttributes,
								nullptr);
	if(handle != INVALID_HANDLE_VALUE)
	{
		outFD = new WindowsFD(handle, implicitSync);
		return OpenResult::success;
	}
	else
	{
		switch(GetLastError())
		{
		case ERROR_ACCESS_DENIED: return OpenResult::notPermitted;
		case ERROR_FILE_NOT_FOUND: return OpenResult::doesNotExist;
		case ERROR_IO_DEVICE: return OpenResult::ioError;
		case ERROR_ALREADY_EXISTS: return OpenResult::alreadyExists;

		default: WAVM_UNREACHABLE();
		}
	}
}

GetInfoByPathResult Platform::getHostFileInfo(const std::string& pathName, FileInfo& outInfo)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	const U8* pathNameStart = (const U8*)pathName.c_str();
	const U8* pathNameEnd = pathNameStart + pathName.size();
	if(Unicode::transcodeUTF8ToUTF16(pathNameStart, pathNameEnd, pathNameW) != pathNameEnd)
	{ return GetInfoByPathResult::invalidNameCharacter; }

	// Try to open the file with no access requested.
	HANDLE handle = CreateFileW(pathNameW.c_str(),
								0,
								FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
								nullptr,
								OPEN_EXISTING,
								0,
								nullptr);
	if(handle == INVALID_HANDLE_VALUE)
	{
		switch(GetLastError())
		{
		case ERROR_ACCESS_DENIED: return GetInfoByPathResult::notPermitted;
		case ERROR_FILE_NOT_FOUND: return GetInfoByPathResult::doesNotExist;
		case ERROR_IO_DEVICE: return GetInfoByPathResult::ioError;

		default: WAVM_UNREACHABLE();
		}
	}

	GetInfoResult getInfoResult = getFileInfoByHandle(handle, outInfo);
	switch(getInfoResult)
	{
	case GetInfoResult::success: break;
	case GetInfoResult::ioError: return GetInfoByPathResult::ioError;

	default: WAVM_UNREACHABLE();
	};

	if(!CloseHandle(handle))
	{ Errors::fatalf("CloseHandle failed: GetLastError()=%u", GetLastError()); }

	return GetInfoByPathResult::success;
}

std::string Platform::getCurrentWorkingDirectory()
{
	U16 buffer[MAX_PATH];
	const DWORD numChars = GetCurrentDirectoryW(MAX_PATH, (LPWSTR)buffer);
	errorUnless(numChars);

	std::string result;
	const U16* transcodeEnd = Unicode::transcodeUTF16ToUTF8(buffer, buffer + numChars, result);
	errorUnless(transcodeEnd == buffer + numChars);

	return result;
}
