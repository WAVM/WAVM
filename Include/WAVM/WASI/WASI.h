#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Runtime/Runtime.h"

namespace WAVM { namespace VFS {
	struct FileSystem;
	struct FD;
}}

namespace WAVM { namespace WASI {
	enum class RunResult
	{
		success = 0,
		linkError,
		noStartFunction,
		mistypedStartFunction,
		doesNotExportMemory
	};
	WASI_API RunResult run(Runtime::ModuleConstRefParam module,
						   std::vector<std::string>&& inArgs,
						   std::vector<std::string>&& inEnvs,
						   VFS::FileSystem* fileSystem,
						   VFS::FD* stdIn,
						   VFS::FD* stdOut,
						   VFS::FD* stdErr,
						   I32& outExitCode);

	enum class SyscallTraceLevel
	{
		none,
		syscalls,
		syscallsWithCallstacks
	};

	WASI_API void setSyscallTraceLevel(SyscallTraceLevel newLevel);
}}
