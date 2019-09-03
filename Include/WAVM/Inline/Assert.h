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

#define WAVM_UNREACHABLE()                                                                         \
	while(true) { WAVM_DEBUG_TRAP(); };
