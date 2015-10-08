#ifdef _WIN32

#include "Core/Core.h"
#include "RuntimePrivate.h"

#include <Windows.h>
#include <DbgHelp.h>

#undef min

namespace RuntimePlatform
{
	using namespace Runtime;
	
	// The top two bits are set to indicate an error.
	// The next bit is set to indicate a user-defined exception.
	// The next bit is reserved.
	// https://msdn.microsoft.com/en-us/library/het71c37.aspx
	#define EXCEPTION_WAVM_RUNTIME 0xe0000000

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

	ExecutionContext unwindStack(const CONTEXT& immutableContext)
	{
		// Make a mutable copy of the context.
		CONTEXT context;
		memcpy(&context,&immutableContext,sizeof(CONTEXT));

		// Unwind the stack until there's a valid instruction pointer, which signals we've reached the base.
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

	LONG CALLBACK sehFilterFunction(EXCEPTION_POINTERS* exceptionPointers,Exception*& outRuntimeException)
	{
		// Detect a WAVM runtime exception and just pull the the Exception pointer out for the handler.
		if(exceptionPointers->ExceptionRecord->ExceptionCode == EXCEPTION_WAVM_RUNTIME)
		{
			outRuntimeException = reinterpret_cast<Exception*>(exceptionPointers->ExceptionRecord->ExceptionInformation[0]);
		}
		else
		{
			// Interpret the cause of the exception.
			Exception::Cause cause;
			switch(exceptionPointers->ExceptionRecord->ExceptionCode)
			{
			case EXCEPTION_ACCESS_VIOLATION: cause = Exception::Cause::AccessViolation; break;
			case EXCEPTION_STACK_OVERFLOW: cause = Exception::Cause::StackOverflow; break;
			case EXCEPTION_INT_DIVIDE_BY_ZERO: cause = Exception::Cause::IntegerDivideByZeroOrIntegerOverflow; break;
			case EXCEPTION_INT_OVERFLOW: cause = Exception::Cause::IntegerDivideByZeroOrIntegerOverflow; break;
			default: cause = Exception::Cause::Unknown; break;
			}

			// Unwind the stack frames from the context of the exception.
			auto executionContext = unwindStack(*exceptionPointers->ContextRecord);

			// Describe the exception's call stack.
			outRuntimeException = new Exception {cause,describeExecutionContext(executionContext)};
		}
		return EXCEPTION_EXECUTE_HANDLER;
	}

	Value catchRuntimeExceptions(const std::function<Value()>& thunk)
	{
		Exception* runtimeException = nullptr;
		__try
		{
			return thunk();
		}
		__except(sehFilterFunction(GetExceptionInformation(),runtimeException))
		{
			return Value(runtimeException);
		}
	}

	void raiseException(Runtime::Exception* exception)
	{
		RaiseException(EXCEPTION_WAVM_RUNTIME,0,1,reinterpret_cast<ULONG_PTR*>(&exception));
	}

	void registerSEHUnwindInfo(uintptr textLoadAddress,uintptr xdataLoadAddress,uintptr pdataLoadAddress,size_t pdataNumBytes)
	{
		// Use the smallest address of the 3 segments as the base address of the image.
		// This assumes that the segments are all loaded less than 2GB away from each other!
		auto imageBaseAddress = std::min(textLoadAddress,std::min(pdataLoadAddress,xdataLoadAddress));

		// The LLVM COFF dynamic loader doesn't handle the image-relative relocations used by the pdata section,
		// and overwrites those values with o: https://github.com/llvm-mirror/llvm/blob/e84d8c12d5157a926db15976389f703809c49aa5/lib/ExecutionEngine/RuntimeDyld/Targets/RuntimeDyldCOFFX86_64.h#L96
		// This works around that by making a copy of the RUNTIME_FUNCTION structs and doing a manual relocation
		// with some assumptions about the relocations.
		const uint32 numFunctions = (uint32)(pdataNumBytes / sizeof(RUNTIME_FUNCTION));
		auto functions = reinterpret_cast<RUNTIME_FUNCTION*>(pdataLoadAddress);
		auto functionsCopy = new RUNTIME_FUNCTION[numFunctions];
		uintptr currentFunctionTextLoadAddr = textLoadAddress;
		for(uint32 functionIndex = 0;functionIndex < numFunctions;++functionIndex)
		{
			const auto& function = functions[functionIndex];
			auto& functionCopy = functionsCopy[functionIndex];

			// BeginAddress and EndAddress are relative to the start of the function, so BeginAddress should be 0 and EndAddress the length of the function's code.
			functionCopy.BeginAddress = (uint32)(currentFunctionTextLoadAddr + function.BeginAddress - imageBaseAddress);
			functionCopy.EndAddress = (uint32)(currentFunctionTextLoadAddr + function.EndAddress - imageBaseAddress);

			// UnwindInfoAddress is an offset relative to the start of the xdata section.
			functionCopy.UnwindInfoAddress = (uint32)(xdataLoadAddress + function.UnwindInfoAddress - imageBaseAddress);

			// Assume that the next function immediately follows the current function at the next 16-byte aligned address.
			currentFunctionTextLoadAddr += (function.EndAddress + 15) & ~15;
		}

		// Register our manually fixed up copy of the function table.
		if(!RtlAddFunctionTable(functionsCopy,numFunctions,imageBaseAddress))
		{
			throw;
		}
	}
	
	bool describeInstructionPointer(uintptr_t ip,std::string& outDescription)
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

	Runtime::ExecutionContext captureExecutionContext()
	{
		// Capture the current processor state.
		CONTEXT context;
		RtlCaptureContext(&context);

		// Unwind the stack.
		auto result = unwindStack(context);

		// Remove the top stack frame so the first entry is the caller of this function.
		result.stackFrames.erase(result.stackFrames.begin());

		return result;
	}
}

#endif