#pragma once

#include <string>

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Platform {
	enum class FileAccessMode
	{
		readOnly = 0x1,
		writeOnly = 0x2,
		readWrite = 0x1 | 0x2,
	};

	enum class FileCreateMode
	{
		createAlways,
		createNew,
		openAlways,
		openExisting,
		truncateExisting,
	};

	enum class StdDevice
	{
		in,
		out,
		err,
	};

	enum class FileSeekOrigin
	{
		begin = 0,
		cur = 1,
		end = 2
	};

	struct File;
	PLATFORM_API File* openFile(const std::string& pathName,
								FileAccessMode accessMode,
								FileCreateMode createMode);
	PLATFORM_API bool closeFile(File* file);
	PLATFORM_API File* getStdFile(StdDevice device);
	PLATFORM_API bool seekFile(File* file,
							   I64 offset,
							   FileSeekOrigin origin,
							   U64* outAbsoluteOffset = nullptr);
	PLATFORM_API bool readFile(File* file,
							   void* outData,
							   Uptr numBytes,
							   Uptr* outNumBytesRead = nullptr);
	PLATFORM_API bool writeFile(File* file,
								const void* data,
								Uptr numBytes,
								Uptr* outNumBytesWritten = nullptr);
	PLATFORM_API bool flushFileWrites(File* file);
	PLATFORM_API std::string getCurrentWorkingDirectory();
}}
