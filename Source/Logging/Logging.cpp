#include "Logging.h"
#include "Inline/Assert.h"
#include "Platform/Platform.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace Log
{
	static std::atomic<bool> categoryEnabled[(Uptr)Category::num] = {
		{true}, // error
		{WAVM_DEBUG != 0},
		{WAVM_METRICS_OUTPUT != 0} // metrics
	};
	void setCategoryEnabled(Category category, bool enable)
	{
		wavmAssert(category < Category::num);
		categoryEnabled[(Uptr)category].store(enable);
	}
	bool isCategoryEnabled(Category category)
	{
		wavmAssert(category < Category::num);
		return categoryEnabled[(Uptr)category].load();
	}
	void printf(Category category, const char* format, ...)
	{
		if(categoryEnabled[(Uptr)category].load())
		{
			va_list argList;
			va_start(argList, format);
			vfprintf(stdout, format, argList);
			fflush(stdout);
			va_end(argList);
		}
	}

	void vprintf(Category category, const char* format, va_list argList)
	{
		if(categoryEnabled[(Uptr)category].load())
		{
			vfprintf(stdout, format, argList);
			fflush(stdout);
		}
	}
}
