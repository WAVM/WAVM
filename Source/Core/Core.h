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

// The OSX libc defines uintptr_t to be a long where uint32/uint64 are int. This causes uintptr/uint64 to be treated as distinct types for e.g. overloading.
// Work around it by defining our own uintptr/intptr that are always int type.
template<size_t pointerSize>
struct PointerIntHelper;
template<> struct PointerIntHelper<4> { typedef int32 IntType; typedef uint32 UnsignedIntType; };
template<> struct PointerIntHelper<8> { typedef int64 IntType; typedef uint64 UnsignedIntType; };
typedef PointerIntHelper<sizeof(size_t)>::UnsignedIntType uintptr;
typedef PointerIntHelper<sizeof(size_t)>::IntType intptr;

#ifndef CORE_API
	#define CORE_API DLL_EXPORT
#endif

namespace Core
{
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

		std::string describe(uint32 spacesPerTab = 4) const
		{
			return "(" + std::to_string(newlines + 1) + ":" + std::to_string(tabs * spacesPerTab + characters + 1) + ")";
		}
	};
	
	struct StringCompareFunctor { bool operator()(const char* left,const char* right) const
	{
		while(true)
		{
			if(*left == *right)
			{
				if(!*left) { return false; }
			}
			else if(*left < *right)
			{
				return true;
			}
			else
			{
				return false;
			}
			++left;
			++right;
		};
	}};
}
