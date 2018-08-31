#pragma once

#include "Inline/BasicTypes.h"
#include "Inline/IndexAllocator.h"
#include "Platform/Platform.h"
#include "wavix.h"

namespace Wavix
{
	struct File;

	struct FileSystem
	{
		virtual ~FileSystem() {}
	};

	struct File
	{
		FileSystem* const fs;

		File(FileSystem* inFS) : fs(inFS) {}

		virtual U64 seek(U64 newPosition, Platform::FileSeekOrigin origin) = 0;
		virtual Iptr read(U8* buffer, Uptr numBytes) = 0;
		virtual Iptr write(U8* buffer, Uptr numBytes) = 0;

		virtual ~File() {}
	};
}