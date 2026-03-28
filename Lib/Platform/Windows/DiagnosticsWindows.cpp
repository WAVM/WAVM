#if WAVM_PLATFORM_WINDOWS

#include <inttypes.h>
#include <stdio.h>
#include <atomic>
#include <cstring>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Alloca.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Mutex.h"
#include "WindowsPrivate.h"

#include <libloaderapi.h>

#include <DbgHelp.h>

using namespace WAVM;
using namespace WAVM::Platform;

// The interface to the DbgHelp DLL
struct DbgHelp
{
	typedef BOOL(WINAPI* SymFromAddr)(HANDLE, U64, U64*, SYMBOL_INFO*);
	SymFromAddr symFromAddr;

	static DbgHelp* get()
	{
		static Platform::Mutex dbgHelpMutex;
		static DbgHelp* dbgHelp = nullptr;
		if(!dbgHelp)
		{
			Platform::Mutex::Lock dbgHelpLock(dbgHelpMutex);
			if(!dbgHelp) { dbgHelp = new DbgHelp(); }
		}
		return dbgHelp;
	}

private:
	DbgHelp()
	{
		HMODULE dbgHelpModule = ::LoadLibraryA("Dbghelp.dll");
		if(dbgHelpModule)
		{
			symFromAddr = (SymFromAddr)::GetProcAddress(dbgHelpModule, "SymFromAddr");

			// Initialize the debug symbol lookup.
			typedef BOOL(WINAPI * SymInitialize)(HANDLE, PCTSTR, BOOL);
			SymInitialize symInitialize
				= (SymInitialize)::GetProcAddress(dbgHelpModule, "SymInitialize");
			if(symInitialize) { symInitialize(GetCurrentProcess(), nullptr, TRUE); }
		}
	}
};

static HMODULE getCurrentModule()
{
	HMODULE module = nullptr;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)getCurrentModule, &module);
	return module;
}

static HMODULE getModuleFromBaseAddress(Uptr baseAddress)
{
	return reinterpret_cast<HMODULE>(baseAddress);
}

static void getModuleName(HMODULE module, char* outName, Uptr outNameSize)
{
	U32 len = GetModuleFileNameA(module, outName, U32(outNameSize));
	if(len >= outNameSize) { len = U32(outNameSize - 1); }
	outName[len] = '\0';
}

static void trimModuleName(char* moduleName, Uptr moduleNameSize)
{
	// Get this module's path to find the common directory prefix.
	char thisModuleName[MAX_PATH + 1];
	getModuleName(getCurrentModule(), thisModuleName, sizeof(thisModuleName));

	// Find the last backslash in this module's path.
	const char* lastBackslash = strrchr(thisModuleName, '\\');
	if(!lastBackslash) { return; }

	Uptr prefixLen = Uptr(lastBackslash - thisModuleName);
	if(strlen(moduleName) > prefixLen && strncmp(moduleName, thisModuleName, prefixLen) == 0)
	{
		// Remove the common directory prefix.
		Uptr remaining = strlen(moduleName + prefixLen + 1);
		memmove(moduleName, moduleName + prefixLen + 1, remaining + 1);
	}
}

bool Platform::getInstructionSourceByAddress(Uptr ip, InstructionSource& outSource)
{
	// Look up static symbol information using DbgHelp.
	DbgHelp* dbgHelp = DbgHelp::get();

	// Allocate a SYMBOL_INFO struct to receive information about the symbol for this
	// instruction pointer.
	const Uptr maxSymbolNameChars = 256;
	const Uptr symbolAllocationSize
		= sizeof(SYMBOL_INFO) + sizeof(TCHAR) * (maxSymbolNameChars - 1);
	SYMBOL_INFO* symbolInfo = (SYMBOL_INFO*)alloca(symbolAllocationSize);
	memset(symbolInfo, 0, symbolAllocationSize);
	symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbolInfo->MaxNameLen = maxSymbolNameChars;

	// Call DbgHelp::SymFromAddr to try to find any debug symbol containing this address.
	U64 displacement;
	if(!dbgHelp->symFromAddr(GetCurrentProcess(), ip, &displacement, symbolInfo)) { return false; }
	else
	{
		getModuleName(getModuleFromBaseAddress(Uptr(symbolInfo->ModBase)),
					  outSource.module.mutableData(),
					  outSource.module.capacity());
		trimModuleName(outSource.module.mutableData(), outSource.module.capacity());
		outSource.function.assign(symbolInfo->Name, symbolInfo->NameLen);
		outSource.instructionOffset = Uptr(displacement);
		return true;
	}
}

static std::atomic<Uptr> numCommittedPageBytes{0};

void Platform::printMemoryProfile()
{
	printf("Committed virtual pages: %" PRIuPTR " KB\n",
		   numCommittedPageBytes.load(std::memory_order_seq_cst) / 1024);
	fflush(stdout);
}

void Platform::registerVirtualAllocation(Uptr numBytes) { numCommittedPageBytes += numBytes; }

void Platform::deregisterVirtualAllocation(Uptr numBytes) { numCommittedPageBytes -= numBytes; }

#endif // WAVM_PLATFORM_WINDOWS
