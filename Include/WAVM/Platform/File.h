#pragma once

#include <string>

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/VFS/VFS.h"

namespace WAVM { namespace Platform {

	enum class StdDevice
	{
		in,
		out,
		err,
	};

	PLATFORM_API VFS::VFD* getStdFD(StdDevice device);
	PLATFORM_API std::string getCurrentWorkingDirectory();

	struct HostFS : VFS::FileSystem
	{
		// HostFS is intended to be a singleton, so prevent users from deleting it.
	protected:
		virtual ~HostFS() override {}
	};
	PLATFORM_API HostFS& getHostFS();
}}
