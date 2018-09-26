#include <atomic>
#include <cstdio>

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Defines.h"

using namespace WAVM;
using namespace WAVM::Log;

static std::atomic<bool> categoryEnabled[(Uptr)Category::num] = {
	{true},                     // error
	{WAVM_DEBUG != 0},          // debug
	{WAVM_METRICS_OUTPUT != 0}, // metrics
	{true},                     // output
};

static FILE* getFileForCategory(Log::Category category)
{
	return category == Log::error ? stderr : stdout;
}

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
		FILE* file = getFileForCategory(category);
		vfprintf(file, format, argList);
		fflush(file);
		va_end(argList);
	}
}

void Log::vprintf(Category category, const char* format, va_list argList)
{
	if(categoryEnabled[(Uptr)category].load())
	{
		FILE* file = getFileForCategory(category);
		vfprintf(file, format, argList);
		fflush(file);
	}
}
