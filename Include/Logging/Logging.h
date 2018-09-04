#pragma once

#ifndef LOGGING_API
#define LOGGING_API DLL_IMPORT
#endif

#include <stdarg.h>

#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"

// Debug logging.
namespace Log
{
	// Allow filtering the logging by category.
	enum Category
	{
		error,
		debug,
		metrics,
		num
	};
	LOGGING_API void setCategoryEnabled(Category category, bool enable);
	LOGGING_API bool isCategoryEnabled(Category category);

	// Print some categorized, formatted string, and flush the output. Newline is not included.
	LOGGING_API void printf(Category category, const char* format, ...) VALIDATE_AS_PRINTF(2, 3);
	LOGGING_API void vprintf(Category category, const char* format, va_list argList);
}