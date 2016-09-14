#include "Core.h"
#include "Platform.h"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>

namespace Core
{
	[[noreturn]] void fatalError(const char* message)
	{
		std::printf("fatal error: %s\n",message);
		std::fflush(stdout);
		std::abort();
	}

	[[noreturn]] void unreachable()
	{
		fatalError("reached unreachable code");
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
		categoryEnabled[(uintptr)category] = enable;
	}
	bool isCategoryEnabled(Category category)
	{
		assert(category < Category::num);
		return categoryEnabled[(uintptr)category];
	}
	void printf(Category category,const char* format,...)
	{
		if(categoryEnabled[(uintptr)category])
		{
			va_list varArgs;
			va_start(varArgs,format);
			vfprintf(stdout,format,varArgs);
			fflush(stdout);
		}
	}
}
