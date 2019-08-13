#pragma once

#include "WAVM/Inline/Config.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Diagnostics.h"

#include <cstdarg>

#if WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
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
			WAVM_DEBUG_TRAP();                                                                     \
			break;                                                                                 \
		}                                                                                          \
	}

#define WAVM_UNREACHABLE()                                                                         \
	while(true) { WAVM_DEBUG_TRAP(); };
