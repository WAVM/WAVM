#include <algorithm>
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

		default:
			Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
		}
	}

	switch(windowsFileType)
	{
	case FILE_TYPE_CHAR: outType = FileType::characterDevice; break;
	case FILE_TYPE_PIPE: outType = FileType::pipe; break;
	case FILE_TYPE_DISK: {
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

			default:
				Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
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

		default:
			Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
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

		const U16* fileNameEnd = (U16*)fileInfo->FileName + fileInfo->FileNameLength / sizeof(U16);
		const U16* transcodeResult = Unicode::transcodeUTF16ToUTF8(
			(const U16*)fileInfo->FileName, fileNameEnd, dirEnt.name);
		if(transcodeResult != fileNameEnd)
		{
			Errors::fatalf(
				"GetFileInformationByHandleEx returned a file name with an "
				"invalid UTF-16 code point: %u\n",
				*transcodeResult);
		}

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

static bool transcodeUTF8ToUTF16(const std::string& in, std::wstring& out)
{
	const U8* inStart = (const U8*)in.c_str();
	const U8* inEnd = inStart + in.size();
	return Unicode::transcodeUTF8ToUTF16(inStart, inEnd, out) == inEnd;
}

struct WindowsDirEntStream : DirEntStream
{
	WindowsDirEntStream(HANDLE inHandle, std::vector<DirEnt>&& inDirEnts)
	: handle(inHandle), dirEnts(std::move(inDirEnts))
	{
	}

	virtual void close() override
	{
		errorUnless(CloseHandle(handle));
		delete this;
	}

	virtual bool getNext(DirEnt& outEntry) override
	{
		while(nextReadIndex >= dirEnts.size())
		{
			if(!readDirEnts(handle, nextReadIndex == 0, dirEnts))
			{
				switch(GetLastError())
				{
				case ERROR_INVALID_HANDLE:
					Errors::fatalf(
						"GetFileInformationByHandleEx returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR
						")",
						reinterpret_cast<Uptr>(handle));

				case ERROR_NO_MORE_FILES: return false;

				default:
					Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u",
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

			default:
				Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
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

			default:
				Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
			};
		}
	}
	virtual ReadResult readv(const IOReadBuffer* buffers,
							 Uptr numBuffers,
							 Uptr* outNumBytesRead = nullptr) override
	{
		if(outNumBytesRead) { *outNumBytesRead = 0; }
		if(numBuffers == 0) { return ReadResult::success; }

		// Count the number of bytes in all the buffers.
		Uptr numBufferBytes = 0;
		for(Uptr bufferIndex = 0; bufferIndex < numBuffers; ++bufferIndex)
		{
			const IOReadBuffer& buffer = buffers[bufferIndex];
			if(numBufferBytes + buffer.numBytes < numBufferBytes)
			{ return ReadResult::tooManyBytes; }
			numBufferBytes += buffer.numBytes;
		}
		if(numBufferBytes > UINT32_MAX) { return ReadResult::tooManyBytes; }
		const U32 numBufferBytesU32 = U32(numBufferBytes);

		// If there's a single buffer, just use it directly. Otherwise, allocate a combined buffer.
		U8* combinedBuffer = (U8*)(numBuffers == 1 ? buffers[0].data : malloc(numBufferBytes));
		if(!combinedBuffer) { return ReadResult::outOfMemory; }

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
			return ReadResult::success;
		}
		else
		{
			// If there was a combined buffer, free it.
			if(numBuffers > 1) { free(combinedBuffer); }

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

			default:
				Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
			};
		}
	}
	virtual WriteResult writev(const IOWriteBuffer* buffers,
							   Uptr numBuffers,
							   Uptr* outNumBytesWritten = nullptr) override
	{
		if(outNumBytesWritten) { *outNumBytesWritten = 0; }
		if(numBuffers == 0) { return WriteResult::success; }

		// Count the number of bytes in all the buffers.
		Uptr numBufferBytes = 0;
		for(Uptr bufferIndex = 0; bufferIndex < numBuffers; ++bufferIndex)
		{
			const IOWriteBuffer& buffer = buffers[bufferIndex];
			if(numBufferBytes + buffer.numBytes < numBufferBytes)
			{ return WriteResult::tooManyBytes; }
			numBufferBytes += buffer.numBytes;
		}
		if(numBufferBytes > Uptr(UINT32_MAX)) { return WriteResult::tooManyBytes; }
		const U32 numBufferBytesU32 = U32(numBufferBytes);

		// If there's a single buffer, just use it directly. Otherwise, allocate a combined buffer.
		const void* combinedBuffer;
		if(numBuffers == 1) { combinedBuffer = buffers[0].data; }
		else
		{
			U8* combinedBufferU8 = (U8*)malloc(numBufferBytes);
			if(!combinedBufferU8) { return WriteResult::outOfMemory; }

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

			default:
				Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
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

			default:
				Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
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

	virtual OpenDirResult openDir(DirEntStream*& outStream) override
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
			return OpenDirResult::success;
		}
		else
		{
			// Close the duplicated handle.
			errorUnless(CloseHandle(duplicatedHandle));

			switch(GetLastError())
			{
			case ERROR_INVALID_HANDLE:
				Errors::fatalf(
					"GetFileInformationByHandleEx returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR
					")",
					reinterpret_cast<Uptr>(handle));

			case ERROR_INVALID_PARAMETER: return OpenDirResult::notDirectory;

			default:
				Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
			};
		}
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
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW)) { return OpenResult::invalidNameCharacter; }

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

		default:
			Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
		}
	}
}

GetInfoByPathResult Platform::getHostFileInfo(const std::string& pathName, FileInfo& outInfo)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW))
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

		default:
			Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
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

UnlinkFileResult Platform::unlinkHostFile(const std::string& pathName)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW))
	{ return UnlinkFileResult::invalidNameCharacter; }

	if(DeleteFileW(pathNameW.c_str())) { return UnlinkFileResult::success; }
	else
	{
		switch(GetLastError())
		{
		case ERROR_ACCESS_DENIED: return UnlinkFileResult::notPermitted;
		case ERROR_FILE_NOT_FOUND: return UnlinkFileResult::doesNotExist;

		default:
			Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
		};
	}
}

RemoveDirResult Platform::removeHostDir(const std::string& pathName)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW)) { return RemoveDirResult::invalidNameCharacter; }

	if(RemoveDirectoryW(pathNameW.c_str())) { return RemoveDirResult::success; }
	else
	{
		switch(GetLastError())
		{
		case ERROR_ACCESS_DENIED: return RemoveDirResult::notPermitted;
		case ERROR_FILE_NOT_FOUND: return RemoveDirResult::doesNotExist;
		case ERROR_DIR_NOT_EMPTY: return RemoveDirResult::notEmpty;

		default:
			Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
		};
	}
}

CreateDirResult Platform::createHostDir(const std::string& pathName)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW)) { return CreateDirResult::invalidNameCharacter; }

	if(CreateDirectoryW(pathNameW.c_str(), nullptr)) { return CreateDirResult::success; }
	else
	{
		switch(GetLastError())
		{
		case ERROR_ACCESS_DENIED: return CreateDirResult::notPermitted;
		case ERROR_ALREADY_EXISTS: return CreateDirResult::alreadyExists;
		case ERROR_HANDLE_DISK_FULL: return CreateDirResult::outOfFreeSpace;

		default:
			Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
		};
	}
}

OpenDirByPathResult Platform::openHostDir(const std::string& pathName, DirEntStream*& outStream)
{
	// Translate the path from UTF-8 to UTF-16.
	std::wstring pathNameW;
	if(!transcodeUTF8ToUTF16(pathName, pathNameW))
	{ return OpenDirByPathResult::invalidNameCharacter; }

	// Try to open the file.
	HANDLE handle = CreateFileW(pathNameW.c_str(),
								0,
								FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
								nullptr,
								OPEN_EXISTING,
								FILE_FLAG_BACKUP_SEMANTICS,
								nullptr);
	if(handle == INVALID_HANDLE_VALUE)
	{
		switch(GetLastError())
		{
		case ERROR_ACCESS_DENIED: return OpenDirByPathResult::notPermitted;
		case ERROR_FILE_NOT_FOUND: return OpenDirByPathResult::doesNotExist;
		case ERROR_IO_DEVICE: return OpenDirByPathResult::ioError;

		default:
			Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
		}
	}
	else
	{
		// Try to read the initial buffer-full of dirents to determine whether this is a directory.
		std::vector<DirEnt> initialDirEnts;
		if(readDirEnts(handle, true, initialDirEnts) || GetLastError() == ERROR_NO_MORE_FILES)
		{
			outStream = new WindowsDirEntStream(handle, std::move(initialDirEnts));
			return OpenDirByPathResult::success;
		}
		else
		{
			// Close the file handle we just opened if there was an error reading dirents from it.
			errorUnless(CloseHandle(handle));

			switch(GetLastError())
			{
			case ERROR_INVALID_HANDLE:
				Errors::fatalf(
					"GetFileInformationByHandleEx returned ERROR_INVALID_HANDLE (handle=%" PRIxPTR
					")",
					reinterpret_cast<Uptr>(handle));

			case ERROR_INVALID_PARAMETER: return OpenDirByPathResult::notDirectory;

			default:
				Errors::fatalfWithCallStack("Unexpected GetLastError() return: %u", GetLastError());
			};
		}
	}
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
