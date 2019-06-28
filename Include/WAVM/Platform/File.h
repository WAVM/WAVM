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
	PLATFORM_API VFS::OpenResult openHostFile(const std::string& pathName,
											  VFS::FileAccessMode accessMode,
											  VFS::FileCreateMode createMode,
											  VFS::FD*& outFD,
											  VFS::FDImplicitSync implicitSync
											  = VFS::FDImplicitSync::none);
	PLATFORM_API VFS::GetInfoByPathResult getHostFileInfo(const std::string& pathName,
														  VFS::FileInfo& outInfo);
	PLATFORM_API VFS::UnlinkFileResult unlinkHostFile(const std::string& pathName);
	PLATFORM_API VFS::RemoveDirResult removeHostDir(const std::string& pathName);
	PLATFORM_API VFS::CreateDirResult createHostDir(const std::string& pathName);

	PLATFORM_API VFS::OpenDirByPathResult openHostDir(const std::string& pathName,
													  VFS::DirEntStream*& outStream);
	PLATFORM_API std::string getCurrentWorkingDirectory();
}}
