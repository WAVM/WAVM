#pragma once

#include <intrin.h>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Time.h"
#include "WAVM/Platform/Diagnostics.h"

#define NOMINMAX
#include <Windows.h>

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

static_assert(offsetof(ExecutionContext, rip) == 0, "unexpected offset");
static_assert(offsetof(ExecutionContext, cs) == 8, "unexpected offset");
static_assert(offsetof(ExecutionContext, rflags) == 16, "unexpected offset");
static_assert(offsetof(ExecutionContext, rsp) == 24, "unexpected offset");
static_assert(offsetof(ExecutionContext, ss) == 32, "unexpected offset");
static_assert(offsetof(ExecutionContext, r12) == 40, "unexpected offset");
static_assert(offsetof(ExecutionContext, rbp) == 96, "unexpected offset");
static_assert(offsetof(ExecutionContext, xmm6) == 112, "unexpected offset");
static_assert(offsetof(ExecutionContext, xmm15) == 256, "unexpected offset");
static_assert(sizeof(ExecutionContext) == 272, "unexpected size");

namespace WAVM { namespace Platform {
	void initThread();

	CallStack unwindStack(const CONTEXT& immutableContext, Uptr numOmittedFramesFromTop);

	Time fileTimeToWAVMRealTime(FILETIME fileTime);
	FILETIME wavmRealTimeToFileTime(Time realTime);
}}
