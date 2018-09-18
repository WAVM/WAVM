#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/File.h"

#include <vector>

namespace WAVM {
	inline bool loadFile(const char* filename, std::vector<U8>& outFileContents)
	{
		Platform::File* file = Platform::openFile(
			filename, Platform::FileAccessMode::readOnly, Platform::FileCreateMode::openExisting);
		if(!file)
		{
			Log::printf(Log::error, "Couldn't read %s: couldn't open file.\n", filename);
			return false;
		}

		U64 numFileBytes64 = 0;
		errorUnless(Platform::seekFile(file, 0, Platform::FileSeekOrigin::end, &numFileBytes64));
		if(numFileBytes64 > UINTPTR_MAX)
		{
			Log::printf(Log::error, "Couldn't read %s: file doesn't fit in memory.\n", filename);
			errorUnless(Platform::closeFile(file));
			return false;
		}
		const Uptr numFileBytes = Uptr(numFileBytes64);

		std::vector<U8> fileContents;
		outFileContents.resize(numFileBytes);
		errorUnless(Platform::seekFile(file, 0, Platform::FileSeekOrigin::begin));
		errorUnless(
			Platform::readFile(file, const_cast<U8*>(outFileContents.data()), numFileBytes));
		errorUnless(Platform::closeFile(file));

		return true;
	}

	inline bool saveFile(const char* filename, const void* fileBytes, Uptr numFileBytes)
	{
		Platform::File* file = Platform::openFile(
			filename, Platform::FileAccessMode::writeOnly, Platform::FileCreateMode::createAlways);
		if(!file)
		{
			Log::printf(Log::error, "Couldn't write %s: couldn't open file.\n", filename);
			return false;
		}

		errorUnless(Platform::writeFile(file, fileBytes, numFileBytes));
		errorUnless(Platform::closeFile(file));

		return true;
	}
}
