#pragma once

#include <string>

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/VFS/VFS.h"

namespace WAVM { namespace Platform {
	PLATFORM_API VFS::FD* getStdFD(VFS::StdDevice device);
	PLATFORM_API VFS::FD* openHostFile(const std::string& pathName,
									   VFS::FileAccessMode accessMode,
									   VFS::FileCreateMode createMode,
									   VFS::FDImplicitSync implicitSync
									   = VFS::FDImplicitSync::none);
	PLATFORM_API std::string getCurrentWorkingDirectory();
}}
