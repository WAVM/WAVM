#pragma once

#include <string>

namespace WAVM { namespace VFS {
	struct FileSystem;
	VFS_API FileSystem* makeSandboxFS(FileSystem* innerFS, const std::string& innerRootPath);
}}
