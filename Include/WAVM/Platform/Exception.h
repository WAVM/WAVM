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
			intDivideByZeroOrOverflow
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

	PLATFORM_API bool catchSignals(const std::function<void()>& thunk,
								   const std::function<bool(Signal signal, CallStack&&)>& filter);

	typedef void (*SignalHandler)(Signal, CallStack&&);

	PLATFORM_API void setSignalHandler(SignalHandler handler);

	PLATFORM_API void registerEHFrames(const U8* imageBase, const U8* ehFrames, Uptr numBytes);
	PLATFORM_API void deregisterEHFrames(const U8* imageBase, const U8* ehFrames, Uptr numBytes);
}}
