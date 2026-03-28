#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Inline/Time.h"

namespace WAVM { namespace Platform {
	struct Mutex;

	// Platform-independent condition variables.
	struct ConditionVariable
	{
		WAVM_API ConditionVariable();
		WAVM_API ~ConditionVariable();

		// Don't allow copying or moving a ConditionVariable.
		ConditionVariable(const ConditionVariable&) = delete;
		ConditionVariable(ConditionVariable&&) = delete;
		void operator=(const ConditionVariable&) = delete;
		void operator=(ConditionVariable&&) = delete;

		// Atomically unlocks the mutex and waits for the condition variable to be signaled.
		// Returns true if the condition variable was signaled, false if the wait timed out.
		WAVM_API bool wait(Mutex& mutex, Time waitDuration);

		// Wakes one thread waiting on the condition variable.
		WAVM_API void signal();

		// Wakes all threads waiting on the condition variable.
		WAVM_API void broadcast();

	private:
		struct alignas(WAVM_PLATFORM_COND_ALIGN) CondData
		{
			U8 data[WAVM_PLATFORM_COND_SIZE];
		} condData;
	};
}}
