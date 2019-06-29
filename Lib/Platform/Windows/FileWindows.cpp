#include <algorithm>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Inline/Unicode.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/Event.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"
#include "WindowsPrivate.h"

#define NOMINMAX
#include <Windows.h>

using namespace WAVM;
using namespace WAVM::Platform;
using namespace WAVM::VFS;

static Result asVFSResult(DWORD windowsError)
{
	switch(windowsError)
	{
	case ERROR_SUCCESS: return Result::success;

	case ERROR_READ_FAULT:
	case ERROR_WRITE_FAULT:
	case ERROR_NET_WRITE_FAULT:
	case ERROR_IO_DEVICE: return Result::ioDeviceError;

	case ERROR_PATH_NOT_FOUND:
	case ERROR_FILE_NOT_FOUND: return Result::doesNotExist;

	case ERROR_NEGATIVE_SEEK: return Result::invalidOffset;
	case ERROR_INVALID_FUNCTION: return Result::notPermitted;
	case ERROR_ACCESS_DENIED: return Result::notAccessible;
	case ERROR_IO_PENDING: return Result::ioPending;
	case ERROR_OPERATION_ABORTED: return Result::interruptedByCancellation;
	case ERROR_NOT_ENOUGH_MEMORY: return Result::outOfMemory;
	case ERROR_NOT_ENOUGH_QUOTA: return Result::outOfQuota;
	case ERROR_HANDLE_DISK_FULL: return Result::outOfFreeSpace;
	case ERROR_INVALID_USER_BUFFER: return Result::inaccessibleBuffer;
	case ERROR_INSUFFICIENT_BUFFER: return Result::notEnoughBufferBytes;
	case ERROR_BROKEN_PIPE: return Result::brokenPipe;
	case ERROR_ALREADY_EXISTS: return Result::alreadyExists;
	case ERROR_DIR_NOT_EMPTY: return Result::isNotEmpty;
	case ERROR_INVALID_ADDRESS: return Result::inaccessibleBuffer;
	case ERROR_DIRECTORY: return Result::isNotDirectory;

	case ERROR_INVALID_PARAMETER:
		// This probably needs to be handled differently for each API entry point.
		Errors::fatalfWithCallStack("ERROR_INVALID_PARAMETER");

	case ERROR_INVALID_HANDLE:
		// This must be a bug, so make it a fatal error.
		Errors::fatalfWithCallStack("ERROR_INVALID_HANDLE");

	default: Errors::fatalfWithCallStack("Unexpected windows error code: %u", GetLastError());
	};
}

static Result getFileType(HANDLE handle, FileType& outType)
{
	const DWORD windowsFileType = GetFileType(handle);
	if(windowsFileType == FILE_TYPE_UNKNOWN && GetLastError() != ERROR_SUCCESS)
	{ return asVFSResult(GetLastError()); }

	switch(windowsFileType)
	{
	case FILE_TYPE_CHAR: outType = FileType::characterDevice; break;
	case FILE_TYPE_PIPE: outType = FileType::pipe; break;
	case FILE_TYPE_DISK: {
		FILE_BASIC_INFO fileBasicInfo;
		if(!GetFileInformationByHandleEx(
			   handle, FileBasicInfo, &fileBasicInfo, sizeof(fileBasicInfo)))
		{ return asVFSResult(GetLastError()); }

		outType = fileBasicInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY ? FileType::directory
																		  : FileType::file;
		break;
	}

	default: outType = FileType::unknown;
	};

	return Result::success;
}

static Result getFileInfoByHandle(HANDLE handle, FileInfo& outInfo)
{
	BY_HANDLE_FILE_INFORMATION windowsFileInfo;
	if(!GetFileInformationByHandle(handle, &windowsFileInfo))
	{ return asVFSResult(GetLastError()); }

	outInfo.deviceNumber = windowsFileInfo.dwVolumeSerialNumber;
	outInfo.fileNumber
		= windowsFileInfo.nFileIndexLow | (U64(windowsFileInfo.nFileIndexHigh) << 32);

	const Result getTypeResult = getFileType(handle, outInfo.type);
	if(getTypeResult != Result::success) { return getTypeResult; }

	outInfo.numLinks = windowsFileInfo.nNumberOfLinks;
	outInfo.numBytes = windowsFileInfo.nFileSizeLow | (U64(windowsFileInfo.nFileSizeHigh) << 32);

	outInfo.lastAccessTime = fileTimeToWAVMRealTime(windowsFileInfo.ftLastAccessTime);
	outInfo.lastWriteTime = fileTimeToWAVMRealTime(windowsFileInfo.ftLastWriteTime);
	outInfo.creationTime = fileTimeToWAVMRealTime(windowsFileInfo.ftCreationTime);

	return Result::success;
}

static bool transcodeUTF8ToUTF16(const std::string& in, std::wstring& out)
{
	const U8* inStart = (const U8*)in.c_str();
	const U8* inEnd = inStart + in.size();
	return Unicode::transcodeUTF8ToUTF16(inStart, inEnd, out) == inEnd;
}

static void transcodeUTF16ToUTF8(const wchar_t* in,
								 size_t numInBytes,
								 std::string& out,
								 const char* context)
{
	const U16* inStart = (const U16*)in;
	const U16* inEnd = inStart + numInBytes;
	const U16* transcodeResult = Unicode::transcodeUTF16ToUTF8(inStart, inEnd, out);
	if(transcodeResult != inEnd)
	{
		Errors::fatalf("Found an invalid UTF-16 code point (%u) in %s", *transcodeResult, context);
	}
}

static bool readDirEnts(HANDLE handle, bool startFromBeginning, std::vector<DirEnt>& outDirEnts)
{
	U8 buffer[2048];
	if(!GetFileInformationByHandleEx(
		   handle,
		   startFromBeginning ? FileIdBothDirectoryRestartInfo : FileIdBothDirectoryInfo,
		   buffer,
		   sizeof(buffer)))
	{ return false; }

	auto fileInfo = (FILE_ID_BOTH_DIR_INFO*)buffer;
	while(true)
	{
		DirEnt dirEnt;
		dirEnt.fileNumber = fileInfo->FileId.QuadPart;

		transcodeUTF16ToUTF8(fileInfo->FileName,
							 fileInfo->FileNameLength / sizeof(wchar_t),
							 dirEnt.name,
							 "a filename returned by GetFileInformationByHandleEx");

		// Assume this is a FILE_TYPE_DISK.
		dirEnt.type = fileInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY ? FileType::directory
																		  : FileType::file;

		outDirEnts.push_back(dirEnt);

		if(fileInfo->NextEntryOffset == 0) { break; }
		else
		{
			fileInfo = (FILE_ID_BOTH_DIR_INFO*)(((U8*)fileInfo) + fileInfo->NextEntryOffset);
		}
	};

	return true;
}

struct WindowsDirEntStream : DirEntStream
{
	WindowsDirEntStream(HANDLE inHandle, std::vector<DirEnt>&& inDirEnts)
	: handle(inHandle), dirEnts(std::move(inDirEnts))
	{
	}

	virtual void close() override
	{
		if(!CloseHandle(handle))
		{ Errors::fatalf("CloseHandle failed: GetLastError()=%u", GetLastError()); }
		delete this;
	}

	virtual bool getNext(DirEnt& outEntry) override
	{
		while(nextReadIndex >= dirEnts.size())
		{
			if(!readDirEnts(handle, nextReadIndex == 0, dirEnts))
			{
				if(GetLastError() == ERROR_NO_MORE_FILES) { return false; }
				else
				{
					Errors::fatalfWithCallStack("Unexpected windows error code: %u",
												GetLastError());
				}
			}
		};

		outEntry = dirEnts[nextReadIndex++];

		return true;
	}

	virtual void restart() override
	{
		dirEnts.clear();
		nextReadIndex = 0;
	}

	virtual U64 tell() override
	{
		errorUnless(nextReadIndex < UINT64_MAX);
		return U64(nextReadIndex);
	}

	virtual bool seek(U64 offset) override
	{
		// Don't allow seeking forward past the last buffered dirent.
		if(offset > UINTPTR_MAX || offset > dirEnts.size()) { return false; }

		nextReadIndex = Uptr(offset);
		return true;
	}

private:
	HANDLE handle;
	std::vector<DirEnt> dirEnts;
	Uptr nextReadIndex{0};
};

struct WindowsFD : FD
{
	WindowsFD(HANDLE inHandle,
			  DWORD inDesiredAccess,
			  DWORD inShareMode,
			  DWORD inFlagsAndAttributes,
			  bool inNonBlocking,
			  FDImplicitSync inImplicitSync)
	: handle(inHandle)
	, desiredAccess(inDesiredAccess)
	, shareMode(inShareMode)
	, flagsAndAttributes(inFlagsAndAttributes)
	, nonBlocking(inNonBlocking)
	, implicitSync(inImplicitSync)
	{
		wavmAssert(handle != INVALID_HANDLE_VALUE);
	}

	virtual Result close() override
	{
		if(!CloseHandle(handle)) { return asVFSResult(GetLastError()); }

		delete this;
		return Result::success;
	}

	virtual Result seek(I64 offset, SeekOrigin origin, U64* outAbsoluteOffset = nullptr) override
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
		if(result == INVALID_SET_FILE_POINTER)
		{
			// "If an application calls SetFilePointer with distance to move values that result
			// in a position not sector-aligned and a handle that is opened with
			// FILE_FLAG_NO_BUFFERING, the function fails, and GetLastError returns
			// ERROR_INVALID_PARAMETER."
			return GetLastError() == ERROR_INVALID_PARAMETER ? Result::invalidOffset
															 : asVFSResult(GetLastError());
		}

		if(outAbsoluteOffset) { *outAbsoluteOffset = (U64(offsetHigh) << 32) | result; }
		return Result::success;
	}
	virtual Result readv(const IOReadBuffer* buffers,
						 Uptr numBuffers,
						 Uptr* outNumBytesRead = nullptr) override
	{
		if(outNumBytesRead) { *outNumBytesRead = 0; }
		if(numBuffers == 0) { return Result::success; }

		// Count the number of bytes in all the buffers.
		Uptr numBufferBytes = 0;
		for(Uptr bufferIndex = 0; bufferIndex < numBuffers; ++bufferIndex)
		{
			const IOReadBuffer& buffer = buffers[bufferIndex];
			if(numBufferBytes + buffer.numBytes < numBufferBytes)
			{ return Result::tooManyBufferBytes; }
			numBufferBytes += buffer.numBytes;
		}
		if(numBufferBytes > UINT32_MAX) { return Result::tooManyBufferBytes; }
		const U32 numBufferBytesU32 = U32(numBufferBytes);

		// If there's a single buffer, just use it directly. Otherwise, allocate a combined buffer.
		U8* combinedBuffer = (U8*)(numBuffers == 1 ? buffers[0].data : malloc(numBufferBytes));
		if(!combinedBuffer) { return Result::outOfMemory; }

		// If the FD has implicit syncing before reads, do it.
		if(implicitSync == FDImplicitSync::syncContentsAfterWriteAndBeforeRead
		   || implicitSync == FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead)
		{ FlushFileBuffers(handle); }

		// Do the read.
		DWORD numBytesRead = 0;
		if(ReadFile(handle, combinedBuffer, numBufferBytesU32, &numBytesRead, nullptr))
		{
			if(numBuffers > 1)
			{
				// If there was a combined buffer, copy the contents of it to the individual
				// buffers.
				Uptr numBytesCopied = 0;
				for(Uptr bufferIndex = 0; bufferIndex < numBuffers && numBytesCopied < numBytesRead;
					++bufferIndex)
				{
					const IOReadBuffer& buffer = buffers[bufferIndex];
					const Uptr numBytesToCopy
						= std::min(buffer.numBytes, numBytesRead - numBytesCopied);
					if(numBytesToCopy)
					{ memcpy(buffer.data, combinedBuffer + numBytesCopied, numBytesToCopy); }
					numBytesCopied += numBytesToCopy;
				}

				// Free the combined buffer.
				free(combinedBuffer);
			}

			// Write the total number of bytes read and return success.
			if(outNumBytesRead)
			{
				wavmAssert(numBytesRead <= UINTPTR_MAX);
				*outNumBytesRead = Uptr(numBytesRead);
			}
			return Result::success;
		}
		else
		{
			// If there was a combined buffer, free it.
			if(numBuffers > 1) { free(combinedBuffer); }

			if(GetLastError() == ERROR_NOT_ENOUGH_QUOTA)
			{
				// "The ReadFile function may fail with ERROR_NOT_ENOUGH_QUOTA, which means the
				// calling process's buffer could not be page-locked."
				return Result::outOfMemory;
			}
			else
			{
				return asVFSResult(GetLastError());
			}
		}
	}
	virtual Result writev(const IOWriteBuffer* buffers,
						  Uptr numBuffers,
						  Uptr* outNumBytesWritten = nullptr) override
	{
		if(outNumBytesWritten) { *outNumBytesWritten = 0; }
		if(numBuffers == 0) { return Result::success; }

		// Count the number of bytes in all the buffers.
		Uptr numBufferBytes = 0;
		for(Uptr bufferIndex = 0; bufferIndex < numBuffers; ++bufferIndex)
		{
			const IOWriteBuffer& buffer = buffers[bufferIndex];
			if(numBufferBytes + buffer.numBytes < numBufferBytes)
			{ return Result::tooManyBufferBytes; }
			numBufferBytes += buffer.numBytes;
		}
		if(numBufferBytes > Uptr(UINT32_MAX)) { return Result::tooManyBufferBytes; }
		const U32 numBufferBytesU32 = U32(numBufferBytes);

		// If there's a single buffer, just use it directly. Otherwise, allocate a combined buffer.
		const void* combinedBuffer;
		if(numBuffers == 1) { combinedBuffer = buffers[0].data; }
		else
		{
			U8* combinedBufferU8 = (U8*)malloc(numBufferBytes);
			if(!combinedBufferU8) { return Result::outOfMemory; }

			Uptr numBytesCopied = 0;
			for(Uptr bufferIndex = 0; bufferIndex < numBuffers; ++bufferIndex)
			{
				const IOWriteBuffer& buffer = buffers[bufferIndex];
				if(buffer.numBytes)
				{ memcpy(combinedBufferU8 + numBytesCopied, buffer.data, buffer.numBytes); }
				numBytesCopied += buffer.numBytes;
			}
			combinedBuffer = combinedBufferU8;
		}

		// Do the write.
		DWORD numBytesWritten = 0;
		const BOOL result
			= WriteFile(handle, combinedBuffer, numBufferBytesU32, &numBytesWritten, nullptr);

		// Free the combined buffer.
		if(numBuffers > 1) { free((void*)combinedBuffer); }

		if(result)
		{
			// If the FD has implicit syncing before reads, do it.
			if(implicitSync != FDImplicitSync::none) { FlushFileBuffers(handle); }

			// Write the total number of bytes written and return success.
			if(outNumBytesWritten)
			{
				wavmAssert(numBytesWritten <= UINTPTR_MAX);
				*outNumBytesWritten = Uptr(numBytesWritten);
			}
			return Result::success;
		}
		else
		{
			if(result != ERROR_NOT_ENOUGH_QUOTA) { return asVFSResult(result); }
			else
			{
				// "The WriteFile function may fail with ERROR_NOT_ENOUGH_QUOTA, which means the
				// calling process's buffer could not be page-locked."
				return Result::outOfMemory;
			}
		}
	}
	virtual Result sync(SyncType syncType) override
	{
		return FlushFileBuffers(handle) ? Result::success : asVFSResult(GetLastError());
	}

	virtual Result getFDInfo(FDInfo& outInfo) override
	{
		const Result getTypeResult = getFileType(handle, outInfo.type);
		if(getTypeResult != Result::success) { return getTypeResult; }

		outInfo.flags.append = desiredAccess & FILE_APPEND_DATA;
		outInfo.flags.nonBlocking = nonBlocking;
		outInfo.flags.implicitSync = implicitSync;
		return Result::success;
	}
	virtual Result getFileInfo(FileInfo& outInfo) override
	{
		return getFileInfoByHandle(handle, outInfo);
	}

	virtual Result setFDFlags(const FDFlags& flags)
	{
		const DWORD originalDesiredAccess = desiredAccess;
		if(originalDesiredAccess & (GENERIC_WRITE | FILE_APPEND_DATA))
		{
			desiredAccess &= ~(GENERIC_WRITE | FILE_APPEND_DATA);
			desiredAccess |= flags.append ? FILE_APPEND_DATA : GENERIC_WRITE;
		}

		if(desiredAccess != originalDesiredAccess)
		{
			HANDLE reopenedHandle
				= ReOpenFile(handle, desiredAccess, shareMode, flagsAndAttributes);
			if(reopenedHandle != INVALID_HANDLE_VALUE)
			{
				if(!CloseHandle(handle))
				{ Errors::fatalf("CloseHandle failed: GetLastError()=%u", GetLastError()); }
				handle = reopenedHandle;
			}
			else
			{
				desiredAccess = originalDesiredAccess;
				return asVFSResult(GetLastError());
			}
		}

		nonBlocking = flags.nonBlocking;
		implicitSync = flags.implicitSync;

		return Result::success;
	}

	virtual Result openDir(DirEntStream*& outStream) override
	{
		// Make a copy of the handle, so the FD and the DirEntStream can be closed independently.
		HANDLE duplicatedHandle = INVALID_HANDLE_VALUE;
		errorUnless(DuplicateHandle(GetCurrentProcess(),
									handle,
									GetCurrentProcess(),
									&duplicatedHandle,
									0,
									TRUE,
									DUPLICATE_SAME_ACCESS));

		// Try to read the initial buffer-full of dirents to determine whether this is a directory.
		std::vector<DirEnt> initialDirEnts;
		if(readDirEnts(duplicatedHandle, true, initialDirEnts)
		   || GetLastError() == ERROR_NO_MORE_FILES)
		{
			outStream = new WindowsDirEntStream(duplicatedHandle, std::move(initialDirEnts));
			return Result::success;
		}
		else
		{
			const Result result = GetLastError() == ERROR_INVALID_PARAMETER
									  ? Result::isNotDirectory
									  : asVFSResult(GetLastError());

			// Close the duplicated handle.
			if(!CloseHandle(duplicatedHandle))
			{ Errors::fatalf("CloseHandle failed: GetLastError()=%u", GetLastError()); }

			return result;
		}
	}

private:
	HANDLE handle;
	DWORD desiredAccess;
	DWORD shareMode;
	DWORD flagsAndAttributes;
	bool nonBlocking;
	FDImplicitSync implicitSync;
};

struct WindowsStdFD : WindowsFD
{
	WindowsStdFD(HANDLE inHandle, DWORD inDesiredAccess, FDImplicitSync inImplicitSync)
	: WindowsFD(inHandle,
				inDesiredAccess,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				0,
				false,
				inImplicitSync)
	{
	}

	virtual Result close()
	{
		// The stdio FDs are shared, so don't close them.
		return Result::success;
	}
};

FD* Platform::getStdFD(StdDevice device)
{
	static WindowsStdFD* stdinVFD
		= new WindowsStdFD(GetStdHandle(STD_INPUT_HANDLE), GENERIC_READ, FDImplicitSync::none);
	static WindowsStdFD* stdoutVFD
		= new WindowsStdFD(GetStdHandle(STD_OUTPUT_HANDLE), FILE_APPEND_DATA, FDImplicitSync::none);
	static WindowsStdFD* stderrVFD
		= new WindowsStdFD(GetStdHandle(STD_ERROR_HANDLE), FILE_APPEND_DATA, FDImplicitSync::none);

	switch(device)
	{
	case StdDevice::in: return stdinVFD;
	case StdDevice::out: return stdoutVFD;
	case StdDevice::err: return stderrVFD;
	default: WAVM_UNREACHABLE();
	};
}

Result Platform::openHostFile(const std::string& pathName,
							  FileAccessMode accessMode,
							  FileCreateMode createMode,
							  FD*& outFD,
							  const FDFlags& flags)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW)) { return Result::invalidNameCharacter; }

	// Map the VFS accessMode/createMode to the appropriate Windows CreateFile arguments.
	DWORD desiredAccess = 0;
	DWORD shareMode = FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE;
	DWORD creationDisposition = 0;
	DWORD flagsAndAttributes = 0;

	// From https://docs.microsoft.com/en-us/windows/desktop/api/fileapi/nf-fileapi-createfilew:
	// The file is being opened or created for a backup or restore operation. The system
	// ensures that the calling process overrides file security checks when the process has
	// SE_BACKUP_NAME and SE_RESTORE_NAME privileges. For more information, see Changing
	// Privileges in a Token.
	// You must set this flag to obtain a handle to a directory. A directory handle can be
	// passed to some functions instead of a file handle.For more information, see the Remarks
	// section.
	flagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;

	const DWORD writeOrAppend = flags.append ? FILE_APPEND_DATA : GENERIC_WRITE;
	switch(accessMode)
	{
	case FileAccessMode::none: desiredAccess = 0; break;
	case FileAccessMode::readOnly: desiredAccess = GENERIC_READ; break;
	case FileAccessMode::writeOnly: desiredAccess = writeOrAppend; break;
	case FileAccessMode::readWrite: desiredAccess = GENERIC_READ | writeOrAppend; break;
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
	if(handle == INVALID_HANDLE_VALUE) { return asVFSResult(GetLastError()); }

	outFD = new WindowsFD(handle,
						  desiredAccess,
						  shareMode,
						  flagsAndAttributes,
						  flags.nonBlocking,
						  flags.implicitSync);
	return Result::success;
}

Result Platform::getHostFileInfo(const std::string& pathName, FileInfo& outInfo)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW)) { return Result::invalidNameCharacter; }

	// Try to open the file with no access requested.
	HANDLE handle = CreateFileW(pathNameW.c_str(),
								0,
								FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
								nullptr,
								OPEN_EXISTING,
								0,
								nullptr);
	if(handle == INVALID_HANDLE_VALUE) { return asVFSResult(GetLastError()); }

	Result getInfoResult = getFileInfoByHandle(handle, outInfo);
	if(getInfoResult != Result::success) { return getInfoResult; }

	if(!CloseHandle(handle))
	{ Errors::fatalf("CloseHandle failed: GetLastError()=%u", GetLastError()); }

	return Result::success;
}

Result Platform::unlinkHostFile(const std::string& pathName)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW)) { return Result::invalidNameCharacter; }

	return DeleteFileW(pathNameW.c_str()) ? Result::success : asVFSResult(GetLastError());
}

Result Platform::removeHostDir(const std::string& pathName)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW)) { return Result::invalidNameCharacter; }

	return RemoveDirectoryW(pathNameW.c_str()) ? Result::success : asVFSResult(GetLastError());
}

Result Platform::createHostDir(const std::string& pathName)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW)) { return Result::invalidNameCharacter; }

	return CreateDirectoryW(pathNameW.c_str(), nullptr) ? Result::success
														: asVFSResult(GetLastError());
}

Result Platform::openHostDir(const std::string& pathName, DirEntStream*& outStream)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW)) { return Result::invalidNameCharacter; }

	// Try to open the file.
	HANDLE handle = CreateFileW(pathNameW.c_str(),
								0,
								FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
								nullptr,
								OPEN_EXISTING,
								FILE_FLAG_BACKUP_SEMANTICS,
								nullptr);
	if(handle == INVALID_HANDLE_VALUE) { return asVFSResult(GetLastError()); }

	// Try to read the initial buffer-full of dirents to determine whether this is a directory.
	std::vector<DirEnt> initialDirEnts;
	if(readDirEnts(handle, true, initialDirEnts) || GetLastError() == ERROR_NO_MORE_FILES)
	{
		outStream = new WindowsDirEntStream(handle, std::move(initialDirEnts));
		return Result::success;
	}
	else
	{
		const VFS::Result result = GetLastError() == ERROR_INVALID_PARAMETER
									   ? Result::isNotDirectory
									   : asVFSResult(GetLastError());

		// Close the file handle we just opened if there was an error reading dirents from it.
		if(!CloseHandle(handle))
		{ Errors::fatalf("CloseHandle failed: GetLastError()=%u", GetLastError()); }

		return result;
	}
}

std::string Platform::getCurrentWorkingDirectory()
{
	wchar_t buffer[MAX_PATH];
	const DWORD numChars = GetCurrentDirectoryW(MAX_PATH, buffer);
	errorUnless(numChars);

	std::string result;
	transcodeUTF16ToUTF8(buffer, numChars, result, "the result of GetCurrentDirectoryW");

	return result;
}
