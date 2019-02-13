#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Platform {
	struct Thread;
	PLATFORM_API Thread* createThread(Uptr numStackBytes,
									  I64 (*threadEntry)(void*),
									  void* argument);
	PLATFORM_API void detachThread(Thread* thread);
	PLATFORM_API I64 joinThread(Thread* thread);

	RETURNS_TWICE PLATFORM_API Thread* forkCurrentThread();

	PLATFORM_API Uptr getNumberOfHardwareThreads();
}}
