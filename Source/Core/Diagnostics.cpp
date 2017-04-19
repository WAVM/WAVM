#include "Core.h"
#include "Platform.h"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>

namespace Core
{
	[[noreturn]] void errorf(const char* messageFormat,...)
	{
		va_list varArgs;
		va_start(varArgs,messageFormat);
		std::vfprintf(stderr,messageFormat,varArgs);
		std::fflush(stderr);
		va_end(varArgs);
		std::abort();
	}
}

namespace Log
{
	static bool categoryEnabled[(size_t)Category::num] =
	{
		true, // error
		_DEBUG != 0, // debug
		WAVM_METRICS_OUTPUT != 0 // metrics
	};
	void setCategoryEnabled(Category category,bool enable)
	{
		assert(category < Category::num);
		categoryEnabled[(uintp)category] = enable;
	}
	bool isCategoryEnabled(Category category)
	{
		assert(category < Category::num);
		return categoryEnabled[(uintp)category];
	}
	void printf(Category category,const char* format,...)
	{
		if(categoryEnabled[(uintp)category])
		{
			va_list varArgs;
			va_start(varArgs,format);
			vfprintf(stdout,format,varArgs);
			fflush(stdout);
			va_end(varArgs);
		}
	}
}
