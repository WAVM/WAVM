#pragma once

#if WAVM_PLATFORM_POSIX

#include <setjmp.h>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Platform/Signal.h"
#include "WAVM/Platform/Unwind.h"

namespace WAVM { namespace Platform {

	struct SignalContext
	{
		SignalContext* outerContext;
		jmp_buf catchJump;
		bool (*filter)(void*, Signal, UnwindState&&);
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

	UnwindState makeUnwindStateFromSignalContext(void* contextPtr);

	void getCurrentThreadStack(U8*& outMinGuardAddr, U8*& outMinAddr, U8*& outMaxAddr);
}}

#endif // WAVM_PLATFORM_POSIX
