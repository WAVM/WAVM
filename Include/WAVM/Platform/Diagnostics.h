#pragma once

#include <cstdarg>
#include <string>
#include <vector>

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Platform {
	//
	// Error reporting
	//

	PACKED_STRUCT(struct AssertMetadata {
		const char* condition;
		const char* file;
		U32 line;
	});

	PLATFORM_API void handleAssertionFailure(const AssertMetadata& metadata);
	[[noreturn]] PLATFORM_API void handleFatalError(const char* messageFormat,
													bool printCallStack,
													va_list varArgs);

	//
	// Call stack and exceptions
	//

	// Describes a call stack.
	struct CallStack
	{
		struct Frame
		{
			Uptr ip;
		};
		std::vector<Frame> stackFrames;
	};

	// Captures the execution context of the caller.
	PLATFORM_API CallStack captureCallStack(Uptr numOmittedFramesFromTop = 0);

	// Describes an instruction pointer.
	PLATFORM_API bool describeInstructionPointer(Uptr ip, std::string& outDescription);
}}
