#ifndef _WIN32

#include "Core/Core.h"
#include "Core/Platform.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>

#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/resource.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifdef __APPLE__
    #define MAP_ANONYMOUS MAP_ANON
#endif

#ifdef __linux__
	#include <execinfo.h>
	#include <dlfcn.h>
#endif

namespace Platform
{
	Mutex::Mutex()
	{
		handle = new pthread_mutex_t();
		if(pthread_mutex_init((pthread_mutex_t*)handle,nullptr)) { Core::fatalError("pthread_mutex_init failed"); }
	}

	Mutex::~Mutex()
	{
		if(pthread_mutex_destroy((pthread_mutex_t*)handle)) { Core::fatalError("pthread_mutex_destroy failed"); }
	}

	void Mutex::Lock()
	{
		if(pthread_mutex_lock((pthread_mutex_t*)handle)) { Core::fatalError("pthread_mutex_lock failed"); }
	}

	void Mutex::Unlock()
	{
		if(pthread_mutex_unlock((pthread_mutex_t*)handle)) { Core::fatalError("pthread_mutex_unlock failed"); }
	}

	static size_t internalGetPreferredVirtualPageSizeLog2()
	{
		uint32 preferredVirtualPageSize = sysconf(_SC_PAGESIZE);
		// Verify our assumption that the virtual page size is a power of two.
		assert(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
		return floorLogTwo(preferredVirtualPageSize);
	}
	uintp getPageSizeLog2()
	{
		static size_t preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
		return preferredVirtualPageSizeLog2;
	}
	
	uint32 memoryAccessAsPOSIXFlag(MemoryAccess access)
	{
		switch(access)
		{
		default:
		case MemoryAccess::None: return PROT_NONE;
		case MemoryAccess::ReadOnly: return PROT_READ;
		case MemoryAccess::ReadWrite: return PROT_READ | PROT_WRITE;
		case MemoryAccess::Execute: return PROT_EXEC;
		case MemoryAccess::ReadWriteExecute: return PROT_EXEC | PROT_READ | PROT_WRITE;
		}
	}

	bool isPageAligned(uint8* address)
	{
		const uintp addressBits = reinterpret_cast<uintp>(address);
		return (addressBits & ((1ull << getPageSizeLog2()) - 1)) == 0;
	}

	uint8* allocateVirtualPages(size_t numPages)
	{
		size_t numBytes = numPages << getPageSizeLog2();
		auto result = mmap(nullptr,numBytes,PROT_NONE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		if(result == MAP_FAILED)
		{
			Log::printf(Log::Category::error,"mmap(%" PRIuPTR " KB) failed: errno=%s\n",numBytes/1024,strerror(errno));
			return nullptr;
		}
		return (uint8*)result;
	}

	bool commitVirtualPages(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		assert(isPageAligned(baseVirtualAddress));
		return mprotect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsPOSIXFlag(access)) == 0;
	}
	
	bool setVirtualPageAccess(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		assert(isPageAligned(baseVirtualAddress));
		return mprotect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsPOSIXFlag(access)) == 0;
	}

	void decommitVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto numBytes = numPages << getPageSizeLog2();
		if(madvise(baseVirtualAddress,numBytes,MADV_DONTNEED)) { Core::fatalError("madvise failed"); }
		if(mprotect(baseVirtualAddress,numBytes,PROT_NONE)) { Core::fatalError("mprotect failed"); }
	}

	void freeVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		if(munmap(baseVirtualAddress,numPages << getPageSizeLog2())) { Core::fatalError("munmap failed"); }
	}

	bool describeInstructionPointer(uintp ip,std::string& outDescription)
	{
		#ifdef __linux__
			// Look up static symbol information for the address.
			Dl_info symbolInfo;
			if(dladdr((void*)ip,&symbolInfo) && symbolInfo.dli_sname)
			{
				outDescription = symbolInfo.dli_sname;
				return true;
			}
		#endif
		return false;
	}

	THREAD_LOCAL jmp_buf signalReturnEnv;
	THREAD_LOCAL HardwareTrapType signalType = HardwareTrapType::none;
	THREAD_LOCAL ExecutionContext* signalContext = nullptr;
	THREAD_LOCAL uintp* signalOperand = nullptr;
	THREAD_LOCAL bool isReentrantSignal = false;

	enum { signalStackNumBytes = SIGSTKSZ };
	THREAD_LOCAL uint8* signalStack = nullptr;
	THREAD_LOCAL uint8* stackMinAddr = nullptr;
	THREAD_LOCAL uint8* stackMaxAddr = nullptr;

	void initThread()
	{
		if(!signalStack)
		{
			signalStack = new uint8[signalStackNumBytes];
			stack_t signalStackInfo;
			signalStackInfo.ss_size = signalStackNumBytes;
			signalStackInfo.ss_sp = signalStack;
			signalStackInfo.ss_flags = 0;
			if(sigaltstack(&signalStackInfo,nullptr) < 0)
			{
				Core::fatalError("sigaltstack failed");
			}
		}

		struct rlimit stackLimit;
		getrlimit(RLIMIT_STACK,&stackLimit);
		const size_t stackSize = stackLimit.rlim_cur;

		stackMinAddr = (uint8*)&stackLimit - stackSize;
		stackMaxAddr = (uint8*)&stackSize;
	}

	void signalHandler(int signalNumber,siginfo_t* signalInfo,void*)
	{
		if(isReentrantSignal) { Core::fatalError("reentrant signal handler"); }
		isReentrantSignal = true;

		// Derive the exception cause the from signal that was received.
		switch(signalNumber)
		{
		case SIGFPE:
			if(signalInfo->si_code != FPE_INTDIV && signalInfo->si_code != FPE_INTOVF) { Core::fatalError("unknown SIGFPE code"); }
			signalType = HardwareTrapType::intDivideByZeroOrOverflow;
			break;
		case SIGSEGV:
		case SIGBUS:
			signalType = signalInfo->si_addr > stackMinAddr && signalInfo->si_addr < stackMaxAddr
				? HardwareTrapType::stackOverflow
				: HardwareTrapType::accessViolation;
			*signalOperand = reinterpret_cast<uintp>(signalInfo->si_addr);
			break;
		default:
			Core::fatalError("unknown signal number");
			break;
		};

		// Capture the execution context, omitting this function and the function that called it,
		// so the top of the callstack is the function that triggered the signal.
		*signalContext = captureExecutionContext(2);

		// Jump back to the setjmp in catchRuntimeExceptions.
		siglongjmp(signalReturnEnv,1);
	}

	HardwareTrapType catchHardwareTraps(
		ExecutionContext& outContext,
		uintp& outOperand,
		const std::function<void()>& thunk
		)
	{
		assert(signalStack);
		
		struct sigaction oldSignalActionSEGV;
		struct sigaction oldSignalActionBUS;
		struct sigaction oldSignalActionFPE;

		// Use setjmp to allow signals to jump back to this point.
		bool isReturningFromSignalHandler = sigsetjmp(signalReturnEnv,1);
		if(!isReturningFromSignalHandler)
		{
			signalType = HardwareTrapType::none;
			signalContext = &outContext;
			signalOperand = &outOperand;

			// Set a signal handler for the signals we want to intercept.
			struct sigaction signalAction;
			signalAction.sa_sigaction = signalHandler;
			sigemptyset(&signalAction.sa_mask);
			signalAction.sa_flags = SA_SIGINFO | SA_ONSTACK;
			sigaction(SIGSEGV,&signalAction,&oldSignalActionSEGV);
			sigaction(SIGBUS,&signalAction,&oldSignalActionBUS);
			sigaction(SIGFPE,&signalAction,&oldSignalActionFPE);

			// Call the thunk.
			thunk();
		}

		// Reset the signal state.
		isReentrantSignal = false;
		signalContext = nullptr;
		signalOperand = nullptr;
		sigaction(SIGSEGV,&oldSignalActionSEGV,nullptr);
		sigaction(SIGBUS,&oldSignalActionBUS,nullptr);
		sigaction(SIGFPE,&oldSignalActionFPE,nullptr);

		return signalType;
	}

	ExecutionContext captureExecutionContext(uintp numOmittedFramesFromTop)
	{
		#ifdef __linux__
			// Unwind the callstack.
			enum { maxCallStackSize = 512 };
			void* callstackAddresses[maxCallStackSize];
			auto numCallStackEntries = backtrace(callstackAddresses,maxCallStackSize);

			// Copy the return pointers into the stack frames of the resulting ExecutionContext.
			// Skip the first numOmittedFramesFromTop+1 frames, which correspond to this function
			// and others that the caller would like to omit.
			ExecutionContext result;
			for(intp index = numOmittedFramesFromTop + 1;index < numCallStackEntries;++index)
			{
				result.stackFrames.push_back({(uintp)callstackAddresses[index],0});
			}
			return result;
		#else
			return ExecutionContext();
		#endif
	}
}

#endif
