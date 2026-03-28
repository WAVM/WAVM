#if WAVM_PLATFORM_POSIX

#include <dlfcn.h>
#include <stdio.h>
#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Platform/Diagnostics.h"

#if WAVM_ENABLE_ASAN                                                                               \
	&& (defined(HAS_SANITIZER_PRINT_MEMORY_PROFILE_1)                                              \
		|| defined(HAS_SANITIZER_PRINT_MEMORY_PROFILE_2))
#include <sanitizer/common_interface_defs.h>
#endif

using namespace WAVM;
using namespace WAVM::Platform;

extern "C" const char* __asan_default_options()
{
	return "handle_segv=false"
		   ":handle_sigbus=false"
		   ":handle_sigfpe=false"
		   ":replace_intrin=false";
}

extern "C" const char* __tsan_default_options()
{
	// WAVM handles signals itself for WebAssembly traps.
	// Disable TSAN's signal interception to avoid conflicts with our signal handlers
	// and stack unwinding through JIT-compiled code.
	return "handle_segv=0"
		   ":handle_sigbus=0"
		   ":handle_sigfpe=0";
}

bool Platform::getInstructionSourceByAddress(Uptr ip, InstructionSource& outSource)
{
#if defined(__linux__) || defined(__APPLE__)
	// Look up static symbol information for the address.
	// dladdr is async-signal-safe on Linux and macOS.
	Dl_info symbolInfo;
	if(dladdr((void*)ip, &symbolInfo))
	{
		WAVM_ASSERT(symbolInfo.dli_fname);
		outSource.module = symbolInfo.dli_fname;
		if(!symbolInfo.dli_sname)
		{
			outSource.instructionOffset = ip - reinterpret_cast<Uptr>(symbolInfo.dli_fbase);
		}
		else
		{
			// Skip demangling in signal-safe path; use the raw symbol name.
			outSource.function = symbolInfo.dli_sname;
			outSource.instructionOffset = ip - reinterpret_cast<Uptr>(symbolInfo.dli_saddr);
		}
		return true;
	}
#endif
	return false;
}

static std::atomic<Uptr> numCommittedPageBytes{0};

void Platform::printMemoryProfile()
{
#if WAVM_ENABLE_ASAN
#if defined(HAS_SANITIZER_PRINT_MEMORY_PROFILE_1)
	__sanitizer_print_memory_profile(100);
#elif defined(HAS_SANITIZER_PRINT_MEMORY_PROFILE_2)
	__sanitizer_print_memory_profile(100, 20);
#endif
#endif
	printf("Committed virtual pages: %" PRIuPTR " KB\n",
		   uintptr_t(numCommittedPageBytes.load(std::memory_order_seq_cst) / 1024));
	fflush(stdout);
}

void Platform::registerVirtualAllocation(Uptr numBytes) { numCommittedPageBytes += numBytes; }

void Platform::deregisterVirtualAllocation(Uptr numBytes) { numCommittedPageBytes -= numBytes; }

#endif // WAVM_PLATFORM_POSIX
