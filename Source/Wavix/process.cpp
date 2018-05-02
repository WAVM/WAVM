#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/ConcurrentHashMap.h"
#include "Inline/HashMap.h"
#include "Inline/IndexAllocator.h"
#include "IR/Module.h"
#include "IR/Types.h"
#include "Platform/Platform.h"
#include "process.h"
#include "Runtime/Linker.h"
#include "Logging/Logging.h"
#include "Runtime/Intrinsics.h"
#include "wavix.h"

#include "../Programs/CLI.h"

using namespace IR;
using namespace Runtime;

namespace Wavix
{
	thread_local Process* currentProcess = nullptr;

	static ConcurrentHashMap<Uptr, Process*> pidToProcessMap;

	static Platform::Mutex pidAllocatorMutex;
	static IndexAllocator<Uptr, UINTPTR_MAX> pidAllocator;

	Process::Process(
		Compartment* inCompartment,
		const std::vector<std::string>& inArgs,
		const std::vector<std::string>& inEnvs,
		const std::string& inCWD
		)
	: compartment(inCompartment)
	, cwd(inCWD)
	, args(inArgs)
	, envs(inEnvs)
	{
		files.push_back(Platform::getStdFile(Platform::StdDevice::in));
		files.push_back(Platform::getStdFile(Platform::StdDevice::out));
		files.push_back(Platform::getStdFile(Platform::StdDevice::err));
	
		// Allocate a PID for the process.
		{
			Platform::Lock pidAllocatorLock(pidAllocatorMutex);
			id = pidAllocator.alloc();
		}

		// Add the process to the PID->process hash table.
		errorUnless(pidToProcessMap.add(id, this));
	}

	Process::~Process()
	{
		// Remove the process from the PID->process hash table.
		errorUnless(pidToProcessMap.remove(id));
	}

	struct RootResolver : Resolver
	{
		HashMap<std::string,ModuleInstance*> moduleNameToInstanceMap;

		bool resolve(const std::string& moduleName,const std::string& exportName,ObjectType type,Object*& outObject) override
		{
			auto namedInstance = moduleNameToInstanceMap.get(moduleName);
			if(namedInstance)
			{
				outObject = getInstanceExport(*namedInstance, exportName);
				return outObject && isA(outObject,type);
			}
			return false;
		}
	};
	
	struct MainThreadArgs
	{
		Thread* thread;
		GCPointer<FunctionInstance> startFunction;
		GCPointer<FunctionInstance> mainFunction;
	};

	static I64 mainThreadEntry(void* argsVoid)
	{
		auto args = (const MainThreadArgs*)argsVoid;
		currentProcess = args->thread->process;

		// Call the module start function, if it has one.
		if(args->startFunction)
		{
			// Validation should reject the module if the start function isn't ()->()
			wavmAssert(getFunctionType(args->startFunction) == FunctionType::get());
			invokeFunctionUnchecked(args->thread->context,args->startFunction,nullptr);
		}

		// Call the main function.
		return invokeFunctionUnchecked(args->thread->context,args->mainFunction,nullptr)->i64;
	}

	Process* spawnProcess(
		const char* filename,
		const std::vector<std::string>& args,
		const std::vector<std::string>& env,
		const std::string& cwd
		)
	{
		std::string hostFilename = sysroot + filename;
		Module module;

		// Enable some additional "features" in WAVM that are disabled by default.
		module.featureSpec.importExportMutableGlobals = true;
		module.featureSpec.sharedTables = true;
		// Allow atomics on unshared memories to accomodate atomics on the Emscripten memory.
		module.featureSpec.requireSharedFlagForAtomicOperators = false;

		// Load the module.
		if(!loadBinaryModule(hostFilename.c_str(),module)) { return nullptr; }

		// Create the process and compartment.
		Process* process = new Process(
			Runtime::createCompartment(),
			args, env, cwd
			);

		process->args.insert(process->args.begin(),filename);

		// Link the module with the Wavix intrinsics.
		RootResolver rootResolver;
		ModuleInstance* wavixIntrinsicModuleInstance = Intrinsics::instantiateModule(
			process->compartment,
			INTRINSIC_MODULE_REF(wavix));
		rootResolver.moduleNameToInstanceMap.set("env", wavixIntrinsicModuleInstance);

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
			return nullptr;
		}

		// Instantiate the module.
		ModuleInstance* moduleInstance = instantiateModule(process->compartment,module,std::move(linkResult.resolvedImports));
		if(!moduleInstance) { return nullptr; }

		// Look up the module's start, and main functions.
		MainThreadArgs* mainThreadArgs = new MainThreadArgs;
		mainThreadArgs->startFunction = getStartFunction(moduleInstance);
		mainThreadArgs->mainFunction = asFunctionNullable(getInstanceExport(moduleInstance,"_start"));;

		// Validate that the module exported a main function, and that it is the expected type.
		if(!mainThreadArgs->mainFunction)
		{
			std::cerr << "Module does not export _start function" << std::endl;
			return nullptr;
		}

		const FunctionType* functionType = getFunctionType(mainThreadArgs->mainFunction);
		if(functionType != FunctionType::get())
		{
			std::cerr << "Module _start signature is " << asString(functionType) << ", but ()->() was expected." << std::endl;
			return nullptr;
		}

		// Create the context and Wavix Thread object for the main thread.
		Context* mainContext = Runtime::createContext(process->compartment);
		mainThreadArgs->thread = new Thread(process,mainContext);
		process->threads.push_back(mainThreadArgs->thread);

		// Start the process's main thread.
		enum { mainThreadNumStackBytes = 1*1024*1024 };
		Platform::createThread(
			mainThreadNumStackBytes,
			&mainThreadEntry,
			mainThreadArgs);

		return process;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_exit",I32,__syscall_exit,I32 exitCode)
	{
		traceSyscallf("exit","(%i)",exitCode);
		Platform::exitThread(exitCode);
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_exit_group",I32,__syscall_exit_group,I32 exitCode)
	{
		traceSyscallf("exit_group","(%i)",exitCode);
		Platform::exitThread(exitCode);
	}

	FORCENOINLINE static void setCurrentProcess(Process* newProcess)
	{
		currentProcess = newProcess;
	}

	DEFINE_INTRINSIC_FUNCTION_WITH_CONTEXT_SWITCH(wavix,"__syscall_fork",I32,__syscall_fork,I32 dummy)
	{
		Process* originalProcess = currentProcess;
		wavmAssert(originalProcess);
		auto newProcess = new Process(
			cloneCompartment(originalProcess->compartment),
			originalProcess->args,
			originalProcess->envs,
			originalProcess->cwd
			);
		//newProcess->files = process->files;
		//newProcess->freeFileIndices = process->freeFileIndices;

		auto newContext = cloneContext(
			getContextFromRuntimeData(contextRuntimeData),
			newProcess->compartment
			);
		Thread* newThread = new Thread(newProcess,newContext);
		newProcess->threads.push_back(newThread);

		Platform::Thread* platformThread = Platform::forkCurrentThread();
		if(platformThread)
		{
			return Intrinsics::resultInContextRuntimeData<I32>(contextRuntimeData,1);
		}
		else
		{
			// Move the newProcess pointer into the thread-local currentProcess variable. Since some
			// compilers will cache a pointer to thread-local data that's accessed multiple times in
			// one function, and currentProcess is accessed before calling forkCurrentThread, we
			// can't directly write to it in this function in case the compiler tries to write to
			// the original thread's currentProcess variable. Instead, call a FORCENOINLINE function
			// (setCurrentProcess) to set the variable.
			setCurrentProcess(newProcess);

			// Switch the contextRuntimeData to point to the new context's runtime data.
			contextRuntimeData = getContextRuntimeData(newContext);

			return Intrinsics::resultInContextRuntimeData<I32>(contextRuntimeData,0);
		}
	}

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__syscall_execve",I32,__syscall_execve,
		U32 pathAddress, U32 argsAddress, U32 envsAddress)
	{
		MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData,defaultMemoryId.id);

		std::string pathString = readUserString(memory,pathAddress);
		std::vector<std::string> args;
		std::vector<std::string> envs;
		while(true)
		{
			const U32 argStringAddress = memoryRef<U32>(memory,argsAddress);
			if(!argStringAddress) { break; }
			args.push_back(readUserString(memory,argStringAddress));
			argsAddress += sizeof(U32);
		};
		while(true)
		{
			const U32 envStringAddress = memoryRef<U32>(memory,envsAddress);
			if(!envStringAddress) { break; }
			envs.push_back(readUserString(memory,envStringAddress));
			envsAddress += sizeof(U32);
		};

		if(isTracingSyscalls)
		{
			std::string argsString;
			for(const auto& argString : args)
			{
				if(argsString.size()) { argsString += ','; }
				argsString += '\"';
				argsString += argString;
				argsString += '\"';
			}
			std::string envsString;
			for(const auto& envString : args)
			{
				if(envsString.size()) { envsString += ','; }
				envsString += '\"';
				envsString += envString;
				envsString += '\"';
			}
			traceSyscallf("execve","(\"%s\", {%s}, {%s})",pathString.c_str(), argsString.c_str(), envsString.c_str());
		}

		spawnProcess(pathString.c_str(),std::move(args),std::move(envs), currentProcess->cwd);

		Platform::exitThread(0);

		Errors::unreachable();
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_kill",I32,__syscall_kill,I32 a,I32 b)
	{
		traceSyscallf("kill","(%i,%i)",a,b);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getpid",I32,__syscall_getpid,I32 a)
	{
		traceSyscallf("getpid","");
		return 1;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_getppid",I32,__syscall_getppid,I32 dummy)
	{
		traceSyscallf("getppid","");
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_sched_getaffinity",I32,__syscall_sched_getaffinity,I32 a,I32 b,I32 c)
	{
		traceSyscallf("sched_getaffinity","(%i,%i,%i,%i)",a,b,c);
		throwException(Exception::calledUnimplementedIntrinsicType);
	}

	#define WAVIX_WNOHANG    1
	#define WAVIX_WUNTRACED  2

	#define WAVIX_WSTOPPED   2
	#define WAVIX_WEXITED    4
	#define WAVIX_WCONTINUED 8
	#define WAVIX_WNOWAIT    0x1000000

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavix,"__syscall_wait4",I32,__syscall_wait4,
		I32 pid, U32 statusAddress, U32 options, U32 rusageAddress)
	{
		MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData,defaultMemoryId.id);

		traceSyscallf("wait4","(%i,0x%08x,%i,0x%08x)",pid,statusAddress,options,rusageAddress);

		if(rusageAddress != 0)
		{
			throwException(Exception::calledUnimplementedIntrinsicType);
		}

		if(options & WAVIX_WNOHANG)
		{
			memoryRef<I32>(memory,statusAddress) = 0;
			return 0;
		}
		else
		{
			memoryRef<I32>(memory,statusAddress) = WAVIX_WEXITED;
			return pid == -1 ? 1 : pid;
		}
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_gettid",I32,__syscall_gettid,I32)
	{
		traceSyscallf("gettid","()");
		//throwException(Exception::calledUnimplementedIntrinsicType);
		return 1;
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_tkill",I32,__syscall_tkill,U32 threadId,I32 signalNumber)
	{
		traceSyscallf("tkill", "(%i,%i)", threadId, signalNumber);
		Platform::exitThread(-1);
		Errors::unreachable();
	}

	DEFINE_INTRINSIC_FUNCTION(wavix,"__syscall_rt_sigprocmask",I32,__syscall_rt_sigprocmask,
		I32 how, U32 setAddress, U32 oldSetAddress)
	{
		traceSyscallf("rt_sigprocmask", "(%i, 0x%08x, 0x%08x)", how, setAddress, oldSetAddress);
		return 0;
	}

	void staticInitializeProcess()
	{
	}
}
