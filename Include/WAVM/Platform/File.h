#pragma once

#include <string>

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/VFS/VFS.h"

namespace WAVM { namespace Platform {

	enum class StdDevice
	{
		in,
		out,
		err,
	};

	PLATFORM_API VFS::FD* getStdFD(StdDevice device);

	PLATFORM_API VFS::Result openHostFile(const std::string& pathName,
										  VFS::FileAccessMode accessMode,
										  VFS::FileCreateMode createMode,
										  VFS::FD*& outFD,
										  const VFS::FDFlags& flags = VFS::FDFlags{});

	PLATFORM_API VFS::Result getHostFileInfo(const std::string& pathName, VFS::FileInfo& outInfo);
	PLATFORM_API VFS::Result setHostFileTimes(const std::string& pathName,
											  bool setLastAccessTime,
											  I128 lastAccessTime,
											  bool setLastWriteTime,
											  I128 lastWriteTime);

	PLATFORM_API VFS::Result unlinkHostFile(const std::string& pathName);
	PLATFORM_API VFS::Result removeHostDir(const std::string& pathName);
	PLATFORM_API VFS::Result createHostDir(const std::string& pathName);

	PLATFORM_API VFS::Result openHostDir(const std::string& pathName,
										 VFS::DirEntStream*& outStream);

	PLATFORM_API std::string getCurrentWorkingDirectory();
}}
