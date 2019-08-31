#pragma once

#include <setjmp.h>
#include <functional>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Signal.h"

// This struct layout is replicated in POSIX.S
struct ExecutionContext
{
	U64 rbx;
	U64 rsp;
	U64 r12;
	U64 r13;
	U64 r14;
	U64 r15;
	U64 rbp;
	U64 rip;
};
static_assert(offsetof(ExecutionContext, rbx) == 0, "unexpected offset");
static_assert(offsetof(ExecutionContext, rsp) == 8, "unexpected offset");
static_assert(offsetof(ExecutionContext, r12) == 16, "unexpected offset");
static_assert(offsetof(ExecutionContext, r13) == 24, "unexpected offset");
static_assert(offsetof(ExecutionContext, r14) == 32, "unexpected offset");
static_assert(offsetof(ExecutionContext, r15) == 40, "unexpected offset");
static_assert(offsetof(ExecutionContext, rbp) == 48, "unexpected offset");
static_assert(offsetof(ExecutionContext, rip) == 56, "unexpected offset");
static_assert(sizeof(ExecutionContext) == 64, "unexpected size");

#ifdef __WAVIX__
// libunwind dynamic frame registration
inline void __register_frame(const void* fde)
{
	WAVM::Errors::unimplemented("Wavix __register_frame");
}

inline void __deregister_frame(const void* fde)
{
	WAVM::Errors::unimplemented("Wavix __deregister_frame");
}

#else
// libunwind dynamic frame registration
extern "C" void __register_frame(const void* fde);
extern "C" void __deregister_frame(const void* fde);
#endif

namespace WAVM { namespace Platform {

	struct CallStack;

	struct SignalContext
	{
		SignalContext* outerContext;
		jmp_buf catchJump;
		bool (*filter)(void*, Signal, CallStack&&);
		void* filterArgument;
	};

	struct SigAltStack
	{
		~SigAltStack() { deinit(); }

		void init();
		void deinit();

		void getNonSignalStack(U8*& outMinGuardAddr, U8*& outMinAddr, U8*& outMaxAddr);

	private:
		U8* base = nullptr;

		U8* stackMinAddr;
		U8* stackMaxAddr;
		U8* stackMinGuardAddr;
	};

	extern thread_local SigAltStack sigAltStack;
	extern thread_local SignalContext* innermostSignalContext;

	extern bool initThreadAndGlobalSignalsOnce();
	extern bool initGlobalSignalsOnce();

	inline void initGlobalSignals()
	{
		static bool initedGlobalSignals = initGlobalSignalsOnce();
		WAVM_ASSERT(initedGlobalSignals);
	}
	inline void initThreadAndGlobalSignals()
	{
		static thread_local bool initedThread = initThreadAndGlobalSignalsOnce();
		WAVM_ASSERT(initedThread);
	}

	void dumpErrorCallStack(Uptr numOmittedFramesFromTop);
	void getCurrentThreadStack(U8*& outMinGuardAddr, U8*& outMinAddr, U8*& outMaxAddr);
}}
