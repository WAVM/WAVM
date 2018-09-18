#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Platform {
	// Returns the current value of a clock that may be used as an absolute time for wait timeouts.
	// The resolution is microseconds, and the origin is arbitrary.
	PLATFORM_API U64 getMonotonicClock();

	// Platform-independent events.
	struct Event
	{
		PLATFORM_API Event();
		PLATFORM_API ~Event();

		// Don't allow copying or moving an Event.
		Event(const Event&) = delete;
		Event(Event&&) = delete;
		void operator=(const Event&) = delete;
		void operator=(Event&&) = delete;

		PLATFORM_API bool wait(U64 untilClock);
		PLATFORM_API void signal();

	private:
#ifdef WIN32
		void* handle;
#elif defined(__linux__)
		struct PthreadMutex
		{
			Uptr data[5];
		} pthreadMutex;
		struct PthreadCond
		{
			Uptr data[6];
		} pthreadCond;
#elif defined(__APPLE__)
		struct PthreadMutex
		{
			Uptr data[8];
		} pthreadMutex;
		struct PthreadCond
		{
			Uptr data[6];
		} pthreadCond;
#elif defined(__WAVIX__)
		struct PthreadMutex
		{
			Uptr data[6];
		} pthreadMutex;
		struct PthreadCond
		{
			Uptr data[12];
		} pthreadCond;
#else
#error unsupported platform
#endif
	};
}}
