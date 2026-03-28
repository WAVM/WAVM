// GDB JIT Interface - allows GDB to see JIT-compiled code.
// See: https://sourceware.org/gdb/current/onlinedocs/gdb.html/JIT-Interface.html

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Mutex.h"

using namespace WAVM;

extern "C" {

struct JITCodeEntry
{
	JITCodeEntry* nextEntry;
	JITCodeEntry* prevEntry;
	const char* symfileAddr;
	U64 symfileSize;
};

struct JITDescriptor
{
	U32 version;
	U32 actionFlag; // 0 = no action, 1 = register, 2 = unregister
	JITCodeEntry* relevantEntry;
	JITCodeEntry* firstEntry;
};

// GDB puts a breakpoint on this function and inspects __jit_debug_descriptor when it's called.
// The noinline and the asm prevent calls to this function from being optimized out.
// Both symbols must have default visibility so GDB can find them despite -fvisibility=hidden.
#if defined(__GNUC__) || defined(__clang__)
#define WAVM_GDB_VISIBLE __attribute__((visibility("default")))
#else
#define WAVM_GDB_VISIBLE
#endif

WAVM_GDB_VISIBLE WAVM_FORCENOINLINE void __jit_debug_register_code()
{
#if !defined(_MSC_VER)
	asm volatile("" ::: "memory");
#endif
}

// Must be named exactly __jit_debug_descriptor for GDB to find it.
WAVM_GDB_VISIBLE JITDescriptor __jit_debug_descriptor = {1, 0, nullptr, nullptr};
}

static Platform::Mutex gdbJITMutex;

namespace WAVM { namespace LLVMJIT {

	void* registerObjectWithGDB(const U8* objectBytes, Uptr objectSize)
	{
		Platform::Mutex::Lock lock(gdbJITMutex);

		JITCodeEntry* entry = new JITCodeEntry;
		entry->symfileAddr = reinterpret_cast<const char*>(objectBytes);
		entry->symfileSize = objectSize;

		// Insert at head of linked list.
		entry->prevEntry = nullptr;
		entry->nextEntry = ::__jit_debug_descriptor.firstEntry;
		if(entry->nextEntry) { entry->nextEntry->prevEntry = entry; }
		::__jit_debug_descriptor.firstEntry = entry;

		// Notify GDB.
		::__jit_debug_descriptor.relevantEntry = entry;
		::__jit_debug_descriptor.actionFlag = 1; // JIT_REGISTER_FN
		::__jit_debug_register_code();

		return entry;
	}

	void unregisterObjectWithGDB(void* handle)
	{
		if(!handle) { return; }

		Platform::Mutex::Lock lock(gdbJITMutex);

		JITCodeEntry* entry = static_cast<JITCodeEntry*>(handle);

		// Remove from linked list.
		if(entry->prevEntry) { entry->prevEntry->nextEntry = entry->nextEntry; }
		else
		{
			::__jit_debug_descriptor.firstEntry = entry->nextEntry;
		}
		if(entry->nextEntry) { entry->nextEntry->prevEntry = entry->prevEntry; }

		// Notify GDB.
		::__jit_debug_descriptor.relevantEntry = entry;
		::__jit_debug_descriptor.actionFlag = 2; // JIT_UNREGISTER_FN
		::__jit_debug_register_code();

		delete entry;
	}

}}
