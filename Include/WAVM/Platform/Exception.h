#pragma once

#include <functional>

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Diagnostics.h"

namespace WAVM { namespace Platform {
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
		const std::function<bool(Signal signal, const CallStack&)>& filter);

	typedef bool (*SignalHandler)(Signal, const CallStack&);

	PLATFORM_API void setSignalHandler(SignalHandler handler);

	PLATFORM_API void registerEHFrames(const U8* imageBase, const U8* ehFrames, Uptr numBytes);
	PLATFORM_API void deregisterEHFrames(const U8* imageBase, const U8* ehFrames, Uptr numBytes);

	// Calls a thunk, catching any platform exceptions raised.
	// If a platform exception is caught, the exception is passed to the handler function, and true
	// is returned. If no exceptions are caught, false is returned.
	PLATFORM_API bool catchPlatformExceptions(
		const std::function<void()>& thunk,
		const std::function<void(void*, const CallStack&)>& handler);

	[[noreturn]] PLATFORM_API void raisePlatformException(void* data);

	enum
	{
		SEH_WAVM_EXCEPTION = 0xE0000001
	};
	PLATFORM_API std::type_info* getUserExceptionTypeInfo();
}}
