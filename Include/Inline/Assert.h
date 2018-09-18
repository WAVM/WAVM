#pragma once

#include "Platform/Defines.h"
#include "Platform/Diagnostics.h"

#include <cstdarg>

#if WAVM_DEBUG || ENABLE_RELEASE_ASSERTS
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
