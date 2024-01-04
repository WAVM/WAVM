#pragma once

#include <cstdarg>
#include "WAVM/Inline/Config.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Error.h"

#define WAVM_ENABLE_ASSERTS (WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS)

#if WAVM_ENABLE_ASSERTS
#define WAVM_ASSERT(condition)                                                                     \
	if(!(condition))                                                                               \
	{                                                                                              \
		for(static const WAVM::Platform::AssertMetadata wavmAssertMetadata{                        \
				#condition, __FILE__, __LINE__};                                                   \
			;)                                                                                     \
		{                                                                                          \
			WAVM::Platform::handleAssertionFailure(wavmAssertMetadata);                            \
			WAVM_DEBUG_TRAP();                                                                     \
			break;                                                                                 \
		}                                                                                          \
	}
#else
#define WAVM_ASSERT(condition)                                                                     \
	if(false && !(condition)) {}
#endif

#define WAVM_ERROR_UNLESS(condition)                                                               \
	if(!(condition))                                                                               \
	{                                                                                              \
		for(static const WAVM::Platform::AssertMetadata wavmAssertMetadata{                        \
				#condition, __FILE__, __LINE__};                                                   \
			;)                                                                                     \
		{                                                                                          \
			WAVM::Platform::handleAssertionFailure(wavmAssertMetadata);                            \
			WAVM_UNREACHABLE();                                                                    \
			break;                                                                                 \
		}                                                                                          \
	}

#if defined(_MSC_VER) && !defined(__clang__)
#define WAVM_UNREACHABLE()                                                                         \
	__debugbreak();                                                                                \
	__assume(0)
#else
#define WAVM_UNREACHABLE()                                                                         \
	WAVM_DEBUG_TRAP();                                                                             \
	__builtin_unreachable()
#endif
