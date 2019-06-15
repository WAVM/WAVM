#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Unicode.h"
#include "WAVM/Platform/Event.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"

#define NOMINMAX
#include <Windows.h>

using namespace WAVM;
using namespace WAVM::Platform;
using namespace WAVM::VFS;

struct WindowsFD : FD
{
	WindowsFD(HANDLE inHandle, FDType inType, FDImplicitSync inImplicitSync)
	: handle(inHandle), type(inType), implicitSync(inImplicitSync)
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

			default: Errors::unreachable();
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
		default: Errors::unreachable();
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

			default: Errors::unreachable();
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

			default: Errors::unreachable();
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

			default: Errors::unreachable();
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

			default: Errors::unreachable();
			};
		}
	}
	virtual GetFDInfoResult getInfo(FDInfo& outInfo) override
	{
		outInfo.type = type;
		outInfo.flags.append = false;
		outInfo.flags.nonBlocking = false;
		outInfo.flags.implicitSync = implicitSync;
		return GetFDInfoResult::success;
	}

private:
	HANDLE handle;
	FDType type;
	FDImplicitSync implicitSync;
};

FD* Platform::getStdFD(StdDevice device)
{
	DWORD StdHandle = 0;
	switch(device)
	{
	case StdDevice::in: StdHandle = STD_INPUT_HANDLE; break;
	case StdDevice::out: StdHandle = STD_OUTPUT_HANDLE; break;
	case StdDevice::err: StdHandle = STD_ERROR_HANDLE; break;
	default: Errors::unreachable();
	};
	return new WindowsFD(GetStdHandle(StdHandle), FDType::characterDevice, FDImplicitSync::none);
}

FD* Platform::openHostFile(const std::string& pathName,
						   FileAccessMode accessMode,
						   FileCreateMode createMode,
						   FDImplicitSync implicitSync)
{
	DWORD desiredAccess = 0;
	DWORD shareMode = 0;
	DWORD creationDisposition = 0;
	DWORD flagsAndAttributes = 0;

	switch(accessMode)
	{
	case FileAccessMode::readOnly: desiredAccess = GENERIC_READ; break;
	case FileAccessMode::writeOnly: desiredAccess = GENERIC_WRITE; break;
	case FileAccessMode::readWrite: desiredAccess = GENERIC_READ | GENERIC_WRITE; break;
	default: Errors::unreachable();
	};

	switch(createMode)
	{
	case FileCreateMode::createAlways: creationDisposition = CREATE_ALWAYS; break;
	case FileCreateMode::createNew: creationDisposition = CREATE_NEW; break;
	case FileCreateMode::openAlways: creationDisposition = OPEN_ALWAYS; break;
	case FileCreateMode::openExisting: creationDisposition = OPEN_EXISTING; break;
	case FileCreateMode::truncateExisting: creationDisposition = TRUNCATE_EXISTING; break;
	default: Errors::unreachable();
	};

	const U8* pathNameStart = (const U8*)pathName.c_str();
	const U8* pathNameEnd = pathNameStart + pathName.size();
	std::wstring pathNameW;
	if(Unicode::transcodeUTF8ToUTF16(pathNameStart, pathNameEnd, pathNameW) != pathNameEnd)
	{ return nullptr; }

	HANDLE handle = CreateFileW(pathNameW.c_str(),
								desiredAccess,
								shareMode,
								nullptr,
								creationDisposition,
								flagsAndAttributes,
								nullptr);

	return handle == INVALID_HANDLE_VALUE ? nullptr
										  : new WindowsFD(handle, FDType::file, implicitSync);
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
