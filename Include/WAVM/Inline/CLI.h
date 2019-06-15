#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"

#include <vector>

namespace WAVM {
	inline bool loadFile(const char* filename, std::vector<U8>& outFileContents)
	{
		VFS::FD* vfd = Platform::openHostFile(
			filename, VFS::FileAccessMode::readOnly, VFS::FileCreateMode::openExisting);
		if(!vfd)
		{
			Log::printf(Log::error, "Couldn't read %s: couldn't open file.\n", filename);
			return false;
		}

		U64 numFileBytes64 = 0;
		errorUnless(vfd->seek(0, VFS::SeekOrigin::end, &numFileBytes64)
					== VFS::SeekResult::success);
		if(numFileBytes64 > UINTPTR_MAX)
		{
			Log::printf(Log::error, "Couldn't read %s: file doesn't fit in memory.\n", filename);
			errorUnless(vfd->close() == VFS::CloseResult::success);
			return false;
		}
		const Uptr numFileBytes = Uptr(numFileBytes64);

		std::vector<U8> fileContents;
		outFileContents.resize(numFileBytes);
		errorUnless(vfd->seek(0, VFS::SeekOrigin::begin) == VFS::SeekResult::success);
		errorUnless(vfd->read(const_cast<U8*>(outFileContents.data()), numFileBytes)
					== VFS::ReadResult::success);
		errorUnless(vfd->close() == VFS::CloseResult::success);

		return true;
	}

	inline bool saveFile(const char* filename, const void* fileBytes, Uptr numFileBytes)
	{
		VFS::FD* vfd = Platform::openHostFile(
			filename, VFS::FileAccessMode::writeOnly, VFS::FileCreateMode::createAlways);
		if(!vfd)
		{
			Log::printf(Log::error, "Couldn't write %s: couldn't open file.\n", filename);
			return false;
		}

		errorUnless(vfd->write(fileBytes, numFileBytes) == VFS::WriteResult::success);
		errorUnless(vfd->close() == VFS::CloseResult::success);

		return true;
	}
}
