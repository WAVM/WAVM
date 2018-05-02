#include "Inline/Assert.h"
#include "Logging.h"
#include "Platform/Platform.h"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>

namespace Log
{
	static Platform::Mutex categoryEnabledMutex;
	static bool categoryEnabled[(Uptr)Category::num] =
	{
		true, // error
		WAVM_DEBUG,
		WAVM_METRICS_OUTPUT != 0 // metrics
	};
	void setCategoryEnabled(Category category,bool enable)
	{
		Platform::Lock lock(categoryEnabledMutex);
		wavmAssert(category < Category::num);
		categoryEnabled[(Uptr)category] = enable;
	}
	bool isCategoryEnabled(Category category)
	{
		Platform::Lock lock(categoryEnabledMutex);
		wavmAssert(category < Category::num);
		return categoryEnabled[(Uptr)category];
	}
	void printf(Category category,const char* format,...)
	{
		Platform::Lock lock(categoryEnabledMutex);
		if(categoryEnabled[(Uptr)category])
		{
			va_list argList;
			va_start(argList,format);
			vfprintf(stdout,format,argList);
			fflush(stdout);
			va_end(argList);
		}
	}

	void vprintf(Category category,const char* format,va_list argList)
	{
		Platform::Lock lock(categoryEnabledMutex);
		if(categoryEnabled[(Uptr)category])
		{
			vfprintf(stdout,format,argList);
			fflush(stdout);
		}
	}
}
