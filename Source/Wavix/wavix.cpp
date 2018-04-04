#include "Inline/BasicTypes.h"
#include "Inline/Timing.h"
#include "Platform/Platform.h"
#include "WAST/WAST.h"
#include "Runtime/Runtime.h"
#include "Runtime/Linker.h"
#include "Runtime/Intrinsics.h"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Validate.h"
#include "wavix.h"

#include "../Programs/CLI.h"

#include <map>
#include <atomic>
#include <thread>

using namespace IR;

thread_local Thread* currentThread = nullptr;
std::string sysroot;
bool isTracingSyscalls = false;

DEFINE_INTRINSIC_MODULE(wavix);

DEFINE_INTRINSIC_FUNCTION(wavix,"__invalid_syscall",I32,__invalid_syscall,
	I32 n,
	I32 a,
	I32 b,
	I32 c,
	I32 d,
	I32 e,
	I32 f)
{
	std::cerr << "__invalid_syscall: " << n << std::endl;
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
	return coerce32bitAddress(currentThread->process->args.size());
}

DEFINE_INTRINSIC_FUNCTION(wavix,"__wavix_get_arg_length",I32,__wavix_get_arg_length,I32 argIndex)
{
	if(U32(argIndex) < currentThread->process->args.size())
	{
		const Uptr safeArgIndex = Platform::saturateToBounds((Uptr)argIndex,currentThread->process->args.size());
		return coerce32bitAddress(currentThread->process->args[safeArgIndex].size() + 1);
	}
	else
	{
		throwException(Exception::accessViolationType);
	}
}

DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__wavix_get_arg",void,__wavix_get_arg,
	I32 argIndex, I32 bufferAddress, I32 numCharsInBuffer)
{
	if(U32(argIndex) < currentThread->process->args.size())
	{
		const Uptr safeArgIndex = Platform::saturateToBounds((Uptr)argIndex,currentThread->process->args.size());
		const Uptr numChars = currentThread->process->args[safeArgIndex].size();
		if(numChars + 1 <= Uptr(numCharsInBuffer))
	{
			memcpy(
				memoryArrayPtr<char>(currentThread->process->memory,bufferAddress,numCharsInBuffer),
				currentThread->process->args[safeArgIndex].c_str(),
				numChars);
			memoryRef<char>(currentThread->process->memory,bufferAddress + numChars) = 0;
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
	traceSyscallf("uname","(0x%08x)",resultAddress);
	wavix_utsname& result = memoryRef<wavix_utsname>(currentThread->process->memory,resultAddress);
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

	wavix_timespec& result = memoryRef<wavix_timespec>(currentThread->process->memory,resultAddress);

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

struct RootResolver : Resolver
{
	std::map<std::string,ModuleInstance*> moduleNameToInstanceMap;

	bool resolve(const std::string& moduleName,const std::string& exportName,ObjectType type,Object*& outObject) override
	{
		auto namedInstanceIt = moduleNameToInstanceMap.find(moduleName);
		if(namedInstanceIt != moduleNameToInstanceMap.end())
		{
			outObject = getInstanceExport(namedInstanceIt->second,exportName);
			return outObject && isA(outObject,type);
		}
		return false;
	}
};

struct CommandLineOptions
{
	const char* filename = nullptr;
	char** args = nullptr;
	bool onlyCheck = false;
};

struct MainThreadArgs
{
	Thread* thread;
	ModuleInstance* moduleInstance;
};

static I64 mainThreadEntry(const MainThreadArgs* args)
{
	currentThread = args->thread;

	// Call the module start function, if it has one.
	FunctionInstance* startFunction = getStartFunction(args->moduleInstance);
	if(startFunction)
	{
		// Validation should reject the module if the start function isn't ()->()
		assert(getFunctionType(startFunction) == FunctionType::get());
		invokeFunctionUnchecked(currentThread->context,startFunction,nullptr);
	}

	// Look up the module's exported main function (called _start).
	FunctionInstance* functionInstance = asFunctionNullable(getInstanceExport(args->moduleInstance,"_start"));
	if(!functionInstance)
	{
		std::cerr << "Module does not export _start function" << std::endl;
		return EXIT_FAILURE;
	}
	const FunctionType* functionType = getFunctionType(functionInstance);

	if(functionType != FunctionType::get())
	{
		std::cerr << "Module _start signature is " << asString(functionType) << ", but ()->() was expected." << std::endl;
		return EXIT_FAILURE;
	}

	// Call the main function.
	return invokeFunctionUnchecked(currentThread->context,functionInstance,nullptr)->i64;
}

static void unhandledExceptionHandler(Exception&& exception)
{
	Errors::fatalf("Unhandled runtime exception: %s\n",describeException(exception).c_str());
}

static I64 run(const CommandLineOptions& options)
{
	std::string hostFilename = sysroot + options.filename;
	Module module;

	// Enable some additional "features" in WAVM that are disabled by default.
	module.featureSpec.importExportMutableGlobals = true;
	module.featureSpec.sharedTables = true;
	// Allow atomics on unshared memories to accomodate atomics on the Emscripten memory.
	module.featureSpec.requireSharedFlagForAtomicOperators = false;

	// Load the module.
	if(!loadBinaryModule(hostFilename.c_str(),module)) { return EXIT_FAILURE; }
	if(options.onlyCheck) { return EXIT_SUCCESS; }

	// Create the process and compartment.
	Process* process = new Process;
	process->compartment = Runtime::createCompartment();

	process->args.push_back(options.filename);
	char** args = options.args;
	while(*args) { process->args.push_back(*args++); };

	// Link the module with the Wavix intrinsics.
	RootResolver rootResolver;
	ModuleInstance* wavixIntrinsicModuleInstance = Intrinsics::instantiateModule(
		process->compartment,
		INTRINSIC_MODULE_REF(wavix));
	rootResolver.moduleNameToInstanceMap["env"] = wavixIntrinsicModuleInstance;

	LinkResult linkResult = linkModule(module,rootResolver);
	if(!linkResult.success)
	{
		std::cerr << "Failed to link module:" << std::endl;
		for(auto& missingImport : linkResult.missingImports)
		{
			std::cerr << "Missing import: module=\"" << missingImport.moduleName
				<< "\" export=\"" << missingImport.exportName
				<< "\" type=\"" << asString(missingImport.type) << "\"" << std::endl;
		}
		return EXIT_FAILURE;
	}

	// Instantiate the module.
	ModuleInstance* moduleInstance = instantiateModule(process->compartment,module,std::move(linkResult.resolvedImports));
	if(!moduleInstance) { return EXIT_FAILURE; }

	process->memory = asMemoryNullable(getInstanceExport(moduleInstance,"__memory"));
	process->table = asTableNullable(getInstanceExport(moduleInstance,"__table"));
	if(!process->memory || !process->table)
	{
		std::cerr << "Module does not export memory and table" << std::endl;
		return EXIT_FAILURE;
	}

	// Create the context and Wavix Thread object for the main thread.
	Context* mainContext = Runtime::createContext(process->compartment);
	Thread* mainThread = new Thread(process,mainContext);
	process->threads.push_back(mainThread);

	// Start the main thread, and wait for it to exit.
	MainThreadArgs mainThreadArgs;
	mainThreadArgs.thread = mainThread;
	mainThreadArgs.moduleInstance = moduleInstance;
	enum { mainThreadNumStackBytes = 1*1024*1024 };
	Platform::Thread* mainPlatformThread = Platform::createThread(
		mainThreadNumStackBytes,
		(I64(*)(void*))&mainThreadEntry,
		&mainThreadArgs);
	const I64 result = Platform::joinThread(mainPlatformThread);

	// Clean up the process.
	delete process;
	process = nullptr;

	return result;
}

void showHelp()
{
	std::cerr << "Usage: wavix [options] <executable module path> [--] [arguments]" << std::endl;
	std::cerr << "  in.wast|in.wasm\t\tSpecify program file (.wast/.wasm)" << std::endl;
	std::cerr << "  -c|--check        Exit after checking that the program is valid" << std::endl;
	std::cerr << "  -d|--debug        Write additional debug information to stdout" << std::endl;
	std::cerr << "  --trace-syscalls  Trace Wavix syscalls to stdout" << std::endl;
	std::cerr << "  --sysroot <path>  Sets the system root directory to the given path." << std::endl;
	std::cerr << "                      Defaults to the CWD. All Wavix file accesses will be" << std::endl;
	std::cerr << "                      relative to sysroot, including the executable module path." << std::endl;
	std::cerr << "  --                Stop parsing arguments" << std::endl;
	//           "--------------------------------------------------------------------------------" <- 80 chars wide
}

extern void staticInitializeFile();
extern void staticInitializeMemory();
extern void staticInitializeProcess();

int main(int argc,char** argv)
{
	staticInitializeFile();
	staticInitializeMemory();
	staticInitializeProcess();

	sysroot = Platform::getCurrentWorkingDirectory();

	CommandLineOptions options;
	options.args = argv;
	while(*++options.args)
	{
		if(!strcmp(*options.args, "--sysroot"))
		{
			if(*++options.args == nullptr)
			{
				std::cerr << "Expected path following '--sysroot', but it was the last argument." << std::endl;
				return EXIT_FAILURE;
			}
			sysroot = *options.args;
		}
		else if(!strcmp(*options.args, "--check") || !strcmp(*options.args, "-c"))
		{
			options.onlyCheck = true;
		}
		else if(!strcmp(*options.args, "--debug") || !strcmp(*options.args, "-d"))
		{
			Log::setCategoryEnabled(Log::Category::debug,true);
		}
		else if(!strcmp(*options.args, "--trace-syscalls"))
		{
			isTracingSyscalls = true;
		}
		else if(!strcmp(*options.args, "--"))
		{
			++options.args;
			break;
		}
		else if(!strcmp(*options.args, "--help") || !strcmp(*options.args, "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else if(!options.filename)
		{
			options.filename = *options.args;
		}
		else { break; }
	}

	if(!options.filename)
	{
		showHelp();
		return EXIT_FAILURE;
	}

	// Instead of catching unhandled exceptions/signals, register a global handler.
	Runtime::setUnhandledExceptionHandler(unhandledExceptionHandler);

	I64 returnCode = EXIT_FAILURE;
#ifdef __AFL_LOOP
	while(__AFL_LOOP(2000))
#endif
	{
		returnCode = run(options);
		Runtime::collectGarbage();
	}
	return (int)returnCode;
}
