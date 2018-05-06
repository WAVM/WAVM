#if _WIN32

#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Errors.h"
#include "Inline/Unicode.h"
#include "Platform.h"

#include <algorithm>
#include <atomic>
#include <inttypes.h>
#include <memory>

#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>
#include <string>

#define POISON_FORKED_STACK_SELF_POINTERS 0

// An execution context containing all non-volatile registers that will be preserved across calls.
// The layout is mirrored in Windows.asm, so keep them in sync!
struct ExecutionContext
{
	U64 rip;
	U16 cs;
	U64 rflags;
	U64 rsp;
	U16 ss;

	U64 r12;
	U64 r13;
	U64 r14;
	U64 r15;
	U64 rdi;
	U64 rsi;
	U64 rbx;
	U64 rbp;

	__m128 xmm6;
	__m128 xmm7;
	__m128 xmm8;
	__m128 xmm9;
	__m128 xmm10;
	__m128 xmm11;
	__m128 xmm12;
	__m128 xmm13;
	__m128 xmm14;
	__m128 xmm15;
};

static_assert(offsetof(ExecutionContext,rip)       == 0,"unexpected offset");
static_assert(offsetof(ExecutionContext,cs)        == 8,"unexpected offset");
static_assert(offsetof(ExecutionContext,rflags)    == 16,"unexpected offset");
static_assert(offsetof(ExecutionContext,rsp)       == 24,"unexpected offset");
static_assert(offsetof(ExecutionContext,ss)        == 32,"unexpected offset");
static_assert(offsetof(ExecutionContext,r12)       == 40,"unexpected offset");
static_assert(offsetof(ExecutionContext,rbp)       == 96,"unexpected offset");
static_assert(offsetof(ExecutionContext,xmm6)      == 112,"unexpected offset");
static_assert(offsetof(ExecutionContext,xmm15)     == 256,"unexpected offset");
static_assert(sizeof(ExecutionContext)             == 272,"unexpected size");

extern "C" I64 saveExecutionState(ExecutionContext* outContext,I64 returnCode);
extern "C" I64 switchToForkedStackContext(ExecutionContext* forkedContext,U8* trampolineFramePointer) noexcept(false);
extern "C" U8* getStackPointer();

namespace Platform
{
	// Defined in WindowsThreads.cpp
	extern void initThread();

	static Uptr internalGetPreferredVirtualPageSizeLog2()
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		Uptr preferredVirtualPageSize = systemInfo.dwPageSize;
		// Verify our assumption that the virtual page size is a power of two.
		errorUnless(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
		return floorLogTwo(preferredVirtualPageSize);
	}
	Uptr getPageSizeLog2()
	{
		static Uptr preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
		return preferredVirtualPageSizeLog2;
	}

	static U32 memoryAccessAsWin32Flag(MemoryAccess access)
	{
		switch(access)
		{
		default:
		case MemoryAccess::none: return PAGE_NOACCESS;
		case MemoryAccess::readOnly: return PAGE_READONLY;
		case MemoryAccess::readWrite: return PAGE_READWRITE;
		case MemoryAccess::execute: return PAGE_EXECUTE_READ;
		case MemoryAccess::readWriteExecute: return PAGE_EXECUTE_READWRITE;
		}
	}

	static bool isPageAligned(U8* address)
	{
		const Uptr addressBits = reinterpret_cast<Uptr>(address);
		return (addressBits & ((1ull << getPageSizeLog2()) - 1)) == 0;
	}

	U8* allocateVirtualPages(Uptr numPages)
	{
		const Uptr pageSizeLog2 = getPageSizeLog2();
		const Uptr numBytes = numPages << pageSizeLog2;
		void* result = VirtualAlloc(nullptr,numBytes,MEM_RESERVE,PAGE_NOACCESS);
		if(result == nullptr)
		{
			return nullptr;
		}
		return (U8*)result;
	}

	U8* allocateAlignedVirtualPages(Uptr numPages,Uptr alignmentLog2,U8*& outUnalignedBaseAddress)
	{
		const Uptr pageSizeLog2 = getPageSizeLog2();
		const Uptr numBytes = numPages << pageSizeLog2;
		if(alignmentLog2 > pageSizeLog2)
		{
			Uptr numTries = 0;
			while(true)
			{
				// Call VirtualAlloc with enough padding added to the size to align the allocation within the unaligned mapping.
				const Uptr alignmentBytes = 1ull << alignmentLog2;
				void* probeResult = VirtualAlloc(nullptr,numBytes + alignmentBytes,MEM_RESERVE,PAGE_NOACCESS);
				if(!probeResult) { return nullptr; }

				const Uptr address = reinterpret_cast<Uptr>(probeResult);
				const Uptr alignedAddress = (address + alignmentBytes - 1) & ~(alignmentBytes - 1);

				if(numTries < 10)
				{
					// Free the unaligned+padded allocation, and try to immediately reserve just the aligned middle part again.
					// This can fail due to races with other threads, so handle the VirtualAlloc failing by just retrying with
					// a new unaligned+padded allocation.
					errorUnless(VirtualFree(probeResult,0,MEM_RELEASE));
					outUnalignedBaseAddress = (U8*)VirtualAlloc(reinterpret_cast<void*>(alignedAddress),numBytes,MEM_RESERVE,PAGE_NOACCESS);
					if(outUnalignedBaseAddress) { return outUnalignedBaseAddress; }

					++numTries;
				}
				else
				{
					// If the below free and re-alloc of the aligned address fails too many times, just return the padded allocation.
					outUnalignedBaseAddress = (U8*)probeResult;
					return reinterpret_cast<U8*>(alignedAddress);
				}
			}
		}
		else
		{
			outUnalignedBaseAddress = allocateVirtualPages(numPages);
			return outUnalignedBaseAddress;
		}
	}

	bool commitVirtualPages(U8* baseVirtualAddress,Uptr numPages,MemoryAccess access)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		return baseVirtualAddress == VirtualAlloc(baseVirtualAddress,numPages << getPageSizeLog2(),MEM_COMMIT,memoryAccessAsWin32Flag(access));
	}

	bool setVirtualPageAccess(U8* baseVirtualAddress,Uptr numPages,MemoryAccess access)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		DWORD oldProtection = 0;
		return VirtualProtect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsWin32Flag(access),&oldProtection) != 0;
	}
	
	void decommitVirtualPages(U8* baseVirtualAddress,Uptr numPages)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,numPages << getPageSizeLog2(),MEM_DECOMMIT);
		if(baseVirtualAddress && !result) { Errors::fatal("VirtualFree(MEM_DECOMMIT) failed"); }
	}

	void freeVirtualPages(U8* baseVirtualAddress,Uptr numPages)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,0,MEM_RELEASE);
		if(baseVirtualAddress && !result) { Errors::fatal("VirtualFree(MEM_RELEASE) failed"); }
	}

	void freeAlignedVirtualPages(U8* unalignedBaseAddress,Uptr numPages,Uptr alignmentLog2)
	{
		errorUnless(isPageAligned(unalignedBaseAddress));
		auto result = VirtualFree(unalignedBaseAddress,0,MEM_RELEASE);
		if(unalignedBaseAddress && !result) { Errors::fatal("VirtualFree(MEM_RELEASE) failed"); }
	}

	Mutex& getErrorReportingMutex()
	{
		static Platform::Mutex mutex;
		return mutex;
	}

	void handleFatalError(const char* messageFormat,va_list varArgs)
	{
		Lock lock(getErrorReportingMutex());
		std::vfprintf(stderr,messageFormat,varArgs);
		std::fflush(stderr);
		std::abort();
	}

	void handleAssertionFailure(const AssertMetadata& metadata)
	{
		Lock lock(getErrorReportingMutex());
		std::fprintf(
			stderr,
			"Assertion failed at %s(%u): %s\n",
			metadata.file,
			metadata.line,
			metadata.condition
			);
	}

	// The interface to the DbgHelp DLL
	struct DbgHelp
	{
		typedef BOOL (WINAPI* SymFromAddr)(HANDLE,U64,U64*,SYMBOL_INFO*);
		SymFromAddr symFromAddr;

		static DbgHelp* get()
		{
			static Platform::Mutex dbgHelpMutex;
			static DbgHelp* dbgHelp = nullptr;
			if(!dbgHelp)
			{
				Platform::Lock dbgHelpLock(dbgHelpMutex);
				if(!dbgHelp)
				{
					dbgHelp = new DbgHelp();
				}
			}
			return dbgHelp;
		}

	private:

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

	bool describeInstructionPointer(Uptr ip,std::string& outDescription)
	{
		// Initialize DbgHelp.
		DbgHelp* dbgHelp = DbgHelp::get();

		// Allocate up a SYMBOL_INFO struct to receive information about the symbol for this instruction pointer.
		const Uptr maxSymbolNameChars = 256;
		const Uptr symbolAllocationSize = sizeof(SYMBOL_INFO) + sizeof(TCHAR) * (maxSymbolNameChars - 1);
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
	
	#if defined(_WIN32) && defined(_AMD64_)
		void registerSEHUnwindInfo(Uptr imageLoadAddress,Uptr pdataAddress,Uptr pdataNumBytes)
		{
			const U32 numFunctions = (U32)(pdataNumBytes / sizeof(RUNTIME_FUNCTION));

			// Register our manually fixed up copy of the function table.
			if(!RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION*>(pdataAddress),numFunctions,imageLoadAddress))
			{
				Errors::fatal("RtlAddFunctionTable failed");
			}
		}
		void deregisterSEHUnwindInfo(Uptr pdataAddress)
		{
			auto functionTable = reinterpret_cast<RUNTIME_FUNCTION*>(pdataAddress);
			RtlDeleteFunctionTable(functionTable);
		}
	#endif
	
	static CallStack unwindStack(const CONTEXT& immutableContext)
	{
		// Make a mutable copy of the context.
		CONTEXT context;
		memcpy(&context,&immutableContext,sizeof(CONTEXT));

		// Unwind the stack until there isn't a valid instruction pointer, which signals we've reached the base.
		CallStack callStack;
		#ifdef _WIN64
		while(context.Rip)
		{
			callStack.stackFrames.push_back({context.Rip});

			// Look up the SEH unwind information for this function.
			U64 imageBase;
			auto runtimeFunction = RtlLookupFunctionEntry(context.Rip,&imageBase,nullptr);
			if(!runtimeFunction)
			{
				// Leaf functions that don't touch Rsp may not have unwind information.
				context.Rip  = *(U64*)context.Rsp;
				context.Rsp += 8;
			}
			else
			{
				// Use the SEH information to unwind to the next stack frame.
				void* handlerData;
				U64 establisherFrame;
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
		#endif

		return callStack;
	}

	CallStack captureCallStack(Uptr numOmittedFramesFromTop)
	{
		// Capture the current processor state.
		CONTEXT context;
		RtlCaptureContext(&context);

		// Unwind the stack.
		CallStack result = unwindStack(context);

		// Remote the requested number of omitted frames, +1 for this function.
		const Uptr numOmittedFrames = std::min(result.stackFrames.size(),numOmittedFramesFromTop + 1);
		result.stackFrames.erase(result.stackFrames.begin(),result.stackFrames.begin() + numOmittedFrames);

		return result;
	}

	static bool translateSEHToSignal(EXCEPTION_POINTERS* exceptionPointers,Signal& outSignal)
	{
		// Decide how to handle this exception code.
		switch(exceptionPointers->ExceptionRecord->ExceptionCode)
		{
		case EXCEPTION_ACCESS_VIOLATION:
		{
			outSignal.type = Signal::Type::accessViolation;
			outSignal.accessViolation.address = exceptionPointers->ExceptionRecord->ExceptionInformation[1];
			return true;
		}
		case EXCEPTION_STACK_OVERFLOW: outSignal.type = Signal::Type::stackOverflow; return true;
		case STATUS_INTEGER_DIVIDE_BY_ZERO: outSignal.type = Signal::Type::intDivideByZeroOrOverflow; return true;
		case STATUS_INTEGER_OVERFLOW: outSignal.type = Signal::Type::intDivideByZeroOrOverflow; return true;
		default: return false;
		}
	}
	
	// __try/__except doesn't support locals with destructors in the same function, so this is just the body of the
	// sehSignalFilterFunction __try pulled out into a function.
	static LONG CALLBACK sehSignalFilterFunctionNonReentrant(
		EXCEPTION_POINTERS* exceptionPointers,
		const std::function<bool(Signal,const CallStack&)>& filter)
	{
		Signal signal;
		if(!translateSEHToSignal(exceptionPointers,signal)) { return EXCEPTION_CONTINUE_SEARCH; }
		else
		{
			// Unwind the stack frames from the context of the exception.
			CallStack callStack = unwindStack(*exceptionPointers->ContextRecord);

			if(filter(signal,callStack)) { return EXCEPTION_EXECUTE_HANDLER; }
			else { return EXCEPTION_CONTINUE_SEARCH; }
		}
	}

	static LONG CALLBACK sehSignalFilterFunction(
		EXCEPTION_POINTERS* exceptionPointers,
		const std::function<bool(Signal,const CallStack&)>& filter)
	{
		__try { return sehSignalFilterFunctionNonReentrant(exceptionPointers,filter); }
		__except(Errors::fatal("reentrant exception"), true) { Errors::unreachable(); }
	}

	bool catchSignals(
		const std::function<void()>& thunk,
		const std::function<bool(Signal,const CallStack&)>& filter
		)
	{
		initThread();

		__try
		{
			thunk();
			return false;
		}
		__except(sehSignalFilterFunction(GetExceptionInformation(),filter))
		{
			// After a stack overflow, the stack will be left in a damaged state. Let the CRT repair it.
			errorUnless(_resetstkoflw());

			return true;
		}
	}

	static std::atomic<SignalHandler> signalHandler;

	// __try/__except doesn't support locals with destructors in the same function, so this is just the body of the
	// unhandledExceptionFilter __try pulled out into a function.
	static LONG NTAPI unhandledExceptionFilterNonRentrant(struct _EXCEPTION_POINTERS* exceptionPointers)
	{
		Signal signal;

		if(!translateSEHToSignal(exceptionPointers,signal))
		{
			if(exceptionPointers->ExceptionRecord->ExceptionCode == SEH_WAVM_EXCEPTION)
			{
				signal.type = Signal::Type::unhandledException;
				signal.unhandledException.data = reinterpret_cast<void*>(
					exceptionPointers->ExceptionRecord->ExceptionInformation[0]);
			}
			else { return EXCEPTION_CONTINUE_SEARCH; }
		}

		// Unwind the stack frames from the context of the exception.
		CallStack callStack = unwindStack(*exceptionPointers->ContextRecord);

		(signalHandler.load())(signal,callStack);

		return EXCEPTION_CONTINUE_SEARCH;
	}

	static LONG NTAPI unhandledExceptionFilter(struct _EXCEPTION_POINTERS* exceptionPointers)
	{
		__try { return unhandledExceptionFilterNonRentrant(exceptionPointers); }
		__except(Errors::fatal("reentrant exception"), true) { Errors::unreachable(); }
	}

	void setSignalHandler(SignalHandler handler)
	{
		static struct UnhandledExceptionFilterRegistrar
		{
			UnhandledExceptionFilterRegistrar()
			{
				SetUnhandledExceptionFilter(unhandledExceptionFilter);
			}
		} unhandledExceptionFilterRegistrar;

		signalHandler.store(handler);
	}

	static LONG CALLBACK sehPlatformExceptionFilterFunction(EXCEPTION_POINTERS* exceptionPointers,CallStack*& outCallStack,void*& outExceptionData)
	{
		if(exceptionPointers->ExceptionRecord->ExceptionCode != SEH_WAVM_EXCEPTION)
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}
		else
		{
			outExceptionData = reinterpret_cast<void*>(exceptionPointers->ExceptionRecord->ExceptionInformation[0]);

			// Unwind the stack frames from the context of the exception.
			outCallStack = new CallStack(unwindStack(*exceptionPointers->ContextRecord));
			return EXCEPTION_EXECUTE_HANDLER;
		}
	}

	bool catchPlatformExceptions(
		const std::function<void()>& thunk,
		const std::function<void(void*,const CallStack&)>& handler
		)
	{
		CallStack* callStack = nullptr;
		void* exceptionData = nullptr;
		__try
		{
			thunk();
			return false;
		}
		__except(sehPlatformExceptionFilterFunction(GetExceptionInformation(),callStack,exceptionData))
		{
			handler(exceptionData,*callStack);

			delete callStack;
			if(exceptionData) { free(exceptionData); }

			return true;
		}
	}
	
	[[noreturn]] void raisePlatformException(void* data)
	{
		Uptr arguments[1] = { reinterpret_cast<Uptr>(data) };
		RaiseException(U32(SEH_WAVM_EXCEPTION),0,1,arguments);
		Errors::unreachable();
	}

	struct Thread
	{
		HANDLE handle = INVALID_HANDLE_VALUE;
		DWORD id = 0xffffffff;
		std::atomic<I32> numRefs = 2;
		I64 result = -1;

		void releaseRef()
		{
			if(--numRefs == 0)
			{
				delete this;
			}
		}

	private:

		~Thread()
		{
			errorUnless(CloseHandle(handle));
			handle = nullptr;
			id = 0;
		}
	};

	struct ThreadArgs
	{
		Thread* thread;

		ThreadArgs(): thread(nullptr) {}

		~ThreadArgs()
		{
			if(thread)
			{
				thread->releaseRef();
				thread = nullptr;
			}
		}
	};

	struct CreateThreadArgs : ThreadArgs
	{
		I64 (*entry)(void*);
		void* entryArgument;
	};

	struct ForkThreadArgs : ThreadArgs
	{
		ExecutionContext forkContext;
		U8* threadEntryFramePointer;
	};

	struct ExitThreadException
	{
		I64 exitCode;
	};

	static thread_local bool isThreadInitialized = false;
	static thread_local U8* threadEntryFramePointer = nullptr;

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

	static DWORD createThreadEntry2(void* argsVoid)
	{
		initThread();

		std::unique_ptr<CreateThreadArgs> args((CreateThreadArgs*)argsVoid);

		try
		{
			threadEntryFramePointer = getStackPointer();

			args->thread->result = (*args->entry)(args->entryArgument);
		}
		catch(ExitThreadException exception)
		{
			args->thread->result = exception.exitCode;
		}

		return 0;
	}

	static DWORD WINAPI createThreadEntry(void* argsVoid)
	{
		__try { return createThreadEntry2(argsVoid); }
		__except(unhandledExceptionFilter(GetExceptionInformation())) { Errors::unreachable(); }
	}

	Thread* createThread(Uptr numStackBytes,I64 (*entry)(void*),void* entryArgument)
	{
		CreateThreadArgs* args = new CreateThreadArgs;
		auto thread = new Thread;
		args->thread = thread;
		args->entry = entry;
		args->entryArgument = entryArgument;

		thread->handle = CreateThread(
			nullptr,
			numStackBytes,
			createThreadEntry,
			args,
			0,
			&args->thread->id);
		return args->thread;
	}

	void detachThread(Thread* thread)
	{
		wavmAssert(thread);
		thread->releaseRef();
	}

	I64 joinThread(Thread* thread)
	{
		errorUnless(WaitForSingleObject(thread->handle,INFINITE) == WAIT_OBJECT_0);
		const I64 result = thread->result;
		thread->releaseRef();
		return result;
	}

	void exitThread(I64 code)
	{
		throw ExitThreadException {code};
		Errors::unreachable();
	}

	static DWORD forkThreadEntry2(void* argsVoid)
	{
		std::unique_ptr<ForkThreadArgs> args((ForkThreadArgs*)argsVoid);

		try
		{
			threadEntryFramePointer = args->threadEntryFramePointer;

			args->thread->result = switchToForkedStackContext(
				&args->forkContext,
				args->threadEntryFramePointer);
		}
		catch(ExitThreadException exception)
		{
			args->thread->result = exception.exitCode;
		}

		return 0;
	}

	static DWORD WINAPI forkThreadEntry(void* argsVoid)
	{
		__try { return forkThreadEntry2(argsVoid); }
		__except(unhandledExceptionFilter(GetExceptionInformation())) { Errors::unreachable(); }
	}

	Thread* forkCurrentThread()
	{
		auto forkThreadArgs = new ForkThreadArgs;
		auto thread = new Thread;
		forkThreadArgs->thread = thread;

		if(!threadEntryFramePointer)
		{
			Errors::fatal("Cannot fork a thread that wasn't created by Platform::createThread");
		}

		// Capture the current execution state in forkThreadArgs->forkContext.
		// The forked thread will load this execution context, and "return" from this function on the forked stack.
		const I64 isExecutingInFork = saveExecutionState(&forkThreadArgs->forkContext,0);
		if(isExecutingInFork)
		{
			initThread();

			return nullptr;
		}
		else
		{
			// Compute the address extent of this thread's stack.
			const U8* minStackAddr;
			const U8* maxStackAddr;
			GetCurrentThreadStackLimits(
				reinterpret_cast<ULONG_PTR*>(&minStackAddr),
				reinterpret_cast<ULONG_PTR*>(&maxStackAddr));
			const Uptr numStackBytes = maxStackAddr - minStackAddr;

			// Use the current stack pointer derive a conservative bounds on the area of this thread's stack that is active.
			const U8* minActiveStackAddr = getStackPointer() - 128;
			const U8* maxActiveStackAddr = threadEntryFramePointer;
			const Uptr numActiveStackBytes = maxActiveStackAddr - minActiveStackAddr;

			if(numActiveStackBytes + 65536 + 4096 > numStackBytes)
			{
				Errors::fatal("not enough stack space to fork thread");
			}

			// Create a suspended thread.
			forkThreadArgs->thread->handle = CreateThread(
				nullptr,
				numStackBytes,
				forkThreadEntry,
				forkThreadArgs,
				/*STACK_SIZE_PARAM_IS_A_RESERVATION |*/ CREATE_SUSPENDED,
				&forkThreadArgs->thread->id);

			// Read the thread's initial stack pointer.
			CONTEXT* threadContext = new CONTEXT;
			threadContext->ContextFlags = CONTEXT_FULL;
			errorUnless(GetThreadContext(forkThreadArgs->thread->handle,threadContext));

			// Query the virtual address range allocated for the thread's stack.
			auto forkedStackInfo = new MEMORY_BASIC_INFORMATION;
			errorUnless(VirtualQuery(
				reinterpret_cast<void*>(threadContext->Rsp),
				forkedStackInfo,
				sizeof(MEMORY_BASIC_INFORMATION)
			) == sizeof(MEMORY_BASIC_INFORMATION));
			U8* forkedStackMinAddr = reinterpret_cast<U8*>(forkedStackInfo->AllocationBase);
			U8* forkedStackMaxAddr = reinterpret_cast<U8*>(threadContext->Rsp & -16) - 4096;
			delete threadContext;
			delete forkedStackInfo;

			errorUnless(numActiveStackBytes < Uptr(forkedStackMaxAddr - forkedStackMinAddr));

			// Copy the forked stack data.
			if(POISON_FORKED_STACK_SELF_POINTERS)
			{
				const Uptr* source = (const Uptr*)minActiveStackAddr;
				const Uptr* sourceEnd = (const Uptr*)maxActiveStackAddr;
				Uptr* dest = (Uptr*)(forkedStackMaxAddr - numActiveStackBytes);
				wavmAssert(!(reinterpret_cast<Uptr>(source) & 7));
				wavmAssert(!(reinterpret_cast<Uptr>(dest) & 7));
				while(source < sourceEnd)
				{
					if(*source >= reinterpret_cast<Uptr>(minStackAddr)
						&& *source < reinterpret_cast<Uptr>(maxStackAddr))
					{
						*dest++ = 0xCCCCCCCCCCCCCCCC;
						source++;
					}
					else
					{
						*dest++ = *source++;
					}
				};
			}
			else
			{
				memcpy(forkedStackMaxAddr - numActiveStackBytes,minActiveStackAddr,numActiveStackBytes);
			}

			// Compute the offset to add to stack pointers to translate them to the forked thread's stack.
			const Iptr forkedStackOffset = forkedStackMaxAddr - maxActiveStackAddr;
			wavmAssert(!(forkedStackOffset & 15));

			// Translate this thread's captured stack and frame pointers to the forked stack.
			forkThreadArgs->forkContext.rsp += forkedStackOffset;

			// Translate this thread's entry stack pointer to the forked stack.
			forkThreadArgs->threadEntryFramePointer = threadEntryFramePointer + forkedStackOffset;

			ResumeThread(forkThreadArgs->thread->handle);

			return forkThreadArgs->thread;
		}
	}

	U64 getMonotonicClock()
	{
		LARGE_INTEGER performanceCounter;
		LARGE_INTEGER performanceCounterFrequency;
		QueryPerformanceCounter(&performanceCounter);
		QueryPerformanceFrequency(&performanceCounterFrequency);

		const U64 wavmFrequency = 1000000;

		return performanceCounterFrequency.QuadPart > wavmFrequency
			? performanceCounter.QuadPart / (performanceCounterFrequency.QuadPart / wavmFrequency)
			: performanceCounter.QuadPart * (wavmFrequency / performanceCounterFrequency.QuadPart);
	}

	Mutex::Mutex()
	{
		static_assert(sizeof(criticalSection) == sizeof(CRITICAL_SECTION), "");
		static_assert(alignof(CriticalSection) >= alignof(CRITICAL_SECTION), "");
		InitializeCriticalSection((CRITICAL_SECTION*)&criticalSection);
	}

	Mutex::~Mutex()
	{
		DeleteCriticalSection((CRITICAL_SECTION*)&criticalSection);
	}

	void Mutex::lock()
	{
		EnterCriticalSection((CRITICAL_SECTION*)&criticalSection);
	}

	void Mutex::unlock()
	{
		LeaveCriticalSection((CRITICAL_SECTION*)&criticalSection);
	}

	Event* createEvent()
	{
		return reinterpret_cast<Event*>(CreateEvent(nullptr,FALSE,FALSE,nullptr));
	}

	void destroyEvent(Event* event)
	{
		CloseHandle(reinterpret_cast<HANDLE>(event));
	}

	bool waitForEvent(Event* event,U64 untilTime)
	{
		U64 currentTime = getMonotonicClock();
		const U64 startProcessTime = currentTime;
		while(true)
		{
			const U64 timeoutMicroseconds = currentTime > untilTime ? 0 : (untilTime - currentTime);
			const U64 timeoutMilliseconds64 = timeoutMicroseconds / 1000;
			const U32 timeoutMilliseconds32 =
				timeoutMilliseconds64 > UINT32_MAX
				? (UINT32_MAX - 1)
				: U32(timeoutMilliseconds64);

			const U32 waitResult = WaitForSingleObject(reinterpret_cast<HANDLE>(event),timeoutMilliseconds32);
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

	static File* fileHandleToPointer(HANDLE handle)
	{
		return reinterpret_cast<File*>(reinterpret_cast<Uptr>(handle) + 1);
	}

	static HANDLE filePointerToHandle(File* file)
	{
		return reinterpret_cast<HANDLE>(reinterpret_cast<Uptr>(file) - 1);
	}

	File* openFile(const std::string& pathName, FileAccessMode accessMode, FileCreateMode createMode)
	{
		DWORD desiredAccess = 0;
		DWORD shareMode = 0;
		DWORD creationDisposition = 0;
		DWORD flagsAndAttributes = 0;

		switch(accessMode)
		{
		case FileAccessMode::readOnly: desiredAccess = GENERIC_READ; break;
		case FileAccessMode::writeOnly: desiredAccess = GENERIC_WRITE; break;
		case FileAccessMode::readWrite: desiredAccess = GENERIC_READ | GENERIC_WRITE; break;
		default: Errors::unreachable();
		};

		switch(createMode)
		{
		case FileCreateMode::createAlways: creationDisposition = CREATE_ALWAYS; break;
		case FileCreateMode::createNew: creationDisposition = CREATE_NEW; break;
		case FileCreateMode::openAlways: creationDisposition = OPEN_ALWAYS; break;
		case FileCreateMode::openExisting: creationDisposition = OPEN_EXISTING; break;
		case FileCreateMode::truncateExisting: creationDisposition = TRUNCATE_EXISTING; break;
		default: Errors::unreachable();
		};

		const U8* pathNameStart = (const U8*)pathName.c_str();
		const U8* pathNameEnd = pathNameStart + pathName.size();
		std::wstring pathNameW;
		if(Unicode::transcodeUTF8ToUTF16(pathNameStart,pathNameEnd,pathNameW) != pathNameEnd)
		{
			return nullptr;
		}

		HANDLE handle = CreateFileW(pathNameW.c_str(),desiredAccess,shareMode,nullptr,creationDisposition,flagsAndAttributes,nullptr);

		return fileHandleToPointer(handle);
	}

	bool closeFile(File* file)
	{
		return CloseHandle(filePointerToHandle(file)) != 0;
	}

	File* getStdFile(StdDevice device)
	{
		DWORD StdHandle = 0;
		switch(device)
		{
		case StdDevice::in: StdHandle = STD_INPUT_HANDLE; break;
		case StdDevice::out: StdHandle = STD_OUTPUT_HANDLE; break;
		case StdDevice::err: StdHandle = STD_ERROR_HANDLE; break;
		default: Errors::unreachable();
		};

		return fileHandleToPointer(GetStdHandle(StdHandle));
	}

	bool seekFile(File* file,I64 offset,FileSeekOrigin origin,U64& outAbsoluteOffset)
	{
		LONG offsetHigh = LONG((offset >> 32) & 0xffffffff);
		LONG result = SetFilePointer(filePointerToHandle(file),U32(offset & 0xffffffff),&offsetHigh,DWORD(origin));
		if(result == INVALID_SET_FILE_POINTER) { return false; }
		outAbsoluteOffset = (U64(offsetHigh) << 32) | result;
		return true;
	}

	bool readFile(File* file, U8* outData,Uptr numBytes,Uptr& outNumBytesRead)
	{
		outNumBytesRead = 0;
		if(numBytes > Uptr(UINT32_MAX)) { return false; }

		DWORD windowsNumBytesRead = 0;
		const BOOL result = ReadFile(filePointerToHandle(file),outData,U32(numBytes),&windowsNumBytesRead,nullptr);

		outNumBytesRead = Uptr(windowsNumBytesRead);

		return result != 0;
	}

	bool writeFile(File* file,const U8* data,Uptr numBytes,Uptr& outNumBytesWritten)
	{
		outNumBytesWritten = 0;
		if(numBytes > Uptr(UINT32_MAX)) { return false; }

		DWORD windowsNumBytesWritten = 0;
		const BOOL result = WriteFile(filePointerToHandle(file),data, U32(numBytes),&windowsNumBytesWritten,nullptr);

		outNumBytesWritten = Uptr(windowsNumBytesWritten);

		return result != 0;
	}

	bool flushFileWrites(File* file)
	{
		return FlushFileBuffers(filePointerToHandle(file)) != 0;
	}

	std::string getCurrentWorkingDirectory()
	{
		U16 buffer[MAX_PATH];
		const DWORD numChars = GetCurrentDirectoryW(MAX_PATH,(LPWSTR)buffer);
		errorUnless(numChars);

		std::string result;
		const U16* transcodeEnd = Unicode::transcodeUTF16ToUTF8(buffer,buffer + numChars,result);
		errorUnless(transcodeEnd == buffer + numChars);

		return result;
	}
}

#endif
