#include "Logging/Logging.h"

#include <atomic>
#include <cstdio>

#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"

using namespace Log;

static std::atomic<bool> categoryEnabled[(Uptr)Category::num] = {
	{true}, // error
	{WAVM_DEBUG != 0},
	{WAVM_METRICS_OUTPUT != 0} // metrics
};

void Log::setCategoryEnabled(Category category, bool enable)
{
	wavmAssert(category < Category::num);
	categoryEnabled[(Uptr)category].store(enable);
}

bool Log::isCategoryEnabled(Category category)
{
	wavmAssert(category < Category::num);
	return categoryEnabled[(Uptr)category].load();
}

void Log::printf(Category category, const char* format, ...)
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

void Log::vprintf(Category category, const char* format, va_list argList)
{
	if(categoryEnabled[(Uptr)category].load())
	{
		vfprintf(stdout, format, argList);
		fflush(stdout);
	}
}
