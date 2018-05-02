#pragma once

#include "Platform/Platform.h"

#include <cstdarg>

#if WAVM_DEBUG
	#define wavmAssert(condition) \
		if(!(condition)) \
		{ \
			for(static const Platform::AssertMetadata metadata {#condition, __FILE__, __LINE__};;) \
			{ \
				Platform::handleAssertionFailure(metadata); \
				DEBUG_TRAP(); \
				break; \
			} \
		}
#else
	#define wavmAssert(condition) \
		if(false && !(condition)) {}
#endif

#define errorUnless(condition) \
	if(!(condition)) \
	{ \
		for(static const Platform::AssertMetadata metadata {#condition, __FILE__, __LINE__};;) \
		{ \
			Platform::handleAssertionFailure(metadata); \
			DEBUG_TRAP(); \
			break; \
		} \
	}
