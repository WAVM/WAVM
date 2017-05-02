#include "Logging.h"
#include "Platform/Platform.h"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>

namespace Log
{
	static Platform::Mutex* categoryEnabledMutex = Platform::createMutex();
	static bool categoryEnabled[(size_t)Category::num] =
	{
		true, // error
		#ifdef _DEBUG // debug
			true,
		#else
			false,
		#endif
		WAVM_METRICS_OUTPUT != 0 // metrics
	};
	void setCategoryEnabled(Category category,bool enable)
	{
		Platform::Lock lock(categoryEnabledMutex);
		assert(category < Category::num);
		categoryEnabled[(uintp)category] = enable;
	}
	bool isCategoryEnabled(Category category)
	{
		Platform::Lock lock(categoryEnabledMutex);
		assert(category < Category::num);
		return categoryEnabled[(uintp)category];
	}
	void printf(Category category,const char* format,...)
	{
		Platform::Lock lock(categoryEnabledMutex);
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
