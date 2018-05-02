#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Timing.h"
#include "Platform/Platform.h"
#include "process.h"
#include "WAST/WAST.h"
#include "Runtime/Runtime.h"
#include "Runtime/Intrinsics.h"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Validate.h"
#include "wavix.h"

#include <atomic>
#include <iostream>

using namespace IR;
using namespace Runtime;

namespace Wavix
{
	std::string sysroot;
	bool isTracingSyscalls = false;

	DEFINE_INTRINSIC_MODULE(wavix);
		
	extern void staticInitializeFile();
	extern void staticInitializeMemory();
	extern void staticInitializeProcess();

	DEFINE_INTRINSIC_FUNCTION(wavix,"__invalid_syscall",I32,__invalid_syscall,
		I32 n,
		I32 a,
		I32 b,
		I32 c,
		I32 d,
		I32 e,
		I32 f)
	{
		Log::printf(Log::Category::error,"__invalid_syscall: %i\n",n);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_membarrier",I32,__syscall_membarrier,I32 dummy)
	{
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"setjmp",I32,wavix_setjmp,I32 bufferAddress)
	{
		traceSyscallf("setjmp","(0x%08x)",bufferAddress);
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"longjmp",void,wavix_longjmp,I32 a,I32 b)
	{
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_futex",I32,__syscall_futex,I32 a,I32 b,I32 c,I32 d,I32 e,I32 f)
	{
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	// Command-line arguments

	DEFINE_INTRINSIC_FUNCTION(wavix,"__wavix_get_num_args",I32,__wavix_get_num_args,I32 a)
	{
		return coerce32bitAddress(currentProcess->args.size());
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__wavix_get_arg_length",I32,__wavix_get_arg_length,I32 argIndex)
	{
		if(U32(argIndex) < currentProcess->args.size())
		{
			const Uptr safeArgIndex = Platform::saturateToBounds((Uptr)argIndex,currentProcess->args.size());
			return coerce32bitAddress(currentProcess->args[safeArgIndex].size() + 1);
		}
		else
		{
			throwException(Exception::accessViolationType);
		}
	}

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__wavix_get_arg",void,__wavix_get_arg,
		I32 argIndex, I32 bufferAddress, I32 numCharsInBuffer)
	{
		MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData,defaultMemoryId.id);
		if(U32(argIndex) < currentProcess->args.size())
		{
			const Uptr safeArgIndex = Platform::saturateToBounds((Uptr)argIndex,currentProcess->args.size());
			const Uptr numChars = currentProcess->args[safeArgIndex].size();
			if(numChars + 1 <= Uptr(numCharsInBuffer))
			{
				memcpy(
					memoryArrayPtr<char>(memory,bufferAddress,numCharsInBuffer),
					currentProcess->args[safeArgIndex].c_str(),
					numChars);
				memoryRef<char>(memory,bufferAddress + numChars) = 0;
			}
			else
			{
				throwException(Exception::accessViolationType);
			}
		}
		else
		{
			throwException(Exception::accessViolationType);
		}
	}

	// Resource limits/usage

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_setrlimit",I32,__syscall_setrlimit,I32 a,I32 b)
	{
		traceSyscallf("setrlimit","(%i,%i)",a,b);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}
	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getrlimit",I32,__syscall_getrlimit,I32 a,I32 b)
	{
		traceSyscallf("getrlimit","(%i,%i)",a,b);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}
	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_ugetrlimit",I32,__syscall_ugetrlimit,I32 a,I32 b)
	{
		traceSyscallf("ugetrlimit","(%i,%i)",a,b);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}
	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_prlimit64",I32,__syscall_prlimit64,I32 a,I32 b,I32 c,I32 d)
	{
		traceSyscallf("prlimit64","(%i,%i,%i,%i)",a,b,c,d);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}
	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getrusage",I32,__syscall_getrusage,I32 a,I32 b)
	{
		traceSyscallf("getrusage","(%i,%i)",a,b);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	// Sockets

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_socketcall",I32,__syscall_socketcall,I32 a,I32 b)
	{
		traceSyscallf("socketcall","(%i,%i)",a,b);
		return -1;
		//throwException(Exception::calledUnimplementedIntrinsicType);
	}

	// System information

	struct wavix_utsname
	{
		char sysName[65];
		char nodeName[65];
		char release[65];
		char version[65];
		char machine[65];
		char domainName[65];
	};

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__syscall_uname",I32,__syscall_uname,I32 resultAddress)
	{
		MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData,defaultMemoryId.id);
		traceSyscallf("uname","(0x%08x)",resultAddress);
		wavix_utsname& result = memoryRef<wavix_utsname>(memory,resultAddress);
		strcpy(result.sysName,"Wavix");
		strcpy(result.nodeName,"utsname::nodename");
		strcpy(result.release,"utsname::release");
		strcpy(result.version,"utsname::version");
		strcpy(result.machine,"wasm32");
		strcpy(result.domainName,"utsname::domainname");
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_sysinfo",I32,__syscall_sysinfo,I32 a)
	{
		traceSyscallf("sysinfo","(%i)",a);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	// Signals

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_rt_sigaction",I32,__syscall_rt_sigaction,I32 a,I32 b,I32 c)
	{
		traceSyscallf("rt_sigaction","(%u,%u,%u)",a,b,c);
		//throwException(Exception::calledUnimplementedIntrinsicType);
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_tgkill",I32,__syscall_tgkill,I32 a,I32 b,I32 c)
	{
			throwException(Exception::calledUnimplementedIntrinsicType);
	}

	// Time

	enum class ClockId : I32
	{
		realtime = 0,
		monotonic = 1,
	};

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__syscall_clock_gettime",I32,__syscall_clock_gettime,
		I32 clockId, I32 resultAddress)
	{
		traceSyscallf("clock_gettime","(%u,0x%08x)",clockId,resultAddress);

		MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData,defaultMemoryId.id);
		wavix_timespec& result = memoryRef<wavix_timespec>(memory,resultAddress);

		static std::atomic<U64> hackedClock;
		const U64 currentClock = hackedClock;

		switch((ClockId)clockId)
		{
		case ClockId::realtime:
		case ClockId::monotonic:
			result.tv_sec = I32(currentClock / 1000000000);
			result.tv_nsec = I32(currentClock % 1000000000);
			++hackedClock;
			break;
		default:
			throwException(Exception::calledUnimplementedIntrinsicType);
		};

		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_gettimeofday",I32,__syscall_gettimeofday,I32 a,I32 b)
	{
		traceSyscallf("gettimeofday","(%i,%i)",a,b);
			throwException(Exception::calledUnimplementedIntrinsicType);
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_setitimer",I32,__syscall_setitimer,I32 a,I32 b,I32 c)
	{
		traceSyscallf("setitimer","(%i,%i,%i)",a,b,c);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	// Users/groups

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getuid32",I32,__syscall_getuid32,I32 dummy)
	{
		traceSyscallf("getuid32","");
		return 1;
	}
	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getgid32",I32,__syscall_getgid32,I32 dummy)
	{
		traceSyscallf("getgid32","");
		return 1;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_geteuid32",I32,__syscall_geteuid32,I32 dummy)
	{
		traceSyscallf("geteuid32","");
		return 1;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getegid32",I32,__syscall_getegid32,I32 dummy)
	{
		traceSyscallf("geteuid32","");
		return 1;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_setreuid32",I32,__syscall_setreuid32,I32 a,I32 b)
	{
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_setregid32",I32,__syscall_setregid32,I32 a,I32 b)
	{
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getgroups32",I32,__syscall_getgroups32,I32 a,I32 b)
	{
		throwException(Exception::calledUnimplementedIntrinsicType);
	}
}

static void unhandledExceptionHandler(Exception&& exception)
{
	Errors::fatalf("Unhandled runtime exception: %s\n",describeException(exception).c_str());
}

void showHelp()
{
	std::cerr << "Usage: wavix [options] <executable module path> [--] [arguments]" << std::endl;
	std::cerr << "  in.wast|in.wasm\t\tSpecify program file (.wast/.wasm)" << std::endl;
	std::cerr << "  --trace-syscalls  Trace Wavix syscalls to stdout" << std::endl;
	std::cerr << "  --sysroot <path>  Sets the system root directory to the given path." << std::endl;
	std::cerr << "                      Defaults to the CWD. All Wavix file accesses will be" << std::endl;
	std::cerr << "                      relative to sysroot, including the executable module path." << std::endl;
	std::cerr << "  --                Stop parsing arguments" << std::endl;
	//           "--------------------------------------------------------------------------------" <- 80 chars wide
}

int main(int argc,const char** argv)
{
	Wavix::staticInitializeFile();
	Wavix::staticInitializeMemory();
	Wavix::staticInitializeProcess();

	Wavix::sysroot = Platform::getCurrentWorkingDirectory();

	const char* filename = nullptr;
	while(*++argv)
	{
		if(!strcmp(*argv, "--sysroot"))
		{
			if(*++argv == nullptr)
			{
				std::cerr << "Expected path following '--sysroot', but it was the last argument." << std::endl;
				return EXIT_FAILURE;
			}
			Wavix::sysroot = *argv;
		}
		else if(!strcmp(*argv, "--trace-syscalls"))
		{
			Wavix::isTracingSyscalls = true;
		}
		else if(!strcmp(*argv, "--"))
		{
			++argv;
			break;
		}
		else if(!strcmp(*argv, "--help") || !strcmp(*argv, "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else if(!filename)
		{
			filename = *argv;
		}
		else { break; }
	}

	std::vector<std::string> processArgs;
	while(*argv) { processArgs.push_back(*argv++); };

	if(!filename)
	{
		showHelp();
		return EXIT_FAILURE;
	}

	// Instead of catching unhandled exceptions/signals, register a global handler.
	Runtime::setUnhandledExceptionHandler(unhandledExceptionHandler);

	Wavix::Process* process = Wavix::spawnProcess(filename,processArgs,{},"/");
	if(!process) { return EXIT_FAILURE; }

	while(true) {};

	return EXIT_SUCCESS;
}
