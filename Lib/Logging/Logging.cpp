#include "WAVM/Logging/Logging.h"
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Platform/Alloca.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"

using namespace WAVM;
using namespace WAVM::Log;

static std::atomic<bool> categoryEnabled[(Uptr)Category::num] = {
	{true},  // error
	{false}, // debug
	{false}, // metrics
	{true},  // output
	{false}, // trace validation
	{false}, // trace compilation
	{false}, // trace unwind
	{false}, // trace object cache
	{false}, // trace linking
	{false}, // trace dwarf
};
static_assert(sizeof(categoryEnabled) / sizeof(categoryEnabled[0]) == (Uptr)Category::num,
			  "categoryEnabled array size must match Category::num");
static std::atomic<OutputFunction*> atomicOutputFunction{nullptr};

static VFS::VFD* getFileForCategory(Log::Category category)
{
	return category == Log::error ? Platform::getStdFD(Platform::StdDevice::err)
								  : Platform::getStdFD(Platform::StdDevice::out);
}

void Log::setCategoryEnabled(Category category, bool enable)
{
	WAVM_ASSERT(category < Category::num);
	categoryEnabled[(Uptr)category].store(enable);
}

bool Log::isCategoryEnabled(Category category)
{
	WAVM_ASSERT(category < Category::num);
	return categoryEnabled[(Uptr)category].load();
}

void Log::printf(Category category, const char* format, ...)
{
	if(categoryEnabled[(Uptr)category].load())
	{
		va_list argList;
		va_start(argList, format);
		vprintf(category, format, argList);
		va_end(argList);
	}
}

void Log::vprintf(Category category, const char* format, va_list argList)
{
	if(categoryEnabled[(Uptr)category].load())
	{
		static constexpr Uptr maxBufferBytes = 4096;
		char* buffer = (char*)alloca(maxBufferBytes);
		I32 numChars = vsnprintf(buffer, maxBufferBytes, format, argList);
		if(numChars < 0) { numChars = 0; }
		if(Uptr(numChars) >= maxBufferBytes) { numChars = I32(maxBufferBytes - 1); }

		// If an output function is set, call it with the message.
		OutputFunction* outputFunction = atomicOutputFunction.load(std::memory_order_acquire);
		if(outputFunction) { (*outputFunction)(category, buffer, Uptr(numChars)); }
		else
		{
			// Otherwise, write the message to the appropriate stdio device.
			// Ignore write failures (e.g. broken pipe).
			VFS::VFD* fd = getFileForCategory(category);
			Uptr numBytesWritten = 0;
			fd->write(buffer, numChars, &numBytesWritten);
		}
	}
}

void Log::setOutputFunction(OutputFunction* newOutputFunction)
{
	atomicOutputFunction.store(newOutputFunction, std::memory_order_release);
}
