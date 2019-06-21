#include "WAVM/WASI/WASI.h"
#include <atomic>
#include "./WASIDefinitions.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/IndexMap.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/File.h"
#include "WAVM/Platform/Intrinsic.h"
#include "WAVM/Platform/Random.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/VFS/VFS.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;
using namespace WAVM::WASI;

DEFINE_INTRINSIC_MODULE(wasi)

typedef U32 WASIAddress;
#define WASIADDRESS_MAX UINT32_MAX

#define WASIADDRESS_FORMAT "0x%08x"

struct ExitException
{
	__wasi_exitcode_t exitCode;
};

struct ProcessResolver : Resolver
{
	HashMap<std::string, GCPointer<ModuleInstance>> moduleNameToInstanceMap;

	bool resolve(const std::string& moduleName,
				 const std::string& exportName,
				 ExternType type,
				 Object*& outObject) override
	{
		const auto& namedInstance = moduleNameToInstanceMap.get(moduleName);
		if(namedInstance)
		{
			outObject = getInstanceExport(*namedInstance, exportName);
			if(outObject)
			{
				if(isA(outObject, type)) { return true; }
				else
				{
					Log::printf(Log::debug,
								"Resolved import %s.%s to a %s, but was expecting %s\n",
								moduleName.c_str(),
								exportName.c_str(),
								asString(getExternType(outObject)).c_str(),
								asString(type).c_str());
					return false;
				}
			}
		}

		return false;
	}
};

namespace WAVM { namespace WASI {
	struct FD
	{
		VFS::FD* vfd;
		__wasi_rights_t rightsBase;
		__wasi_rights_t rightsInheriting;
	};

	struct Process
	{
		GCPointer<Compartment> compartment;
		GCPointer<Context> context;
		GCPointer<Memory> memory;
		GCPointer<ModuleInstance> moduleInstance;
		std::vector<std::string> args;
		std::vector<std::string> envs;

		IndexMap<__wasi_fd_t, WASI::FD> fds;

		ProcessResolver resolver;
	};
}}

std::atomic<SyscallTraceLevel> syscallTraceLevel{SyscallTraceLevel::none};

void WASI::setSyscallTraceLevel(SyscallTraceLevel newLevel)
{
	syscallTraceLevel.store(newLevel, std::memory_order_relaxed);
}

static void traceSyscallv(const char* syscallName, const char* argFormat, va_list argList)
{
	SyscallTraceLevel syscallTraceLevelSnapshot = syscallTraceLevel.load(std::memory_order_relaxed);
	if(syscallTraceLevelSnapshot != SyscallTraceLevel::none)
	{
		Log::printf(Log::output, "SYSCALL: %s", syscallName);
		Log::vprintf(Log::output, argFormat, argList);
		va_end(argList);

		if(syscallTraceLevelSnapshot != SyscallTraceLevel::syscallsWithCallstacks)
		{ Log::printf(Log::output, "\n"); }
		else
		{
			Log::printf(Log::output, " - Call stack:\n");

			Platform::CallStack callStack = Platform::captureCallStack(4);
			if(callStack.stackFrames.size() > 4) { callStack.stackFrames.resize(4); }
			std::vector<std::string> callStackFrameDescriptions
				= Runtime::describeCallStack(callStack);
			for(const std::string& frameDescription : callStackFrameDescriptions)
			{ Log::printf(Log::output, "SYSCALL:     %s\n", frameDescription.c_str()); }
		}
	}
}

VALIDATE_AS_PRINTF(2, 3)
static void traceSyscallf(const char* syscallName, const char* argFormat, ...)
{
	va_list argList;
	va_start(argList, argFormat);
	traceSyscallv(syscallName, argFormat, argList);
	va_end(argList);
}

VALIDATE_AS_PRINTF(2, 3)
static void traceSyscallReturnf(const char* syscallName, const char* returnFormat, ...)
{
	SyscallTraceLevel syscallTraceLevelSnapshot = syscallTraceLevel.load(std::memory_order_relaxed);
	if(syscallTraceLevelSnapshot != SyscallTraceLevel::none)
	{
		va_list argList;
		va_start(argList, returnFormat);
		Log::printf(Log::output, "SYSCALL: %s -> ", syscallName);
		Log::vprintf(Log::output, returnFormat, argList);
		Log::printf(Log::output, "\n");
		va_end(argList);
	}
}

VALIDATE_AS_PRINTF(2, 3)
static void traceUnimplementedSyscall(const char* syscallName, const char* argFormat, ...)
{
	va_list argList;
	va_start(argList, argFormat);
	traceSyscallv(syscallName, argFormat, argList);
	va_end(argList);

	Log::printf(Log::error, "Called unimplemented WASI syscall %s.\n", syscallName);

	traceSyscallReturnf(syscallName, "ENOSYS");
}

#define TRACE_SYSCALL(syscallName, argFormat, ...)                                                 \
	const char* TRACE_SYSCALL_name = syscallName;                                                  \
	traceSyscallf(TRACE_SYSCALL_name, argFormat, __VA_ARGS__)

#define TRACE_SYSCALL_RETURN(returnCode, ...)                                                      \
	traceSyscallReturnf(TRACE_SYSCALL_name, #returnCode __VA_ARGS__), __WASI_##returnCode

static WASI::FD* getFD(Process* process, __wasi_fd_t fd)
{
	if(fd < process->fds.getMinIndex() || fd > process->fds.getMaxIndex()) { return nullptr; }
	return process->fds.get(fd);
}

static bool checkFDRights(WASI::FD* wasiFD, __wasi_rights_t requiredRights)
{
	return (wasiFD->rightsBase & requiredRights) == requiredRights;
}

WASI::RunResult WASI::run(Runtime::ModuleConstRefParam module,
						  std::vector<std::string>&& inArgs,
						  std::vector<std::string>&& inEnvs,
						  I32& outExitCode)
{
	GCPointer<Compartment> compartment = createCompartment();
	Context* context = createContext(compartment);

	ProcessResolver processResolver;
	processResolver.moduleNameToInstanceMap.set(
		"wasi_unstable", instantiateModule(compartment, INTRINSIC_MODULE_REF(wasi), "wasi"));

	const IR::Module& moduleIR = getModuleIR(module);
	LinkResult linkResult = linkModule(moduleIR, processResolver);

	if(!linkResult.success)
	{
		for(const auto& missingImport : linkResult.missingImports)
		{
			Log::printf(Log::debug,
						"Couldn't resolve import %s.%s : %s\n",
						missingImport.moduleName.c_str(),
						missingImport.exportName.c_str(),
						asString(missingImport.type).c_str());
		}
		return RunResult::linkError;
	}

	ModuleInstance* moduleInstance = instantiateModule(
		compartment, module, std::move(linkResult.resolvedImports), "<main module>");

	Memory* memory = asMemoryNullable(getInstanceExport(moduleInstance, "memory"));

	IndexMap<__wasi_fd_t, FD> files(0, INT32_MAX);

	__wasi_rights_t stdioRights = __WASI_RIGHT_FD_READ | __WASI_RIGHT_FD_FDSTAT_SET_FLAGS
								  | __WASI_RIGHT_FD_WRITE | __WASI_RIGHT_FD_FILESTAT_GET
								  | __WASI_RIGHT_POLL_FD_READWRITE;

	files.insertOrFail(0, FD{Platform::getStdFD(VFS::StdDevice::in), stdioRights, 0});
	files.insertOrFail(1, FD{Platform::getStdFD(VFS::StdDevice::out), stdioRights, 0});
	files.insertOrFail(2, FD{Platform::getStdFD(VFS::StdDevice::err), stdioRights, 0});

	Process* process = new Process{compartment,
								   context,
								   memory,
								   moduleInstance,
								   std::move(inArgs),
								   std::move(inEnvs),
								   std::move(files),
								   std::move(processResolver)};
	setUserData(compartment, process, nullptr);

	try
	{
		Function* startFunction
			= asFunctionNullable(getInstanceExport(process->moduleInstance, "_start"));
		if(!startFunction)
		{
			Log::printf(Log::Category::debug, "WASI module did not export start function.\n");
			return RunResult::noStartFunction;
		}

		if(getFunctionType(startFunction) != FunctionType())
		{
			Log::printf(Log::Category::debug,
						"WASI module exported _start : %s but expected _start : %s.\n",
						asString(getFunctionType(startFunction)).c_str(),
						asString(FunctionType()).c_str());
			return RunResult::mistypedStartFunction;
		}

		invokeFunctionChecked(process->context, startFunction, {});
	}
	catch(const ExitException& exitException)
	{
		outExitCode = exitException.exitCode;
	}

	delete process;
	errorUnless(tryCollectCompartment(std::move(compartment)));

	return RunResult::success;
}

static Process* getProcessFromContextRuntimeData(ContextRuntimeData* contextRuntimeData)
{
	return (Process*)getUserData(getCompartmentFromContextRuntimeData(contextRuntimeData));
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "args_sizes_get",
						  __wasi_errno_t,
						  wasi_args_sizes_get,
						  U32 argcAddress,
						  U32 argBufSizeAddress)
{
	TRACE_SYSCALL("args_sizes_get",
				  "(" WASIADDRESS_FORMAT ", " WASIADDRESS_FORMAT ")",
				  argcAddress,
				  argBufSizeAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	Uptr numArgBufferBytes = 0;
	for(const std::string& arg : process->args) { numArgBufferBytes += arg.size() + 1; }

	if(process->args.size() > WASIADDRESS_MAX || numArgBufferBytes > WASIADDRESS_MAX)
	{ return TRACE_SYSCALL_RETURN(EOVERFLOW); }
	memoryRef<WASIAddress>(process->memory, argcAddress) = WASIAddress(process->args.size());
	memoryRef<WASIAddress>(process->memory, argBufSizeAddress) = WASIAddress(numArgBufferBytes);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "args_get",
						  __wasi_errno_t,
						  wasi_args_get,
						  U32 argvAddress,
						  U32 argBufAddress)
{
	TRACE_SYSCALL(
		"args_get", "(" WASIADDRESS_FORMAT ", " WASIADDRESS_FORMAT ")", argvAddress, argBufAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASIAddress nextArgBufAddress = argBufAddress;
	for(Uptr argIndex = 0; argIndex < process->args.size(); ++argIndex)
	{
		const std::string& arg = process->args[argIndex];
		const Uptr numArgBytes = arg.size() + 1;

		if(numArgBytes > WASIADDRESS_MAX || nextArgBufAddress > WASIADDRESS_MAX - numArgBytes - 1)
		{ return TRACE_SYSCALL_RETURN(EOVERFLOW); }

		Platform::bytewiseMemCopy(
			memoryArrayPtr<U8>(process->memory, nextArgBufAddress, numArgBytes),
			(const U8*)arg.c_str(),
			numArgBytes);
		memoryRef<WASIAddress>(process->memory, argvAddress + argIndex * sizeof(U32))
			= WASIAddress(nextArgBufAddress);

		nextArgBufAddress += WASIAddress(numArgBytes);
	}

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "environ_sizes_get",
						  __wasi_errno_t,
						  wasi_environ_sizes_get,
						  U32 envCountAddress,
						  U32 envBufSizeAddress)
{
	TRACE_SYSCALL("environ_sizes_get",
				  "(" WASIADDRESS_FORMAT ", " WASIADDRESS_FORMAT ")",
				  envCountAddress,
				  envBufSizeAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	Uptr numEnvBufferBytes = 0;
	for(const std::string& env : process->envs) { numEnvBufferBytes += env.size() + 1; }

	if(process->envs.size() > WASIADDRESS_MAX || numEnvBufferBytes > WASIADDRESS_MAX)
	{ return TRACE_SYSCALL_RETURN(EOVERFLOW); }
	memoryRef<WASIAddress>(process->memory, envCountAddress) = WASIAddress(process->envs.size());
	memoryRef<WASIAddress>(process->memory, envBufSizeAddress) = WASIAddress(numEnvBufferBytes);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "environ_get",
						  __wasi_errno_t,
						  wasi_environ_get,
						  U32 envvAddress,
						  U32 envBufAddress)
{
	TRACE_SYSCALL("environ_get",
				  "(" WASIADDRESS_FORMAT ", " WASIADDRESS_FORMAT ")",
				  envvAddress,
				  envBufAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASIAddress nextEnvBufAddress = envBufAddress;
	for(Uptr argIndex = 0; argIndex < process->envs.size(); ++argIndex)
	{
		const std::string& env = process->envs[argIndex];
		const Uptr numEnvBytes = env.size() + 1;

		if(numEnvBytes > WASIADDRESS_MAX || nextEnvBufAddress > WASIADDRESS_MAX - numEnvBytes - 1)
		{ return TRACE_SYSCALL_RETURN(EOVERFLOW); }

		Platform::bytewiseMemCopy(
			memoryArrayPtr<U8>(process->memory, nextEnvBufAddress, numEnvBytes),
			(const U8*)env.c_str(),
			numEnvBytes);
		memoryRef<WASIAddress>(process->memory, envvAddress + argIndex * sizeof(U32))
			= WASIAddress(nextEnvBufAddress);

		nextEnvBufAddress += WASIAddress(numEnvBytes);
	}

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "clock_res_get",
						  __wasi_errno_t,
						  __wasi_clock_res_get,
						  __wasi_clockid_t clockId,
						  WASIAddress resolutionAddress)
{
	TRACE_SYSCALL("clock_res_get", "(%u, " WASIADDRESS_FORMAT ")", clockId, resolutionAddress);

	I128 resolution128;
	switch(clockId)
	{
	case __WASI_CLOCK_REALTIME: resolution128 = Platform::getRealtimeClockResolution(); break;
	case __WASI_CLOCK_MONOTONIC: resolution128 = Platform::getMonotonicClockResolution(); break;
	case __WASI_CLOCK_PROCESS_CPUTIME_ID:
	case __WASI_CLOCK_THREAD_CPUTIME_ID:
		resolution128 = Platform::getProcessClockResolution();
		break;
	default: return TRACE_SYSCALL_RETURN(EINVAL);
	}
	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	__wasi_timestamp_t resolution = __wasi_timestamp_t(resolution128);
	memoryRef<__wasi_timestamp_t>(process->memory, resolutionAddress) = resolution;

	return TRACE_SYSCALL_RETURN(ESUCCESS, " (%" PRIu64 ")", resolution);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "clock_time_get",
						  __wasi_errno_t,
						  __wasi_clock_time_get,
						  __wasi_clockid_t clockId,
						  __wasi_timestamp_t precision,
						  WASIAddress timeAddress)
{
	TRACE_SYSCALL("clock_time_get",
				  "(%u, %" PRIu64 ", " WASIADDRESS_FORMAT ")",
				  clockId,
				  precision,
				  timeAddress);

	I128 currentTime128;
	switch(clockId)
	{
	case __WASI_CLOCK_REALTIME: currentTime128 = Platform::getRealtimeClock(); break;
	case __WASI_CLOCK_MONOTONIC: currentTime128 = Platform::getMonotonicClock(); break;
	case __WASI_CLOCK_PROCESS_CPUTIME_ID:
	case __WASI_CLOCK_THREAD_CPUTIME_ID: currentTime128 = Platform::getProcessClock(); break;
	default: return TRACE_SYSCALL_RETURN(EINVAL);
	}
	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	__wasi_timestamp_t currentTime = __wasi_timestamp_t(currentTime128);
	memoryRef<__wasi_timestamp_t>(process->memory, timeAddress) = currentTime;

	return TRACE_SYSCALL_RETURN(ESUCCESS, " (%" PRIu64 ")", currentTime);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_prestat_get",
						  __wasi_errno_t,
						  wasi_fd_prestat_get,
						  __wasi_fd_t fd,
						  WASIAddress prestatAddress)
{
	TRACE_SYSCALL("fd_prestat_get", "(%u, " WASIADDRESS_FORMAT ")", fd, prestatAddress);
	return TRACE_SYSCALL_RETURN(EBADF);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_prestat_dir_name",
						  __wasi_errno_t,
						  wasi_fd_prestat_dir_name,
						  __wasi_fd_t fd,
						  WASIAddress bufferAddress,
						  WASIAddress bufferLength)
{
	traceUnimplementedSyscall(
		"fd_prestat_dir_name", "(%u, " WASIADDRESS_FORMAT ", %u)", fd, bufferAddress, bufferLength);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi, "fd_close", __wasi_errno_t, wasi_fd_close, __wasi_fd_t fd)
{
	TRACE_SYSCALL("fd_close", "(%u)", fd);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }

	VFS::CloseResult result = wasiFD->vfd->close();
	switch(result)
	{
	case VFS::CloseResult::success:
		// If the close succeeded, remove the fd from the fds map.
		process->fds.removeOrFail(fd);

		return TRACE_SYSCALL_RETURN(ESUCCESS);

	case VFS::CloseResult::interrupted: return TRACE_SYSCALL_RETURN(EINTR);
	case VFS::CloseResult::ioError: return TRACE_SYSCALL_RETURN(EIO);

	default: Errors::unreachable();
	};
}

DEFINE_INTRINSIC_FUNCTION(wasi, "fd_datasync", __wasi_errno_t, wasi_fd_datasync, __wasi_fd_t fd)
{
	TRACE_SYSCALL("fd_datasync", "(%u)", fd);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_DATASYNC))
	{ return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	wasiFD->vfd->sync(VFS::SyncType::contents);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_pread",
						  __wasi_errno_t,
						  wasi_fd_pread,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  __wasi_filesize_t offset,
						  WASIAddress numBytesReadAddress)
{
	traceUnimplementedSyscall("fd_pread",
							  "(%u, " WASIADDRESS_FORMAT ", %u, %" PRIu64 ", " WASIADDRESS_FORMAT
							  ")",
							  fd,
							  iovsAddress,
							  numIOVs,
							  offset,
							  numBytesReadAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_pwrite",
						  __wasi_errno_t,
						  wasi_fd_pwrite,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  __wasi_filesize_t offset,
						  WASIAddress numBytesWrittenAddress)
{
	traceUnimplementedSyscall("fd_pwrite",
							  "(%u, " WASIADDRESS_FORMAT ", %u, %" PRIu64 ", " WASIADDRESS_FORMAT
							  ")",
							  fd,
							  iovsAddress,
							  numIOVs,
							  offset,
							  numBytesWrittenAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_read",
						  __wasi_errno_t,
						  wasi_fd_read,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  WASIAddress numBytesReadAddress)
{
	traceUnimplementedSyscall("fd_read",
							  "(%u, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT ")",
							  fd,
							  iovsAddress,
							  numIOVs,
							  numBytesReadAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_renumber",
						  __wasi_errno_t,
						  wasi_fd_renumber,
						  __wasi_fd_t from,
						  __wasi_fd_t to)
{
	traceUnimplementedSyscall("fd_renumber", "(%u, %u)", from, to);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_seek",
						  __wasi_errno_t,
						  wasi_fd_seek,
						  __wasi_fd_t fd,
						  __wasi_filedelta_t offset,
						  U32 whence,
						  WASIAddress newOffsetAddress)
{
	traceUnimplementedSyscall("fd_seek",
							  "(%u, %" PRIu64 "%u, " WASIADDRESS_FORMAT ")",
							  fd,
							  offset,
							  whence,
							  newOffsetAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_tell",
						  __wasi_errno_t,
						  wasi_fd_tell,
						  __wasi_fd_t fd,
						  WASIAddress offsetAddress)
{
	traceUnimplementedSyscall("fd_tell", "(%u, " WASIADDRESS_FORMAT ")", fd, offsetAddress);
	return __WASI_ENOSYS;
}

__wasi_filetype_t asWaSIFileType(VFS::FDType type)
{
	switch(type)
	{
	case VFS::FDType::unknown: return __WASI_FILETYPE_UNKNOWN;
	case VFS::FDType::blockDevice: return __WASI_FILETYPE_BLOCK_DEVICE;
	case VFS::FDType::characterDevice: return __WASI_FILETYPE_CHARACTER_DEVICE;
	case VFS::FDType::directory: return __WASI_FILETYPE_DIRECTORY;
	case VFS::FDType::file: return __WASI_FILETYPE_REGULAR_FILE;
	case VFS::FDType::datagramSocket: return __WASI_FILETYPE_SOCKET_DGRAM;
	case VFS::FDType::streamSocket: return __WASI_FILETYPE_SOCKET_STREAM;
	case VFS::FDType::symbolicLink: return __WASI_FILETYPE_SYMBOLIC_LINK;
	default: Errors::unreachable();
	};
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_fdstat_get",
						  __wasi_errno_t,
						  wasi_fd_fdstat_get,
						  __wasi_fd_t fd,
						  WASIAddress fdstatAddress)
{
	TRACE_SYSCALL("fd_fdstat_get", "(%u, " WASIADDRESS_FORMAT ")", fd, fdstatAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }

	VFS::FDInfo fdInfo;
	VFS::GetFDInfoResult result = wasiFD->vfd->getInfo(fdInfo);
	switch(result)
	{
	case VFS::GetFDInfoResult::success:
	{
		__wasi_fdstat_t& fdstat = memoryRef<__wasi_fdstat_t>(process->memory, fdstatAddress);
		fdstat.fs_filetype = asWaSIFileType(fdInfo.type);
		fdstat.fs_flags = 0;

		if(fdInfo.flags.append) { fdstat.fs_flags |= __WASI_FDFLAG_APPEND; }
		if(fdInfo.flags.nonBlocking) { fdstat.fs_flags |= __WASI_FDFLAG_NONBLOCK; }
		switch(fdInfo.flags.implicitSync)
		{
		case VFS::FDImplicitSync::none: break;
		case VFS::FDImplicitSync::syncContentsAfterWrite:
			fdstat.fs_flags |= __WASI_FDFLAG_DSYNC;
			break;
		case VFS::FDImplicitSync::syncContentsAndMetadataAfterWrite:
			fdstat.fs_flags |= __WASI_FDFLAG_SYNC;
			break;
		case VFS::FDImplicitSync::syncContentsAfterWriteAndBeforeRead:
			fdstat.fs_flags |= __WASI_FDFLAG_DSYNC | __WASI_FDFLAG_RSYNC;
			break;
		case VFS::FDImplicitSync::syncContentsAndMetadataAfterWriteAndBeforeRead:
			fdstat.fs_flags |= __WASI_FDFLAG_SYNC | __WASI_FDFLAG_RSYNC;
			break;
		default: Errors::unreachable();
		}

		fdstat.fs_rights_base = wasiFD->rightsBase;
		fdstat.fs_rights_inheriting = wasiFD->rightsInheriting;

		return TRACE_SYSCALL_RETURN(ESUCCESS);
	}

	case VFS::GetFDInfoResult::ioError: return TRACE_SYSCALL_RETURN(EIO);

	default: Errors::unreachable();
	};
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_fdstat_set_flags",
						  __wasi_errno_t,
						  wasi_fd_fdstat_set_flags,
						  __wasi_fd_t fd,
						  __wasi_fdflags_t flags)
{
	traceUnimplementedSyscall("fd_fdstat_set_flags", "(%u, 0x%04x)", fd, flags);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_fdstat_set_rights",
						  __wasi_errno_t,
						  wasi_fd_fdstat_set_rights,
						  __wasi_fd_t fd,
						  __wasi_rights_t rightsBase,
						  __wasi_rights_t rightsInheriting)
{
	traceUnimplementedSyscall("fd_fdstat_set_rights",
							  "(%u, 0x%" PRIx64 ", 0x %" PRIx64 ") ",
							  fd,
							  rightsBase,
							  rightsInheriting);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi, "fd_sync", __wasi_errno_t, wasi_fd_sync, __wasi_fd_t fd)
{
	TRACE_SYSCALL("fd_sync", "(%u)", fd);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_SYNC)) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	wasiFD->vfd->sync(VFS::SyncType::contentsAndMetadata);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_write",
						  __wasi_errno_t,
						  wasi_fd_write,
						  __wasi_fd_t fd,
						  WASIAddress iovsAddress,
						  WASIAddress numIOVs,
						  WASIAddress numBytesWrittenAddress)
{
	TRACE_SYSCALL("fd_write",
				  "(%u, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT ")",
				  fd,
				  iovsAddress,
				  numIOVs,
				  numBytesWrittenAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASI::FD* wasiFD = getFD(process, fd);
	if(!wasiFD) { return TRACE_SYSCALL_RETURN(EBADF); }
	if(!checkFDRights(wasiFD, __WASI_RIGHT_FD_WRITE)) { return TRACE_SYSCALL_RETURN(ENOTCAPABLE); }

	const __wasi_ciovec_t* iovs
		= memoryArrayPtr<__wasi_ciovec_t>(process->memory, iovsAddress, numIOVs);
	U64 numBytesWritten = 0;
	VFS::WriteResult writeResult = VFS::WriteResult::success;
	for(WASIAddress iovIndex = 0; iovIndex < numIOVs; ++iovIndex)
	{
		Uptr numBytesWrittenThisIO = 0;
		writeResult = wasiFD->vfd->write(
			memoryArrayPtr<U8>(process->memory, iovs[iovIndex].buf, iovs[iovIndex].buf_len),
			iovs[iovIndex].buf_len,
			&numBytesWrittenThisIO);
		if(writeResult != VFS::WriteResult::success) { break; }

		numBytesWritten += numBytesWrittenThisIO;
	}

	if(numBytesWritten > WASIADDRESS_MAX) { return TRACE_SYSCALL_RETURN(EOVERFLOW); }
	memoryRef<WASIAddress>(process->memory, numBytesWrittenAddress) = WASIAddress(numBytesWritten);

	switch(writeResult)
	{
	case VFS::WriteResult::success:
		return TRACE_SYSCALL_RETURN(ESUCCESS, " (numBytesWritten=%" PRIu64 ")", numBytesWritten);
	case VFS::WriteResult::ioError: return TRACE_SYSCALL_RETURN(EIO);
	case VFS::WriteResult::interrupted: return TRACE_SYSCALL_RETURN(EINTR);
	case VFS::WriteResult::outOfMemory: return TRACE_SYSCALL_RETURN(ENOMEM);
	case VFS::WriteResult::outOfQuota: return TRACE_SYSCALL_RETURN(EDQUOT);
	case VFS::WriteResult::outOfFreeSpace: return TRACE_SYSCALL_RETURN(ENOSPC);
	case VFS::WriteResult::exceededFileSizeLimit: return TRACE_SYSCALL_RETURN(EFBIG);
	case VFS::WriteResult::notPermitted: return TRACE_SYSCALL_RETURN(EPERM);

	default: Errors::unreachable();
	}
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_advise",
						  __wasi_errno_t,
						  wasi_fd_advise,
						  __wasi_fd_t fd,
						  __wasi_filesize_t offset,
						  __wasi_filesize_t numBytes,
						  __wasi_advice_t advice)
{
	traceUnimplementedSyscall(
		"fd_advise", "(%u, %" PRIu64 ", %" PRIu64 ", 0x%02x)", fd, offset, numBytes, advice);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_allocate",
						  __wasi_errno_t,
						  wasi_fd_allocate,
						  __wasi_fd_t fd,
						  __wasi_filesize_t offset,
						  __wasi_filesize_t numBytes)
{
	traceUnimplementedSyscall(
		"fd_allocate", "(%u, %" PRIu64 ", %" PRIu64 ")", fd, offset, numBytes);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "path_link",
						  __wasi_errno_t,
						  wasi_path_link,
						  __wasi_fd_t oldFD,
						  __wasi_lookupflags_t oldFlags,
						  WASIAddress oldPathAddress,
						  WASIAddress numOldPathBytes,
						  __wasi_fd_t newFD,
						  WASIAddress newPathAddress,
						  WASIAddress numNewPathBytes)
{
	traceUnimplementedSyscall("path_link",
							  "(%u, 0x%08x, " WASIADDRESS_FORMAT ", %u, %u, " WASIADDRESS_FORMAT
							  ", %u)",
							  oldFD,
							  oldFlags,
							  oldPathAddress,
							  numOldPathBytes,
							  newFD,
							  newPathAddress,
							  numNewPathBytes);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "path_open",
						  __wasi_errno_t,
						  wasi_path_open,
						  __wasi_fd_t dirfd,
						  __wasi_lookupflags_t dirflags,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes,
						  __wasi_oflags_t oflags,
						  __wasi_rights_t rightsBase,
						  __wasi_rights_t rightsInheriting,
						  __wasi_fdflags_t fdFlags,
						  WASIAddress fdAddress)
{
	traceUnimplementedSyscall("path_open",
							  "(%u, 0x%08x, " WASIADDRESS_FORMAT ", %u, 0x%04x, 0x%" PRIx64
							  ", 0x%" PRIx64 ", 0x%04x, " WASIADDRESS_FORMAT ")",
							  dirfd,
							  dirflags,
							  pathAddress,
							  numPathBytes,
							  oflags,
							  rightsBase,
							  rightsInheriting,
							  fdFlags,
							  fdAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_readdir",
						  __wasi_errno_t,
						  wasi_fd_readdir,
						  __wasi_fd_t fd,
						  WASIAddress bufferAddress,
						  WASIAddress numBufferBytes,
						  __wasi_dircookie_t cookie,
						  WASIAddress outNumBufferBytesUsedAddress)
{
	traceUnimplementedSyscall("fd_readdir",
							  "(%u, " WASIADDRESS_FORMAT ", %u, 0x%" PRIx64 ", " WASIADDRESS_FORMAT
							  ")",
							  fd,
							  bufferAddress,
							  numBufferBytes,
							  cookie,
							  outNumBufferBytesUsedAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "path_readlink",
						  __wasi_errno_t,
						  wasi_path_readlink,
						  __wasi_fd_t fd,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes,
						  WASIAddress bufferAddress,
						  WASIAddress numBufferBytes,
						  WASIAddress outNumBufferBytesUsedAddress)
{
	traceUnimplementedSyscall("path_readlink",
							  "(%u, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT
							  ", %u, " WASIADDRESS_FORMAT ")",
							  fd,
							  pathAddress,
							  numPathBytes,
							  bufferAddress,
							  numBufferBytes,
							  outNumBufferBytesUsedAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "path_rename",
						  __wasi_errno_t,
						  wasi_path_rename,
						  __wasi_fd_t oldFD,
						  WASIAddress oldPathAddress,
						  WASIAddress numOldPathBytes,
						  __wasi_fd_t newFD,
						  WASIAddress newPathAddress,
						  WASIAddress numNewPathBytes)
{
	traceUnimplementedSyscall("path_rename",
							  "(%u, " WASIADDRESS_FORMAT ", %u, %u, " WASIADDRESS_FORMAT ", %u)",
							  oldFD,
							  oldPathAddress,
							  numOldPathBytes,
							  newFD,
							  newPathAddress,
							  numNewPathBytes);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_filestat_get",
						  __wasi_errno_t,
						  wasi_fd_filestat_get,
						  __wasi_fd_t fd,
						  WASIAddress fdstatAddress)
{
	traceUnimplementedSyscall("fd_filestat_get", "(%u, " WASIADDRESS_FORMAT ")", fd, fdstatAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_filestat_set_times",
						  __wasi_errno_t,
						  wasi_fd_filestat_set_times,
						  __wasi_fd_t fd,
						  __wasi_timestamp_t st_atim,
						  __wasi_timestamp_t st_mtim,
						  __wasi_fstflags_t fstflags)
{
	traceUnimplementedSyscall("fd_filestat_set_times",
							  "(%u, %" PRIu64 ", %" PRIu64 ", 0x%04x)",
							  fd,
							  st_atim,
							  st_mtim,
							  fstflags);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "fd_filestat_set_size",
						  __wasi_errno_t,
						  wasi_fd_filestat_set_size,
						  __wasi_fd_t fd,
						  __wasi_filesize_t numBytes)
{
	traceUnimplementedSyscall("fd_filestat_set_size", "(%u, %" PRIu64 ")", fd, numBytes);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "path_filestat_get",
						  __wasi_errno_t,
						  wasi_path_filestat_get,
						  __wasi_fd_t fd,
						  __wasi_lookupflags_t flags,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes,
						  WASIAddress filestatAddress)
{
	traceUnimplementedSyscall("path_filestat_get",
							  "(%u, 0x%08x, " WASIADDRESS_FORMAT ", %u, " WASIADDRESS_FORMAT ")",
							  fd,
							  flags,
							  pathAddress,
							  numPathBytes,
							  filestatAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "path_filestat_set_times",
						  __wasi_errno_t,
						  wasi_path_filestat_set_times,
						  __wasi_fd_t fd,
						  __wasi_lookupflags_t flags,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes,
						  __wasi_timestamp_t st_atim,
						  __wasi_timestamp_t st_mtim,
						  __wasi_fstflags_t fstflags)
{
	traceUnimplementedSyscall("path_filestat_set_times",
							  "(%u, 0x%08x, " WASIADDRESS_FORMAT ", %u, %" PRIu64 ", %" PRIu64
							  ", 0x%04x)",
							  fd,
							  flags,
							  pathAddress,
							  numPathBytes,
							  st_atim,
							  st_mtim,
							  fstflags);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "path_symlink",
						  __wasi_errno_t,
						  wasi_path_symlink,
						  WASIAddress oldPathAddress,
						  WASIAddress numOldPathBytes,
						  __wasi_fd_t fd,
						  WASIAddress newPathAddress,
						  WASIAddress numNewPathBytes)
{
	traceUnimplementedSyscall("path_symlink",
							  "(" WASIADDRESS_FORMAT ", %u, %u, " WASIADDRESS_FORMAT ", %u)",
							  oldPathAddress,
							  numOldPathBytes,
							  fd,
							  newPathAddress,
							  numNewPathBytes);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "path_unlink_file",
						  __wasi_errno_t,
						  wasi_path_unlink_file,
						  __wasi_fd_t fd,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes)
{
	traceUnimplementedSyscall(
		"path_unlink_file", "(%u, " WASIADDRESS_FORMAT ", %u)", fd, pathAddress, numPathBytes);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "path_remove_directory",
						  __wasi_errno_t,
						  wasi_path_remove_directory,
						  __wasi_fd_t fd,
						  WASIAddress pathAddress,
						  WASIAddress numPathBytes)
{
	traceUnimplementedSyscall(
		"path_remove_directory", "(%u, " WASIADDRESS_FORMAT ", %u)", fd, pathAddress, numPathBytes);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "poll_oneoff",
						  __wasi_errno_t,
						  wasi_poll_oneoff,
						  WASIAddress inAddress,
						  WASIAddress outAddress,
						  WASIAddress numSubscriptions,
						  WASIAddress outNumEventsAddress)
{
	traceUnimplementedSyscall("poll_oneoff",
							  "(" WASIADDRESS_FORMAT ", " WASIADDRESS_FORMAT
							  ", %u, " WASIADDRESS_FORMAT ")",
							  inAddress,
							  outAddress,
							  numSubscriptions,
							  outNumEventsAddress);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi, "proc_exit", void, wasi_proc_exit, __wasi_exitcode_t exitCode)
{
	TRACE_SYSCALL("proc_exit", "(%u)", exitCode);
	throw ExitException{exitCode};
}

DEFINE_INTRINSIC_FUNCTION(wasi, "proc_raise", __wasi_errno_t, wasi_proc_raise, __wasi_signal_t sig)
{
	traceUnimplementedSyscall("proc_raise", "(%u)", sig);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "random_get",
						  __wasi_errno_t,
						  wasi_random_get,
						  WASIAddress bufferAddress,
						  WASIAddress numBufferBytes)
{
	TRACE_SYSCALL("random_get", "(" WASIADDRESS_FORMAT ", %u)", bufferAddress, numBufferBytes);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	U8* buffer = memoryArrayPtr<U8>(process->memory, bufferAddress, numBufferBytes);
	Platform::getCryptographicRNG(buffer, numBufferBytes);

	return TRACE_SYSCALL_RETURN(ESUCCESS);
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "sock_recv",
						  __wasi_errno_t,
						  wasi_sock_recv,
						  __wasi_fd_t sock,
						  WASIAddress ri_data,
						  WASIAddress ri_data_len,
						  __wasi_riflags_t ri_flags,
						  WASIAddress ro_datalen,
						  WASIAddress ro_flags)
{
	traceUnimplementedSyscall("sock_recv",
							  "(%u, " WASIADDRESS_FORMAT ", %u, 0x%04x, " WASIADDRESS_FORMAT
							  ", " WASIADDRESS_FORMAT ")",
							  sock,
							  ri_data,
							  ri_data_len,
							  ri_flags,
							  ro_datalen,
							  ro_flags);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "sock_send",
						  __wasi_errno_t,
						  wasi_sock_send,
						  __wasi_fd_t sock,
						  WASIAddress si_data,
						  WASIAddress si_data_len,
						  __wasi_siflags_t si_flags,
						  WASIAddress so_datalen)
{
	traceUnimplementedSyscall("sock_send",
							  "(%u, " WASIADDRESS_FORMAT ", %u, 0x%04x, " WASIADDRESS_FORMAT ")",
							  sock,
							  si_data,
							  si_data_len,
							  si_flags,
							  so_datalen);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi,
						  "sock_shutdown",
						  __wasi_errno_t,
						  wasi_sock_shutdown,
						  __wasi_fd_t sock,
						  __wasi_sdflags_t how)
{
	traceUnimplementedSyscall("sock_shutdown", "(%u, 0x%02x)", sock, how);
	return __WASI_ENOSYS;
}

DEFINE_INTRINSIC_FUNCTION(wasi, "sched_yield", __wasi_errno_t, wasi_sched_yield)
{
	traceUnimplementedSyscall("sched_yield", "()");
	return __WASI_ENOSYS;
}
