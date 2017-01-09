#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <algorithm>
#include <string>
#include <cstring>
#include <string.h>
#include <assert.h>

typedef uint8_t uint8;
typedef int8_t int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint64_t uint64;
typedef int64_t int64;
typedef float float32;
typedef double float64;

// The OSX libc defines uintptr_t to be a long where uint32/uint64 are int. This causes uintptr_t/uint64 to be treated as distinct types for e.g. overloading.
// Work around it by defining our own uintp/intp that are always int type.
template<size_t pointerSize>
struct PointerIntHelper;
template<> struct PointerIntHelper<4> { typedef int32 IntType; typedef uint32 UnsignedIntType; };
template<> struct PointerIntHelper<8> { typedef int64 IntType; typedef uint64 UnsignedIntType; };
typedef PointerIntHelper<sizeof(size_t)>::UnsignedIntType uintp;
typedef PointerIntHelper<sizeof(size_t)>::IntType intp;

#ifndef CORE_API
	#define CORE_API DLL_IMPORT
#endif

#ifndef _DEBUG
	#define _DEBUG 0
#endif

#include "Platform.h"

namespace Core
{
	// Encapsulates a timer that starts when constructed and stops when read.
	struct Timer
	{
		Timer(): startTime(std::chrono::high_resolution_clock::now()), isStopped(false) {}
		void stop() { endTime = std::chrono::high_resolution_clock::now(); }
		uint64 getMicroseconds()
		{
			if(!isStopped) { stop(); }
			return std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
		}
		float64 getMilliseconds() { return getMicroseconds() / 1000.0; }
		float64 getSeconds() { return getMicroseconds() / 1000000.0; }
	private:
		std::chrono::high_resolution_clock::time_point startTime;
		std::chrono::high_resolution_clock::time_point endTime;
		bool isStopped;
	};
	
	// A location in a text file.
	struct TextFileLocus
	{
		uint32 newlines;
		uint8 tabs;
		uint8 characters;

		TextFileLocus(): newlines(0), tabs(0), characters(0) {}

		uint32 lineNumber() const { return newlines + 1; }
		uint32 column(uint32 spacesPerTab = 4) const { return tabs * spacesPerTab + characters + 1; }

		std::string describe(uint32 spacesPerTab = 4) const
		{
			return std::to_string(lineNumber()) + ":" + std::to_string(column(spacesPerTab));
		}
	};

	// Fatal error handling.
	[[noreturn]] CORE_API void errorf(const char* messageFormat,...);
	[[noreturn]] inline void error(const char* message) { errorf("%s",message); }
	[[noreturn]] inline void unreachable() { errorf("reached unreachable code"); }
}

// Like assert, but is never removed in any build configuration.
#define errorUnless(condition) if(!(condition)) { Core::errorf("errorUnless(%s) failed",#condition); }

// Debug logging.
namespace Log
{
	// Allow filtering the logging by category.
	enum class Category
	{
		error,
		debug,
		metrics,
		num
	};
	CORE_API void setCategoryEnabled(Category category,bool enable);
	CORE_API bool isCategoryEnabled(Category category);

	// Print some categorized, formatted string, and flush the output. Newline is not included.
	CORE_API void printf(Category category,const char* format,...);
	
	// Helpers for printing timers.
	inline void logTimer(const char* context,Core::Timer& timer) { printf(Category::metrics,"%s in %.2fms\n",context,timer.getMilliseconds()); }
	inline void logRatePerSecond(const char* context,Core::Timer& timer,float64 numerator,const char* numeratorUnit)
	{
		printf(Category::metrics,"%s in %.2fms (%f %s/s)\n",
			context,
			timer.getMilliseconds(),
			numerator / timer.getSeconds(),
			numeratorUnit
			);
	}
};