#pragma once

#include "WAVM/Inline/Config.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Diagnostics.h"

#include <cstdarg>

#if WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
#define wavmAssert(condition)                                                                      \
	if(!(condition))                                                                               \
	{                                                                                              \
		for(static const WAVM::Platform::AssertMetadata metadata{#condition, __FILE__, __LINE__};  \
			;)                                                                                     \
		{                                                                                          \
			WAVM::Platform::handleAssertionFailure(metadata);                                      \
			DEBUG_TRAP();                                                                          \
			break;                                                                                 \
		}                                                                                          \
	}
#else
#define wavmAssert(condition)                                                                      \
	if(false && !(condition)) {}
#endif

#define errorUnless(condition)                                                                     \
	if(!(condition))                                                                               \
	{                                                                                              \
		for(static const WAVM::Platform::AssertMetadata metadata{#condition, __FILE__, __LINE__};  \
			;)                                                                                     \
		{                                                                                          \
			WAVM::Platform::handleAssertionFailure(metadata);                                      \
			DEBUG_TRAP();                                                                          \
			break;                                                                                 \
		}                                                                                          \
	}
