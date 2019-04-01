#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Platform {
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
