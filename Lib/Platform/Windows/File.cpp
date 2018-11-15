#include "WAVM/Platform/File.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Unicode.h"
#include "WAVM/Platform/Event.h"

#define NOMINMAX
#include <Windows.h>

using namespace WAVM;
using namespace WAVM::Platform;

static File* fileHandleToPointer(HANDLE handle)
{
	return reinterpret_cast<File*>(reinterpret_cast<Uptr>(handle) + 1);
}

static HANDLE filePointerToHandle(File* file)
{
	return reinterpret_cast<HANDLE>(reinterpret_cast<Uptr>(file) - 1);
}

File* Platform::openFile(const std::string& pathName,
						 FileAccessMode accessMode,
						 FileCreateMode createMode)
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

	return fileHandleToPointer(handle);
}

bool Platform::closeFile(File* file) { return CloseHandle(filePointerToHandle(file)) != 0; }

File* Platform::getStdFile(StdDevice device)
{
	DWORD StdHandle = 0;
	switch(device)
	{
	case StdDevice::in: StdHandle = STD_INPUT_HANDLE; break;
	case StdDevice::out: StdHandle = STD_OUTPUT_HANDLE; break;
	case StdDevice::err: StdHandle = STD_ERROR_HANDLE; break;
	default: Errors::unreachable();
	};

	return fileHandleToPointer(GetStdHandle(StdHandle));
}

bool Platform::seekFile(File* file, I64 offset, FileSeekOrigin origin, U64* outAbsoluteOffset)
{
	LONG offsetHigh = LONG((offset >> 32) & 0xffffffff);
	LONG result = SetFilePointer(
		filePointerToHandle(file), U32(offset & 0xffffffff), &offsetHigh, DWORD(origin));
	if(result == LONG(INVALID_SET_FILE_POINTER)) { return false; }
	if(outAbsoluteOffset) { *outAbsoluteOffset = (U64(offsetHigh) << 32) | result; }
	return true;
}

bool Platform::readFile(File* file, void* outData, Uptr numBytes, Uptr* outNumBytesRead)
{
	if(outNumBytesRead) { *outNumBytesRead = 0; }
	if(numBytes > Uptr(UINT32_MAX)) { return false; }

	DWORD windowsNumBytesRead = 0;
	const BOOL result = ReadFile(
		filePointerToHandle(file), outData, U32(numBytes), &windowsNumBytesRead, nullptr);

	if(outNumBytesRead) { *outNumBytesRead = Uptr(windowsNumBytesRead); }

	return result != 0;
}

bool Platform::writeFile(File* file, const void* data, Uptr numBytes, Uptr* outNumBytesWritten)
{
	if(outNumBytesWritten) { *outNumBytesWritten = 0; }
	if(numBytes > Uptr(UINT32_MAX)) { return false; }

	DWORD windowsNumBytesWritten = 0;
	const BOOL result = WriteFile(
		filePointerToHandle(file), data, U32(numBytes), &windowsNumBytesWritten, nullptr);

	if(outNumBytesWritten) { *outNumBytesWritten = Uptr(windowsNumBytesWritten); }

	return result != 0;
}

bool Platform::flushFileWrites(File* file)
{
	return FlushFileBuffers(filePointerToHandle(file)) != 0;
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
