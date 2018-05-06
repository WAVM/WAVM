#pragma once

#include "Inline/BasicTypes.h"

#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32
	#define FORCEINLINE __forceinline
	#define FORCENOINLINE __declspec(noinline)
	#define SUPPRESS_UNUSED(variable) (void)(variable);
	#include <intrin.h>
	#define PACKED_STRUCT(definition) __pragma(pack(push, 1)) definition; __pragma(pack(pop))
	#define NO_ASAN
	#define RETURNS_TWICE
	#define UNLIKELY(condition) (condition)
	#define DEBUG_TRAP() __debugbreak()
#else
	#define FORCEINLINE inline __attribute__((always_inline))
	#define FORCENOINLINE __attribute__((noinline))
	#define SUPPRESS_UNUSED(variable) (void)(variable);
	#define PACKED_STRUCT(definition) definition __attribute__((packed));
	#define NO_ASAN __attribute__((no_sanitize_address))
	#define RETURNS_TWICE __attribute__((returns_twice))
	#define UNLIKELY(condition) __builtin_expect(condition,0)
	#define DEBUG_TRAP() __asm__ __volatile__ ("int3")
#endif

#ifndef PLATFORM_API
	#define PLATFORM_API DLL_IMPORT
#endif

namespace Platform
{
	// The number of bytes in a cache line: assume 64 for now.
	enum { numCacheLineBytes = 64 };

	// countLeadingZeroes/countTrailingZeroes returns the number of leading/trailing zeroes, or the bit width of the input if no bits are set.
	#ifdef _WIN32
		// BitScanReverse/BitScanForward return 0 if the input is 0.
		inline U32 countLeadingZeroes(U32 value) { unsigned long result; return _BitScanReverse(&result,value) ? (31 - result) : 32; }
		inline U32 countTrailingZeroes(U32 value) { unsigned long result; return _BitScanForward(&result,value) ? result : 32; }
		
		#ifdef _WIN64
			inline U64 countLeadingZeroes(U64 value) { unsigned long result; return _BitScanReverse64(&result,value) ? (63 - result) : 64; }
			inline U64 countTrailingZeroes(U64 value) { unsigned long result; return _BitScanForward64(&result,value) ? result : 64; }
		#else
			inline U64 countLeadingZeroes(U64 value) { throw; }
			inline U64 countTrailingZeroes(U64 value) { throw; }
		#endif
	#else
		// __builtin_clz/__builtin_ctz are undefined if the input is 0. 
		inline U64 countLeadingZeroes(U64 value) { return value == 0 ? 64 : __builtin_clzll(value); }
		inline U32 countLeadingZeroes(U32 value) { return value == 0 ? 32 : __builtin_clz(value); }
		inline U64 countTrailingZeroes(U64 value) { return value == 0 ? 64 : __builtin_ctzll(value); }
		inline U32 countTrailingZeroes(U32 value) { return value == 0 ? 32 : __builtin_ctz(value); }
	#endif
	inline U64 floorLogTwo(U64 value) { return value <= 1 ? 0 : 63 - countLeadingZeroes(value); }
	inline U32 floorLogTwo(U32 value) { return value <= 1 ? 0 : 31 - countLeadingZeroes(value); }
	inline U64 ceilLogTwo(U64 value) { return value <= 1 ? 0 : 63 - countLeadingZeroes(value * 2 - 1); }
	inline U32 ceilLogTwo(U32 value) { return value <= 1 ? 0 : 31 - countLeadingZeroes(value * 2 - 1); }

	inline U64 saturateToBounds(U64 value,U64 maxValue)
	{
		return U64(value + ((I64(maxValue - value) >> 63) & (maxValue - value)));
	}

	inline U32 saturateToBounds(U32 value,U32 maxValue)
	{
		return U32(value + ((I32(maxValue - value) >> 31) & (maxValue - value)));
	}

	//
	// Memory
	//

	// Describes allowed memory accesses.
	enum class MemoryAccess
	{
		none,
		readOnly,
		readWrite,
		execute,
		readWriteExecute
	};

	// Returns the base 2 logarithm of the smallest virtual page size.
	PLATFORM_API Uptr getPageSizeLog2();

	// Allocates virtual addresses without commiting physical pages to them.
	// Returns the base virtual address of the allocated addresses, or nullptr if the virtual address space has been exhausted.
	PLATFORM_API U8* allocateVirtualPages(Uptr numPages);

	// Allocates virtual addresses without commiting physical pages to them.
	// Returns the base virtual address of the allocated addresses, or nullptr if the virtual address space has been exhausted.
	PLATFORM_API U8* allocateAlignedVirtualPages(Uptr numPages,Uptr alignmentLog2,U8*& outUnalignedBaseAddress);

	// Commits physical memory to the specified virtual pages.
	// baseVirtualAddress must be a multiple of the preferred page size.
	// Return true if successful, or false if physical memory has been exhausted.
	PLATFORM_API bool commitVirtualPages(U8* baseVirtualAddress,Uptr numPages,MemoryAccess access = MemoryAccess::readWrite);

	// Changes the allowed access to the specified virtual pages.
	// baseVirtualAddress must be a multiple of the preferred page size.
	// Return true if successful, or false if the access-level could not be set.
	PLATFORM_API bool setVirtualPageAccess(U8* baseVirtualAddress,Uptr numPages,MemoryAccess access);

	// Decommits the physical memory that was committed to the specified virtual pages.
	// baseVirtualAddress must be a multiple of the preferred page size.
	PLATFORM_API void decommitVirtualPages(U8* baseVirtualAddress,Uptr numPages);

	// Frees virtual addresses. Any physical memory committed to the addresses must have already been decommitted.
	// baseVirtualAddress must also be an address returned by allocateVirtualPages.
	PLATFORM_API void freeVirtualPages(U8* baseVirtualAddress,Uptr numPages);

	// Frees an aligned virtual address block. Any physical memory committed to the addresses must have already been decommitted.
	// unalignedBaseAddress must be the unaligned base address returned by allocateAlignedVirtualPages.
	PLATFORM_API void freeAlignedVirtualPages(U8* unalignedBaseAddress,Uptr numPages,Uptr alignmentLog2);

	//
	// Error reporting
	//

	PACKED_STRUCT(struct AssertMetadata
	{
		const char* condition;
		const char* file;
		U32 line;
	});

	PLATFORM_API void handleAssertionFailure(const AssertMetadata& metadata);
	[[noreturn]] PLATFORM_API void handleFatalError(const char* messageFormat,va_list varArgs);

	//
	// Call stack and exceptions
	//

	// Describes a call stack.
	struct CallStack
	{
		struct Frame
		{
			Uptr ip;
		};
		std::vector<Frame> stackFrames;
	};

	// Captures the execution context of the caller.
	PLATFORM_API CallStack captureCallStack(Uptr numOmittedFramesFromTop = 0);
	
	// Describes an instruction pointer.
	PLATFORM_API bool describeInstructionPointer(Uptr ip,std::string& outDescription);

	#ifdef _WIN64
		// Registers/deregisters the data used by Windows SEH to unwind stack frames.
		PLATFORM_API void registerSEHUnwindInfo(Uptr imageLoadAddress,Uptr pdataAddress,Uptr pdataNumBytes);
		PLATFORM_API void deregisterSEHUnwindInfo(Uptr pdataAddress);
	#endif

	struct Signal
	{
		enum class Type
		{
			invalid = 0,
			accessViolation,
			stackOverflow,
			intDivideByZeroOrOverflow,
			unhandledException
		};

		Type type = Type::invalid;

		union
		{
			struct
			{
				Uptr address;
			} accessViolation;

			struct
			{
				void* data;
			} unhandledException;
		};
	};

	PLATFORM_API bool catchSignals(
		const std::function<void()>& thunk,
		const std::function<bool(Signal signal,const CallStack&)>& filter
		);

	typedef bool (*SignalHandler)(Signal,const CallStack&);

	PLATFORM_API void setSignalHandler(SignalHandler handler);

	// Calls a thunk, catching any platform exceptions raised.
	// If a platform exception is caught, the exception is passed to the handler function, and true is returned.
	// If no exceptions are caught, false is returned.
	PLATFORM_API bool catchPlatformExceptions(
		const std::function<void()>& thunk,
		const std::function<void(void*,const CallStack&)>& handler
		);

	[[noreturn]] PLATFORM_API void raisePlatformException(void* data);

	#ifdef _WIN64
		enum { SEH_WAVM_EXCEPTION = 0xE0000001 };
	#else
		PLATFORM_API std::type_info* getUserExceptionTypeInfo();
	#endif

	//
	// Threading
	//

	struct Thread;
	PLATFORM_API Thread* createThread(Uptr numStackBytes,I64 (*threadEntry)(void*),void* argument);
	PLATFORM_API void detachThread(Thread* thread);
	PLATFORM_API I64 joinThread(Thread* thread);
	[[noreturn]] PLATFORM_API void exitThread(I64 code);
	
	RETURNS_TWICE PLATFORM_API Thread* forkCurrentThread();

	// Returns the current value of a clock that may be used as an absolute time for wait timeouts.
	// The resolution is microseconds, and the origin is arbitrary.
	PLATFORM_API U64 getMonotonicClock();

	// Platform-independent mutexes.
	struct Mutex
	{
		PLATFORM_API Mutex();
		PLATFORM_API ~Mutex();

		Mutex(const Mutex&) = delete;
		Mutex(Mutex&&) = delete;

		void operator=(const Mutex&) = delete;
		void operator=(Mutex&&) = delete;

		PLATFORM_API void lock();
		PLATFORM_API void unlock();

	private:
		#ifdef WIN32
		struct CriticalSection
		{
			struct _RTL_CRITICAL_SECTION_DEBUG* debugInfo;
			I32 lockCount;
			I32 recursionCount;
			Uptr owningThreadHandle;
			Uptr lockSemaphoreHandle;
			Uptr spinCount;
		} criticalSection;
		#elif defined(__linux__)
		struct PthreadMutex
		{
			Uptr data[5];
		} pthreadMutex;
		#elif defined(__APPLE__)
		struct PthreadMutex
		{
			Uptr data[8];
		} pthreadMutex;
		#else
			#error unsupported platform
		#endif
	};

	// RAII-style lock for Mutex.
	struct Lock
	{
		Lock(Mutex& inMutex): mutex(&inMutex) { mutex->lock(); }
		~Lock() { unlock(); }

		void unlock()
		{
			if(mutex)
			{
				mutex->unlock();
				mutex = nullptr;
			}
		}

	private:
		Mutex* mutex;
	};

	// Platform-independent events.
	struct Event;
	PLATFORM_API Event* createEvent();
	PLATFORM_API void destroyEvent(Event* event);
	PLATFORM_API bool waitForEvent(Event* event,U64 untilClock);
	PLATFORM_API void signalEvent(Event* event);

	//
	// File I/O
	//

	enum class FileAccessMode
	{
		readOnly = 0x1,
		writeOnly = 0x2,
		readWrite = 0x1 | 0x2,
	};

	enum class FileCreateMode
	{
		createAlways,
		createNew,
		openAlways,
		openExisting,
		truncateExisting,
	};

	enum class StdDevice
	{
		in,
		out,
		err,
	};

	enum class FileSeekOrigin
	{
		begin = 0,
		cur = 1,
		end = 2
	};

	struct File;
	PLATFORM_API File* openFile(const std::string& pathName,FileAccessMode accessMode,FileCreateMode createMode);
	PLATFORM_API bool closeFile(File* file);
	PLATFORM_API File* getStdFile(StdDevice device);
	PLATFORM_API bool seekFile(File* file,I64 offset,FileSeekOrigin origin,U64* outAbsoluteOffset = nullptr);
	PLATFORM_API bool readFile(File* file,U8* outData,Uptr numBytes,Uptr* outNumBytesRead = nullptr);
	PLATFORM_API bool writeFile(File* file,const U8* data,Uptr numBytes,Uptr* outNumBytesWritten = nullptr);
	PLATFORM_API bool flushFileWrites(File* file);
	PLATFORM_API std::string getCurrentWorkingDirectory();
}
