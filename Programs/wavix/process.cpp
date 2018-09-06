#include <algorithm>
#include <memory>
#include <new>
#include <utility>

#include "IR/Module.h"
#include "IR/Types.h"
#include "IR/Validate.h"
#include "IR/Value.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/ConcurrentHashMap.h"
#include "Inline/Errors.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/IndexMap.h"
#include "Inline/Lock.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "Platform/Platform.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Linker.h"
#include "Runtime/RuntimeData.h"
#include "WASM/WASM.h"
#include "errno.h"
#include "process.h"
#include "wavix.h"

using namespace IR;
using namespace Runtime;
using namespace Wavix;

namespace Wavix
{
	thread_local Thread* currentThread = nullptr;
	thread_local Process* currentProcess = nullptr;

	void staticInitializeProcess() {}
}

static ConcurrentHashMap<I32, Process*> pidToProcessMap;

static Platform::Mutex processesMutex;
static IndexMap<I32, Process*> processes(1, INT32_MAX);

struct RootResolver : Resolver
{
	HashMap<std::string, ModuleInstance*> moduleNameToInstanceMap;

	bool resolve(const std::string& moduleName,
				 const std::string& exportName,
				 ObjectType type,
				 Object*& outObject) override
	{
		auto namedInstance = moduleNameToInstanceMap.get(moduleName);
		if(namedInstance)
		{
			outObject = getInstanceExport(*namedInstance, exportName);
			return outObject && isA(outObject, type);
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

FORCENOINLINE static void signalProcessWaiters(Process* process)
{
	Lock<Platform::Mutex> waitersLock(process->waitersMutex);
	for(Thread* waitingThread : process->waiters) { waitingThread->wakeEvent.signal(); }
}

static I64 mainThreadEntry(void* argsVoid)
{
	auto args = (const MainThreadArgs*)argsVoid;
	currentThread = args->thread;
	currentProcess = args->thread->process;

	// Call the module start function, if it has one.
	if(args->startFunction)
	{
		// Validation should reject the module if the start function isn't ()->()
		wavmAssert(getFunctionType(args->startFunction) == FunctionType());
		invokeFunctionUnchecked(args->thread->context, args->startFunction, nullptr);
	}

	// Call the main function.
	I64 result = invokeFunctionUnchecked(args->thread->context, args->mainFunction, nullptr)->i64;

	// Wake any threads waiting for this process to exit.
	signalProcessWaiters(currentProcess);

	return result;
}

inline bool loadBinaryModuleFromFile(const char* wasmFilename, IR::Module& outModule)
{
	try
	{
		Platform::File* file = Platform::openFile(wasmFilename,
												  Platform::FileAccessMode::readOnly,
												  Platform::FileCreateMode::openExisting);
		if(!file) { return false; }

		U64 numFileBytes = 0;
		errorUnless(Platform::seekFile(file, 0, Platform::FileSeekOrigin::end, &numFileBytes));
		if(numFileBytes > UINTPTR_MAX)
		{
			errorUnless(Platform::closeFile(file));
			return false;
		}

		std::unique_ptr<U8[]> fileContents{new U8[numFileBytes]};
		errorUnless(Platform::seekFile(file, 0, Platform::FileSeekOrigin::begin));
		errorUnless(Platform::readFile(file, fileContents.get(), numFileBytes));
		errorUnless(Platform::closeFile(file));

		Serialization::MemoryInputStream stream(fileContents.get(), numFileBytes);
		WASM::serialize(stream, outModule);

		return true;
	}
	catch(Serialization::FatalSerializationException exception)
	{
		Log::printf(Log::debug,
					"Error deserializing WebAssembly binary file:\n%s\n",
					exception.message.c_str());
		return false;
	}
	catch(IR::ValidationException exception)
	{
		Log::printf(Log::debug,
					"Error validating WebAssembly binary file:\n%s\n",
					exception.message.c_str());
		return false;
	}
	catch(std::bad_alloc)
	{
		Log::printf(
			Log::debug,
			"Failed to allocate memory during WASM module load: input is likely malformed.\n");
		return false;
	}
}

ModuleInstance* loadModule(Process* process, const char* hostFilename)
{
	// Load the module.
	IR::Module module;
	if(!loadBinaryModuleFromFile(hostFilename, module)) { return nullptr; }

	// Link the module with the Wavix intrinsics.
	RootResolver rootResolver;
	ModuleInstance* wavixIntrinsicModuleInstance = Intrinsics::instantiateModule(
		process->compartment, INTRINSIC_MODULE_REF(wavix), "WavixIntrinsics");
	rootResolver.moduleNameToInstanceMap.set("env", wavixIntrinsicModuleInstance);

	LinkResult linkResult = linkModule(module, rootResolver);
	if(!linkResult.success)
	{
		Log::printf(Log::debug, "Failed to link module:\n");
		for(auto& missingImport : linkResult.missingImports)
		{
			Log::printf(Log::debug,
						"Missing import: module=\"%s\" export=\"%s\" type=\"%s\"\n",
						missingImport.moduleName.c_str(),
						missingImport.exportName.c_str(),
						asString(missingImport.type).c_str());
		}
		return nullptr;
	}

	// Instantiate the module.
	return instantiateModule(process->compartment,
							 compileModule(module),
							 std::move(linkResult.resolvedImports),
							 hostFilename);
}

Thread* executeModule(Process* process, ModuleInstance* moduleInstance)
{
	// Look up the module's start, and main functions.
	MainThreadArgs* mainThreadArgs = new MainThreadArgs;
	mainThreadArgs->startFunction = getStartFunction(moduleInstance);
	mainThreadArgs->mainFunction = asFunctionNullable(getInstanceExport(moduleInstance, "_start"));
	;

	// Validate that the module exported a main function, and that it is the expected type.
	if(!mainThreadArgs->mainFunction)
	{
		Log::printf(Log::debug, "Module does not export _start function");
		return nullptr;
	}

	FunctionType mainFunctionType = getFunctionType(mainThreadArgs->mainFunction);
	if(mainFunctionType != FunctionType())
	{
		Log::printf(Log::debug,
					"Module _start signature is %s, but ()->() was expected.\n",
					asString(mainFunctionType).c_str());
		return nullptr;
	}

	// Create the context and Wavix Thread object for the main thread.
	Context* mainContext = Runtime::createContext(process->compartment);
	Thread* mainThread = new Thread(process, mainContext);
	mainThreadArgs->thread = mainThread;

	// Start the process's main thread.
	enum
	{
		mainThreadNumStackBytes = 1 * 1024 * 1024
	};
	Platform::createThread(mainThreadNumStackBytes, &mainThreadEntry, mainThreadArgs);

	return mainThread;
}

Process* Wavix::spawnProcess(Process* parent,
							 const char* hostFilename,
							 const std::vector<std::string>& args,
							 const std::vector<std::string>& envs,
							 const std::string& cwd)
{
	// Create the process and compartment.
	Process* process = new Process;
	process->compartment = Runtime::createCompartment();
	process->envs = envs;
	process->args = args;
	process->args.insert(process->args.begin(), hostFilename);

	process->cwd = cwd;

	process->parent = parent;
	if(parent)
	{
		Lock<Platform::Mutex> childrenLock(parent->childrenMutex);
		parent->children.push_back(process);
	}

	// Initialize the process's standard IO file descriptors.
	process->files.insertOrFail(0, Platform::getStdFile(Platform::StdDevice::in));
	process->files.insertOrFail(1, Platform::getStdFile(Platform::StdDevice::out));
	process->files.insertOrFail(2, Platform::getStdFile(Platform::StdDevice::err));

	// Allocate a PID for the process.
	{
		Lock<Platform::Mutex> processesLock(processesMutex);
		process->id = processes.add(-1, process);
		if(process->id == -1) { return nullptr; }
	}

	// Add the process to the PID->process hash table.
	pidToProcessMap.addOrFail(process->id, process);

	// Load the module.
	ModuleInstance* moduleInstance = loadModule(process, hostFilename);
	if(!moduleInstance) { return nullptr; }

	// Get the module's memory and table.
	process->memory = asMemoryNullable(getInstanceExport(moduleInstance, "__memory"));
	process->table = asTableNullable(getInstanceExport(moduleInstance, "__table"));

	if(!process->memory || !process->table) { return nullptr; }

	Thread* mainThread = executeModule(process, moduleInstance);
	{
		Lock<Platform::Mutex> threadsLock(process->threadsMutex);
		process->threads.push_back(mainThread);
	}

	return process;
}

DEFINE_INTRINSIC_FUNCTION(wavix, "__syscall_exit", I32, __syscall_exit, I32 exitCode)
{
	traceSyscallf("exit", "(%i)", exitCode);

	// Wake any threads waiting for this process to exit.
	signalProcessWaiters(currentProcess);

	Platform::exitThread(exitCode);
}

DEFINE_INTRINSIC_FUNCTION(wavix, "__syscall_exit_group", I32, __syscall_exit_group, I32 exitCode)
{
	traceSyscallf("exit_group", "(%i)", exitCode);

	// Wake any threads waiting for this process to exit.
	signalProcessWaiters(currentProcess);

	Platform::exitThread(exitCode);
}

FORCENOINLINE static void setCurrentThreadAndProcess(Thread* newThread)
{
	currentThread = newThread;
	currentProcess = newThread->process;
}

DEFINE_INTRINSIC_FUNCTION_WITH_CONTEXT_SWITCH(wavix,
											  "__syscall_fork",
											  I32,
											  __syscall_fork,
											  I32 dummy)
{
	Process* originalProcess = currentProcess;
	wavmAssert(originalProcess);

	traceSyscallf("fork", "");

	// Create a new process with a clone of the original's runtime compartment.
	auto newProcess = new Process;
	newProcess->compartment = cloneCompartment(originalProcess->compartment);
	newProcess->args = originalProcess->args;
	newProcess->envs = originalProcess->envs;

	// Look up the new process's memory and table objects by finding the objects with the same IDs
	// as the original process's memory and table objects in the cloned compartment.
	wavmAssert(originalProcess->memory);
	wavmAssert(originalProcess->table);
	newProcess->memory = getCompartmentMemoryById(newProcess->compartment,
												  getCompartmentMemoryId(originalProcess->memory));
	newProcess->table = getCompartmentTableById(newProcess->compartment,
												getCompartmentTableId(originalProcess->table));
	wavmAssert(newProcess->memory);
	wavmAssert(newProcess->table);

	newProcess->parent = originalProcess;
	{
		Lock<Platform::Mutex> childrenLock(originalProcess->childrenMutex);
		originalProcess->children.push_back(newProcess);
	}

	// Copy the original process's working directory and open files to the new process.
	{
		Lock<Platform::Mutex> cwdLock(originalProcess->cwdMutex);
		newProcess->cwd = originalProcess->cwd;
	}
	{
		Lock<Platform::Mutex> filesLock(originalProcess->filesMutex);
		newProcess->files = originalProcess->files;
	}

	// Allocate a PID for the new process.
	{
		Lock<Platform::Mutex> processesLock(processesMutex);
		newProcess->id = processes.add(-1, newProcess);
		if(newProcess->id == -1) { return nullptr; }
	}

	// Add the process to the PID->process hash table.
	pidToProcessMap.addOrFail(newProcess->id, newProcess);

	// Create a new Wavix Thread with a clone of the original's runtime context.
	auto newContext
		= cloneContext(getContextFromRuntimeData(contextRuntimeData), newProcess->compartment);
	Thread* newThread = new Thread(newProcess, newContext);
	newProcess->threads.push_back(newThread);

	// Fork the current platform thread.
	Platform::Thread* platformThread = Platform::forkCurrentThread();
	if(platformThread)
	{ return Intrinsics::resultInContextRuntimeData<I32>(contextRuntimeData, 1); }
	else
	{
		// Move the newProcess pointer into the thread-local currentProcess variable. Since some
		// compilers will cache a pointer to thread-local data that's accessed multiple times in one
		// function, and currentProcess is accessed before calling forkCurrentThread, we can't
		// directly write to it in this function in case the compiler tries to write to the original
		// thread's currentProcess variable. Instead, call a FORCENOINLINE function
		// (setCurrentProcess) to set the variable.
		setCurrentThreadAndProcess(newThread);

		// Switch the contextRuntimeData to point to the new context's runtime data.
		contextRuntimeData = getContextRuntimeData(newContext);

		return Intrinsics::resultInContextRuntimeData<I32>(contextRuntimeData, 0);
	}
}

DEFINE_INTRINSIC_FUNCTION(wavix,
						  "__syscall_execve",
						  I32,
						  __syscall_execve,
						  U32 pathAddress,
						  U32 argsAddress,
						  U32 envsAddress)
{
	MemoryInstance* memory = currentThread->process->memory;

	std::string pathString = readUserString(memory, pathAddress);
	std::vector<std::string> args;
	std::vector<std::string> envs;
	while(true)
	{
		const U32 argStringAddress = memoryRef<U32>(memory, argsAddress);
		if(!argStringAddress) { break; }
		args.push_back(readUserString(memory, argStringAddress));
		argsAddress += sizeof(U32);
	};
	while(true)
	{
		const U32 envStringAddress = memoryRef<U32>(memory, envsAddress);
		if(!envStringAddress) { break; }
		envs.push_back(readUserString(memory, envStringAddress));
		envsAddress += sizeof(U32);
	};

	if(isTracingSyscalls)
	{
		std::string argsString;
		for(const auto& argString : args)
		{
			if(argsString.size()) { argsString += ", "; }
			argsString += '\"';
			argsString += argString;
			argsString += '\"';
		}
		std::string envsString;
		for(const auto& envString : args)
		{
			if(envsString.size()) { envsString += ", "; }
			envsString += '\"';
			envsString += envString;
			envsString += '\"';
		}
		traceSyscallf("execve",
					  "(\"%s\", {%s}, {%s})",
					  pathString.c_str(),
					  argsString.c_str(),
					  envsString.c_str());
	}

	// Update the process args/envs.
	{
		Lock<Platform::Mutex> argsEnvLock(currentProcess->argsEnvMutex);
		currentProcess->args = args;
		currentProcess->envs = envs;
	}

	// Load the module.
	ModuleInstance* moduleInstance
		= loadModule(currentProcess, (sysroot + "/" + pathString).c_str());
	if(!moduleInstance) { return Wavix::ErrNo::enoent; }

	// Execute the module in a new thread.
	Thread* mainThread = executeModule(currentProcess, moduleInstance);
	{
		Lock<Platform::Mutex> threadsLock(currentProcess->threadsMutex);
		currentProcess->threads.clear();
		currentProcess->threads.push_back(mainThread);
	}

	// Exit the calling thread.
	Platform::exitThread(0);

	Errors::unreachable();
}

DEFINE_INTRINSIC_FUNCTION(wavix, "__syscall_kill", I32, __syscall_kill, I32 a, I32 b)
{
	traceSyscallf("kill", "(%i,%i)", a, b);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(wavix, "__syscall_getpid", I32, __syscall_getpid, I32 dummy)
{
	traceSyscallf("getpid", "");
	return coerce32bitAddress(currentProcess->id);
}

DEFINE_INTRINSIC_FUNCTION(wavix, "__syscall_getppid", I32, __syscall_getppid, I32 dummy)
{
	traceSyscallf("getppid", "");
	if(currentProcess->parent) { return currentProcess->parent->id; }
	else
	{
		return 0;
	}
}

DEFINE_INTRINSIC_FUNCTION(wavix,
						  "__syscall_sched_getaffinity",
						  I32,
						  __syscall_sched_getaffinity,
						  I32 a,
						  I32 b,
						  I32 c)
{
	traceSyscallf("sched_getaffinity", "(%i,%i,%i)", a, b, c);
	throwException(Exception::calledUnimplementedIntrinsicType);
}

#define WAVIX_WNOHANG 1
#define WAVIX_WUNTRACED 2

#define WAVIX_WSTOPPED 2
#define WAVIX_WEXITED 4
#define WAVIX_WCONTINUED 8
#define WAVIX_WNOWAIT 0x1000000

DEFINE_INTRINSIC_FUNCTION(wavix,
						  "__syscall_wait4",
						  I32,
						  __syscall_wait4,
						  I32 pid,
						  U32 statusAddress,
						  U32 options,
						  U32 rusageAddress)
{
	MemoryInstance* memory = currentThread->process->memory;

	traceSyscallf("wait4", "(%i,0x%08x,%i,0x%08x)", pid, statusAddress, options, rusageAddress);

	if(rusageAddress != 0) { throwException(Exception::calledUnimplementedIntrinsicType); }

	if(pid < -1)
	{
		// Wait for any child process whose group id == |pid| to change state.
	}
	else if(pid == -1)
	{
		// Wait for any child process to change state.
	}
	else if(pid == 0)
	{
		// Wait for any child process whose group id == this process's group id to change state.
	}
	else if(pid > 0)
	{
		// Wait for the child process whose process id == pid.
	}

	if(options & WAVIX_WNOHANG)
	{
		memoryRef<I32>(memory, statusAddress) = 0;
		return 0;
	}
	else
	{
		std::vector<Process*> waiteeProcesses;
		{
			Lock<Platform::Mutex> childrenLock(currentProcess->childrenMutex);
			waiteeProcesses = currentProcess->children;
		}

		for(Process* child : waiteeProcesses)
		{
			Lock<Platform::Mutex> waiterLock(child->waitersMutex);
			child->waiters.push_back(currentThread);
		}

		while(!currentThread->wakeEvent.wait(UINT64_MAX)) {};

		for(Process* child : waiteeProcesses)
		{
			Lock<Platform::Mutex> waiterLock(child->waitersMutex);
			auto waiterIt = std::find(child->waiters.begin(), child->waiters.end(), currentThread);
			if(waiterIt != child->waiters.end()) { child->waiters.erase(waiterIt); }
		}

		memoryRef<I32>(memory, statusAddress) = WAVIX_WEXITED;
		return pid == -1 ? 1 : pid;
	}
}

DEFINE_INTRINSIC_FUNCTION(wavix, "__syscall_gettid", I32, __syscall_gettid, I32)
{
	traceSyscallf("gettid", "()");
	// throwException(Exception::calledUnimplementedIntrinsicType);
	return 1;
}

DEFINE_INTRINSIC_FUNCTION(wavix,
						  "__syscall_tkill",
						  I32,
						  __syscall_tkill,
						  U32 threadId,
						  I32 signalNumber)
{
	traceSyscallf("tkill", "(%i,%i)", threadId, signalNumber);

	// Wake any threads waiting for this process to exit.
	signalProcessWaiters(currentProcess);

	Platform::exitThread(-1);
}

DEFINE_INTRINSIC_FUNCTION(wavix,
						  "__syscall_rt_sigprocmask",
						  I32,
						  __syscall_rt_sigprocmask,
						  I32 how,
						  U32 setAddress,
						  U32 oldSetAddress)
{
	traceSyscallf("rt_sigprocmask", "(%i, 0x%08x, 0x%08x)", how, setAddress, oldSetAddress);
	return 0;
}
