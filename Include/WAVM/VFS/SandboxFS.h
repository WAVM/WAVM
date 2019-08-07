#pragma once

#include <string>
#include <memory>

namespace WAVM { namespace VFS {
	struct FileSystem;
	VFS_API std::shared_ptr<FileSystem> makeSandboxFS(FileSystem* innerFS, const std::string& innerRootPath);
}}
