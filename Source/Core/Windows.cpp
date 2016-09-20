#if _WIN32

#include "Core.h"
#include "Platform.h"
#include <Windows.h>
#include <inttypes.h>
#include <DbgHelp.h>

#undef min

namespace Platform
{
	Mutex::Mutex()
	{
		handle = new CRITICAL_SECTION();
		InitializeCriticalSection((CRITICAL_SECTION*)handle);
	}

	Mutex::~Mutex()
	{
		DeleteCriticalSection((CRITICAL_SECTION*)handle);
	}

	void Mutex::Lock()
	{
		EnterCriticalSection((CRITICAL_SECTION*)handle);
	}

	void Mutex::Unlock()
	{
		LeaveCriticalSection((CRITICAL_SECTION*)handle);
	}

	static size_t internalGetPreferredVirtualPageSizeLog2()
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		size_t preferredVirtualPageSize = systemInfo.dwPageSize;
		// Verify our assumption that the virtual page size is a power of two.
		assert(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
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

	#if _DEBUG
		static bool isPageAligned(uint8* address)
		{
			const uintp addressBits = reinterpret_cast<uintp>(address);
			return (addressBits & ((1ull << getPageSizeLog2()) - 1)) == 0;
		}
	#endif

	uint8* allocateVirtualPages(size_t numPages)
	{
		size_t numBytes = numPages << getPageSizeLog2();
		auto result = VirtualAlloc(nullptr,numBytes,MEM_RESERVE,PAGE_NOACCESS);
		if(result == NULL)
		{
			Log::printf(Log::Category::error,"VirtualAlloc(%" PRIuPTR " KB) failed: GetLastError=%u\n",numBytes/1024,GetLastError());
			return nullptr;
		}
		return (uint8*)result;
	}

	bool commitVirtualPages(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		assert(isPageAligned(baseVirtualAddress));
		return baseVirtualAddress == VirtualAlloc(baseVirtualAddress,numPages << getPageSizeLog2(),MEM_COMMIT,memoryAccessAsWin32Flag(access));
	}

	bool setVirtualPageAccess(uint8* baseVirtualAddress,size_t numPages,MemoryAccess access)
	{
		assert(isPageAligned(baseVirtualAddress));
		DWORD oldProtection = 0;
		return VirtualProtect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsWin32Flag(access),&oldProtection) != 0;
	}
	
	void decommitVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,numPages << getPageSizeLog2(),MEM_DECOMMIT);
		if(baseVirtualAddress && !result) { Core::fatalError("VirtualFree(MEM_DECOMMIT) failed"); }
	}

	void freeVirtualPages(uint8* baseVirtualAddress,size_t numPages)
	{
		assert(isPageAligned(baseVirtualAddress));
		auto result = VirtualFree(baseVirtualAddress,0/*numPages << getPageSizeLog2()*/,MEM_RELEASE);
		if(baseVirtualAddress && !result) { Core::fatalError("VirtualFree(MEM_RELEASE) failed"); }
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
	
	void* registerSEHUnwindInfo(uintp imageLoadAddress,uintp textLoadAddress,uintp xdataLoadAddress,uintp pdataLoadAddress,size_t pdataNumBytes)
	{
		// The LLVM COFF dynamic loader doesn't handle the image-relative relocations used by the pdata section,
		// and overwrites those values with o: https://github.com/llvm-mirror/llvm/blob/e84d8c12d5157a926db15976389f703809c49aa5/lib/ExecutionEngine/RuntimeDyld/Targets/RuntimeDyldCOFFX86_64.h#L96
		// This works around that by making a copy of the RUNTIME_FUNCTION structs and doing a manual relocation
		// with some assumptions about the relocations.
		const uint32 numFunctions = (uint32)(pdataNumBytes / sizeof(RUNTIME_FUNCTION));
		auto functions = reinterpret_cast<RUNTIME_FUNCTION*>(pdataLoadAddress);
		auto functionsCopy = new RUNTIME_FUNCTION[numFunctions];
		uintp currentFunctionTextLoadAddr = textLoadAddress;
		for(uint32 functionIndex = 0;functionIndex < numFunctions;++functionIndex)
		{
			const auto& function = functions[functionIndex];
			auto& functionCopy = functionsCopy[functionIndex];

			// BeginAddress and EndAddress are relative to the start of the function, so BeginAddress should be 0 and EndAddress the length of the function's code.
			functionCopy.BeginAddress = (uint32)(currentFunctionTextLoadAddr + function.BeginAddress - imageLoadAddress);
			functionCopy.EndAddress = (uint32)(currentFunctionTextLoadAddr + function.EndAddress - imageLoadAddress);

			// UnwindInfoAddress is an offset relative to the start of the xdata section.
			functionCopy.UnwindInfoAddress = (uint32)(xdataLoadAddress + function.UnwindInfoAddress - imageLoadAddress);

			// Assume that the next function immediately follows the current function at the next 16-byte aligned address.
			currentFunctionTextLoadAddr += (function.EndAddress + 15) & ~15;
		}

		// Register our manually fixed up copy of the function table.
		if(!RtlAddFunctionTable(functionsCopy,numFunctions,imageLoadAddress))
		{
			Core::fatalError("RtlAddFunctionTable failed");
		}

		return functionsCopy;
	}
	void deregisterSEHUnwindInfo(void* registerResult)
	{
		auto functionTable = (RUNTIME_FUNCTION*)registerResult;
		RtlDeleteFunctionTable(functionTable);
		delete [] functionTable;
	}
	
	ExecutionContext unwindStack(const CONTEXT& immutableContext)
	{
		// Make a mutable copy of the context.
		CONTEXT context;
		memcpy(&context,&immutableContext,sizeof(CONTEXT));

		// Unwind the stack until there isn't a valid instruction pointer, which signals we've reached the base.
		ExecutionContext executionContext;
		while(context.Rip)
		{
			executionContext.stackFrames.push_back({context.Rip,context.Rsp});

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

		return executionContext;
	}

	ExecutionContext captureExecutionContext(uintp numOmittedFramesFromTop)
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
	LONG CALLBACK sehFilterFunction(EXCEPTION_POINTERS* exceptionPointers,HardwareTrapType& outType,uintp& outOperand,ExecutionContext& outContext)
	{
		if(isReentrantException) { Core::fatalError("reentrant exception"); }
		else
		{
			// Decide how to handle this exception code.
			switch(exceptionPointers->ExceptionRecord->ExceptionCode)
			{
			case EXCEPTION_ACCESS_VIOLATION:
				outType = HardwareTrapType::accessViolation;
				outOperand = exceptionPointers->ExceptionRecord->ExceptionInformation[1];
				break;
			case EXCEPTION_STACK_OVERFLOW: outType = HardwareTrapType::stackOverflow; break;
			case STATUS_INTEGER_DIVIDE_BY_ZERO: outType = HardwareTrapType::intDivideByZeroOrOverflow; break;
			case STATUS_INTEGER_OVERFLOW: outType = HardwareTrapType::intDivideByZeroOrOverflow; break;
			default: return EXCEPTION_CONTINUE_SEARCH;
			}
			isReentrantException = true;

			// Unwind the stack frames from the context of the exception.
			outContext = unwindStack(*exceptionPointers->ContextRecord);

			return EXCEPTION_EXECUTE_HANDLER;
		}
	}

	THREAD_LOCAL bool isThreadInitialized = false;
	void initThread()
	{
		assert(!isThreadInitialized);
		isThreadInitialized = true;

		// Ensure that there's enough space left on the stack in the case of a stack overflow to prepare the stack trace.
		ULONG stackOverflowReserveBytes = 32768;
		SetThreadStackGuarantee(&stackOverflowReserveBytes);
	}

	HardwareTrapType catchHardwareTraps(
		ExecutionContext& outContext,
		uintp& outOperand,
		const std::function<void()>& thunk
		)
	{
		assert(isThreadInitialized);

		HardwareTrapType result = HardwareTrapType::none;
		__try
		{
			thunk();
		}
		__except(sehFilterFunction(GetExceptionInformation(),result,outOperand,outContext))
		{
			isReentrantException = false;
			
			// After a stack overflow, the stack will be left in a damaged state. Let the CRT repair it.
			if(result == HardwareTrapType::stackOverflow) { _resetstkoflw(); }
		}
		return result;
	}
}

#endif
