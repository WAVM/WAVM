#pragma once

#include <vector>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"

namespace WAVM {
	inline bool loadFile(const char* filename, std::vector<U8>& outFileContents)
	{
		VFS::Result result;
		VFS::VFD* vfd = nullptr;
		{
			result = Platform::getHostFS().open(
				filename, VFS::FileAccessMode::readOnly, VFS::FileCreateMode::openExisting, vfd);
			if(result != VFS::Result::success) { goto printAndReturnError; }

			U64 numFileBytes64 = 0;
			result = vfd->seek(0, VFS::SeekOrigin::end, &numFileBytes64);
			if(result != VFS::Result::success) { goto printAndReturnError; }
			else if(numFileBytes64 > UINTPTR_MAX)
			{
				result = VFS::Result::outOfMemory;
				goto printAndReturnError;
			}

			const Uptr numFileBytes = Uptr(numFileBytes64);
			std::vector<U8> fileContents;
			outFileContents.resize(numFileBytes);

			result = vfd->seek(0, VFS::SeekOrigin::begin);
			if(result != VFS::Result::success) { goto printAndReturnError; }

			result = vfd->read(const_cast<U8*>(outFileContents.data()), numFileBytes);
			if(result != VFS::Result::success) { goto printAndReturnError; }

			result = vfd->close();
			if(result != VFS::Result::success)
			{
				vfd = nullptr;
				goto printAndReturnError;
			}

			return true;
		}
	printAndReturnError:
		Log::printf(Log::error, "Error loading '%s': %s\n", filename, VFS::describeResult(result));
		if(vfd) { WAVM_ERROR_UNLESS(vfd->close() == VFS::Result::success); }
		return false;
	}

	inline bool saveFile(const char* filename, const void* fileBytes, Uptr numFileBytes)
	{
		VFS::Result result;
		VFS::VFD* vfd = nullptr;
		{
			result = Platform::getHostFS().open(
				filename, VFS::FileAccessMode::writeOnly, VFS::FileCreateMode::createAlways, vfd);
			if(result != VFS::Result::success) { goto printAndReturnError; }

			result = vfd->write(fileBytes, numFileBytes);
			if(result != VFS::Result::success) { goto printAndReturnError; }

			result = vfd->close();
			if(result != VFS::Result::success)
			{
				vfd = nullptr;
				goto printAndReturnError;
			}

			return true;
		}
	printAndReturnError:
		Log::printf(Log::error, "Error saving '%s': %s\n", filename, VFS::describeResult(result));
		if(vfd) { WAVM_ERROR_UNLESS(vfd->close() == VFS::Result::success); }
		return false;
	}
}
