#if _WIN32

#include "Inline/BasicTypes.h"
#include "Inline/Errors.h"
#include "Platform.h"

#include <inttypes.h>
#include <algorithm>
#include <Windows.h>
#include <DbgHelp.h>

#undef min

namespace Platform
{
	static size_t internalGetPreferredVirtualPageSizeLog2()
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		size_t preferredVirtualPageSize = systemInfo.dwPageSize;
		// Verify our assumption that the virtual page size is a power of two.
		errorUnless(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
		return floorLogTwo(preferredVirtualPageSize);
	}
	uintp getPageSizeLog2()
	{
		static size_t preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
		return preferredVirtualPageSizeLog2;
	}

	uint32 memoryAccessAsWin32Flag(MemoryAccess access)
	{
		switch(access)
		{
		default:
		case MemoryAccess::None: return PAGE_NOACCESS;
		case MemoryAccess::ReadOnly: return PAGE_READONLY;
		case MemoryAccess::ReadWrite: return PAGE_READWRITE;
		case MemoryAccess::Execute: return PAGE_EXECUTE_READ;
		case MemoryAccess::ReadWriteExecute: return PAGE_EXECUTE_READWRITE;
		}
	}

	static bool isPageAligned(uint8* address)
	{
		const uintp addressBits = reinterpret_cast<uintp>(address);
		return (addressBits & ((1ull << getPageSizeLog2()) - 1)) == 0;
	}

	uint8* allocateVirtualPages(size_t numPages)
	{
		size_t numBytes = numPages << getPageSizeLog2();
		auto result = VirtualAlloc(nullptr,numBytes,MEM_RESERVE,PAGE_NOACCESS);
		if(result == NULL)
		{
			return nullptr;
		}
		return (uint8*)result;
	}

	bool commitVirtualPages(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		return baseVirtualAddress == VirtualAlloc(baseVirtualAddress,numPages << getPageSizeLog2(),MEM_COMMIT,memoryAccessAsWin32Flag(access));
	}

	bool setVirtualPageAccess(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		DWORD oldProtection = 0;
		return VirtualProtect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsWin32Flag(access),&oldProtection) != 0;
	}
	
	void decommitVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,numPages << getPageSizeLog2(),MEM_DECOMMIT);
		if(baseVirtualAddress && !result) { Errors::fatal("VirtualFree(MEM_DECOMMIT) failed"); }
	}

	void freeVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,0/*numPages << getPageSizeLog2()*/,MEM_RELEASE);
		if(baseVirtualAddress && !result) { Errors::fatal("VirtualFree(MEM_RELEASE) failed"); }
	}

	// The interface to the DbgHelp DLL
	struct DbgHelp
	{
		typedef BOOL (WINAPI* SymFromAddr)(HANDLE,uint64,uint64*,SYMBOL_INFO*);
		SymFromAddr symFromAddr;
		DbgHelp()
		{
			HMODULE dbgHelpModule = ::LoadLibraryA("Dbghelp.dll");
			if(dbgHelpModule)
			{
				symFromAddr = (SymFromAddr)::GetProcAddress(dbgHelpModule,"SymFromAddr");

				// Initialize the debug symbol lookup.
				typedef BOOL (WINAPI* SymInitialize)(HANDLE,PCTSTR,BOOL);
				SymInitialize symInitialize = (SymInitialize)::GetProcAddress(dbgHelpModule,"SymInitialize");
				if(symInitialize)
				{
					symInitialize(GetCurrentProcess(),nullptr,TRUE);
				}
			}
		}
	};
	DbgHelp* dbgHelp = nullptr;

	bool describeInstructionPointer(uintp ip,std::string& outDescription)
	{
		// Initialize DbgHelp.
		if(!dbgHelp) { dbgHelp = new DbgHelp(); }

		// Allocate up a SYMBOL_INFO struct to receive information about the symbol for this instruction pointer.
		const size_t maxSymbolNameChars = 256;
		const size_t symbolAllocationSize = sizeof(SYMBOL_INFO) + sizeof(TCHAR) * (maxSymbolNameChars - 1);
		SYMBOL_INFO* symbolInfo = (SYMBOL_INFO*)alloca(symbolAllocationSize);
		ZeroMemory(symbolInfo,symbolAllocationSize);
		symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbolInfo->MaxNameLen = maxSymbolNameChars;

		// Call DbgHelp::SymFromAddr to try to find any debug symbol containing this address.
		if(!dbgHelp->symFromAddr(GetCurrentProcess(),ip,nullptr,symbolInfo)) { return false; }
		else
		{
			outDescription = symbolInfo->Name;
			return true;
		}
	}
	
	void registerSEHUnwindInfo(uintp imageLoadAddress,uintp pdataAddress,size_t pdataNumBytes)
	{
		const uint32 numFunctions = (uint32)(pdataNumBytes / sizeof(RUNTIME_FUNCTION));

		// Register our manually fixed up copy of the function table.
		if(!RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION*>(pdataAddress),numFunctions,imageLoadAddress))
		{
			Errors::fatal("RtlAddFunctionTable failed");
		}
	}
	void deregisterSEHUnwindInfo(uintp pdataAddress)
	{
		auto functionTable = reinterpret_cast<RUNTIME_FUNCTION*>(pdataAddress);
		RtlDeleteFunctionTable(functionTable);
		delete [] functionTable;
	}
	
	CallStack unwindStack(const CONTEXT& immutableContext)
	{
		// Make a mutable copy of the context.
		CONTEXT context;
		memcpy(&context,&immutableContext,sizeof(CONTEXT));

		// Unwind the stack until there isn't a valid instruction pointer, which signals we've reached the base.
		CallStack callStack;
		while(context.Rip)
		{
			callStack.stackFrames.push_back({context.Rip});

			// Look up the SEH unwind information for this function.
			uint64 imageBase;
			auto runtimeFunction = RtlLookupFunctionEntry(context.Rip,&imageBase,nullptr);
			if(!runtimeFunction)
			{
				// Leaf functions that don't touch Rsp may not have unwind information.
				context.Rip  = *(uint64*)context.Rsp;
				context.Rsp += 8;
			}
			else
			{
				// Use the SEH information to unwind to the next stack frame.
				void* handlerData;
				uint64 establisherFrame;
				RtlVirtualUnwind(
					UNW_FLAG_NHANDLER,
					imageBase,
					context.Rip,
					runtimeFunction,
					&context,
					&handlerData,
					&establisherFrame,
					nullptr
					);
			}
		}

		return callStack;
	}

	CallStack captureCallStack(uintp numOmittedFramesFromTop)
	{
		// Capture the current processor state.
		CONTEXT context;
		RtlCaptureContext(&context);

		// Unwind the stack.
		auto result = unwindStack(context);

		// Remote the requested number of omitted frames, +1 for this function.
		const uintp numOmittedFrames = std::min(result.stackFrames.size(),numOmittedFramesFromTop + 1);
		result.stackFrames.erase(result.stackFrames.begin(),result.stackFrames.begin() + numOmittedFrames);

		return result;
	}

	THREAD_LOCAL bool isReentrantException = false;
	LONG CALLBACK sehFilterFunction(EXCEPTION_POINTERS* exceptionPointers,HardwareTrapType& outType,uintp& outTrapOperand,CallStack& outCallStack)
	{
		if(isReentrantException) { Errors::fatal("reentrant exception"); }
		else
		{
			// Decide how to handle this exception code.
			switch(exceptionPointers->ExceptionRecord->ExceptionCode)
			{
			case EXCEPTION_ACCESS_VIOLATION:
				outType = HardwareTrapType::accessViolation;
				outTrapOperand = exceptionPointers->ExceptionRecord->ExceptionInformation[1];
				break;
			case EXCEPTION_STACK_OVERFLOW: outType = HardwareTrapType::stackOverflow; break;
			case STATUS_INTEGER_DIVIDE_BY_ZERO: outType = HardwareTrapType::intDivideByZeroOrOverflow; break;
			case STATUS_INTEGER_OVERFLOW: outType = HardwareTrapType::intDivideByZeroOrOverflow; break;
			default: return EXCEPTION_CONTINUE_SEARCH;
			}
			isReentrantException = true;

			// Unwind the stack frames from the context of the exception.
			outCallStack = unwindStack(*exceptionPointers->ContextRecord);

			return EXCEPTION_EXECUTE_HANDLER;
		}
	}
	
	THREAD_LOCAL bool isThreadInitialized = false;
	void initThread()
	{
		if(!isThreadInitialized)
		{
			isThreadInitialized = true;

			// Ensure that there's enough space left on the stack in the case of a stack overflow to prepare the stack trace.
			ULONG stackOverflowReserveBytes = 32768;
			SetThreadStackGuarantee(&stackOverflowReserveBytes);
		}
	}

	HardwareTrapType catchHardwareTraps(
		CallStack& outTrapCallStack,
		uintp& outTrapOperand,
		const std::function<void()>& thunk
		)
	{
		initThread();

		HardwareTrapType result = HardwareTrapType::none;
		__try
		{
			thunk();
		}
		__except(sehFilterFunction(GetExceptionInformation(),result,outTrapOperand,outTrapCallStack))
		{
			isReentrantException = false;
			
			// After a stack overflow, the stack will be left in a damaged state. Let the CRT repair it.
			if(result == HardwareTrapType::stackOverflow) { _resetstkoflw(); }
		}
		return result;
	}
	
	uint64 getMonotonicClock()
	{
		LARGE_INTEGER performanceCounter;
		LARGE_INTEGER performanceCounterFrequency;
		QueryPerformanceCounter(&performanceCounter);
		QueryPerformanceFrequency(&performanceCounterFrequency);

		const uint64 wavmFrequency = 1000000;

		return performanceCounterFrequency.QuadPart > wavmFrequency
			? performanceCounter.QuadPart / (performanceCounterFrequency.QuadPart / wavmFrequency)
			: performanceCounter.QuadPart * (wavmFrequency / performanceCounterFrequency.QuadPart);
	}

	struct Mutex
	{
		CRITICAL_SECTION criticalSection;
	};

	Mutex* createMutex()
	{
		auto mutex = new Mutex();
		InitializeCriticalSection(&mutex->criticalSection);
		return mutex;
	}

	void destroyMutex(Mutex* mutex)
	{
		DeleteCriticalSection(&mutex->criticalSection);
		delete mutex;
	}

	void lockMutex(Mutex* mutex)
	{
		EnterCriticalSection(&mutex->criticalSection);
	}

	void unlockMutex(Mutex* mutex)
	{
		LeaveCriticalSection(&mutex->criticalSection);
	}

	Event* createEvent()
	{
		return reinterpret_cast<Event*>(CreateEvent(nullptr,FALSE,FALSE,nullptr));
	}

	void destroyEvent(Event* event)
	{
		CloseHandle(reinterpret_cast<HANDLE>(event));
	}

	bool waitForEvent(Event* event,uint64 untilTime)
	{
		uint64 currentTime = getMonotonicClock();
		const uint64 startProcessTime = currentTime;
		while(true)
		{
			const uint64 timeoutMicroseconds = currentTime > untilTime ? 0 : (untilTime - currentTime);
			const uint64 timeoutMilliseconds64 = timeoutMicroseconds / 1000;
			const uint32 timeoutMilliseconds32 =
				timeoutMilliseconds64 > UINT32_MAX
				? (UINT32_MAX - 1)
				: uint32(timeoutMilliseconds64);
		
			const uint32 waitResult = WaitForSingleObject(reinterpret_cast<HANDLE>(event),timeoutMilliseconds32);
			if(waitResult != WAIT_TIMEOUT)
			{
				errorUnless(waitResult == WAIT_OBJECT_0);
				return true;
			}
			else
			{
				currentTime = getMonotonicClock();
				if(currentTime >= untilTime)
				{
					return false;
				}
			}
		};
	}

	void signalEvent(Event* event)
	{
		errorUnless(SetEvent(reinterpret_cast<HANDLE>(event)));
	}
}

#endif
