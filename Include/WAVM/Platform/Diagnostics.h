#pragma once

#include <string>
#include <vector>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/InlineArray.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM { namespace Platform {
	//
	// Call stack and exceptions
	//

	// Describes a call stack.
	struct CallStack
	{
		static constexpr Uptr maxFrames = 32;

		struct Frame
		{
			Uptr ip;
		};

		InlineArray<Frame, maxFrames> frames;

		~CallStack() {}
	};

	// Captures the execution context of the caller.
	WAVM_API CallStack captureCallStack(Uptr numOmittedFramesFromTop = 0);

	// Describes an instruction pointer.
	WAVM_API bool describeInstructionPointer(Uptr ip, std::string& outDescription);

#if WAVM_ENABLE_ASAN
	WAVM_API void expectLeakedObject(void* object);
#else
	inline void expectLeakedObject(void*) {}
#endif
}}
