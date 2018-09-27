#pragma once

#include <setjmp.h>
#include <functional>

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Exception.h"

// This struct layout is replicated in POSIX.S
struct ExecutionContext
{
	U64 rbx;
	U64 rsp;
	U64 rbp;
	U64 r12;
	U64 r13;
	U64 r14;
	U64 r15;
	U64 rip;
};
static_assert(offsetof(ExecutionContext, rbx) == 0, "unexpected offset");
static_assert(offsetof(ExecutionContext, rsp) == 8, "unexpected offset");
static_assert(offsetof(ExecutionContext, rbp) == 16, "unexpected offset");
static_assert(offsetof(ExecutionContext, r12) == 24, "unexpected offset");
static_assert(offsetof(ExecutionContext, r13) == 32, "unexpected offset");
static_assert(offsetof(ExecutionContext, r14) == 40, "unexpected offset");
static_assert(offsetof(ExecutionContext, r15) == 48, "unexpected offset");
static_assert(offsetof(ExecutionContext, rip) == 56, "unexpected offset");
static_assert(sizeof(ExecutionContext) == 64, "unexpected size");

#ifdef __WAVIX__
extern "C" I64 saveExecutionState(ExecutionContext* outContext, I64 returnCode) noexcept(false)
{
	Errors::fatal("saveExecutionState is unimplemented on Wavix");
}

[[noreturn]] extern "C" void loadExecutionState(ExecutionContext* context, I64 returnCode)
{
	Errors::fatal("loadExecutionState is unimplemented on Wavix");
}

extern "C" I64 switchToForkedStackContext(ExecutionContext* forkedContext,
										  U8* trampolineFramePointer) noexcept(false)
{
	Errors::fatal("switchToForkedStackContext is unimplemented on Wavix");
}

extern "C" U8* getStackPointer() { Errors::fatal("getStackPointer is unimplemented on Wavix"); }

// libunwind dynamic frame registration
extern "C" void __register_frame(const void* fde)
{
	Errors::fatal("__register_frame is unimplemented on Wavix");
}

extern "C" void __deregister_frame(const void* fde)
{
	Errors::fatal("__deregister_frame is unimplemented on Wavix");
}

#else
// Defined in POSIX.S
extern "C" I64 saveExecutionState(ExecutionContext* outContext, I64 returnCode) noexcept(false);
[[noreturn]] extern void loadExecutionState(ExecutionContext* context, I64 returnCode);
extern "C" I64 switchToForkedStackContext(ExecutionContext* forkedContext,
										  U8* trampolineFramePointer) noexcept(false);
extern "C" U8* getStackPointer();

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
		std::function<bool(Platform::Signal, const Platform::CallStack&)> filter;
	};

	struct SigAltStack
	{
		~SigAltStack();

		void init();

	private:
		U8* base = nullptr;
	};

	extern thread_local SigAltStack sigAltStack;
	extern thread_local SignalContext* innermostSignalContext;

	void dumpErrorCallStack(Uptr numOmittedFramesFromTop);
	void getCurrentThreadStack(U8*& outMinAddr, U8*& outMaxAddr);
}}
