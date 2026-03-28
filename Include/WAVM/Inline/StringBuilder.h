#pragma once

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Defines.h"

namespace WAVM {

	// Abstract base for incrementally building null-terminated strings.
	// Modeled after Serialization::OutputStream: subclasses implement extendBuffer()
	// to control what happens when the buffer is full.
	//
	// Signal-safe when used with FixedStringBuilder (no heap allocation).
	struct StringBuilderBase
	{
		virtual ~StringBuilderBase() {}

		StringBuilderBase& vappendf(const char* fmt, va_list args)
		{
			va_list argsCopy;
			va_copy(argsCopy, args);
			Uptr remaining = Uptr(end - next);
			int n = std::vsnprintf(next, remaining + 1, fmt, argsCopy);
			va_end(argsCopy);

			if(n >= 0 && Uptr(n) <= remaining) { next += n; }
			else if(n >= 0)
			{
				// Output was truncated. Try to extend the buffer.
				extendBuffer(Uptr(n) - remaining);
				remaining = Uptr(end - next);
				if(Uptr(n) <= remaining)
				{
					// Buffer grew enough; retry.
					va_copy(argsCopy, args);
					n = std::vsnprintf(next, remaining + 1, fmt, argsCopy);
					va_end(argsCopy);
					if(n > 0 && Uptr(n) <= remaining) { next += n; }
				}
				else
				{
					// Truncation accepted (e.g. fixed buffer in truncate mode).
					next = end;
				}
			}
			return *this;
		}

		WAVM_VALIDATE_AS_PRINTF(2, 3)
		StringBuilderBase& appendf(const char* fmt, ...)
		{
			va_list args;
			va_start(args, fmt);
			vappendf(fmt, args);
			va_end(args);
			return *this;
		}

		StringBuilderBase& append(const char* str, Uptr len)
		{
			Uptr remaining = Uptr(end - next);
			if(remaining < len)
			{
				extendBuffer(len - remaining);
				remaining = Uptr(end - next);
			}
			Uptr copyLen = len < remaining ? len : remaining;
			memcpy(next, str, copyLen);
			next += copyLen;
			*next = '\0';
			return *this;
		}

		StringBuilderBase& append(const char* str) { return append(str, strlen(str)); }
		StringBuilderBase& append(std::string_view sv) { return append(sv.data(), sv.size()); }
		StringBuilderBase& append(const std::string& s) { return append(s.data(), s.size()); }

		const char* c_str() const { return start ? start : ""; }
		Uptr size() const { return Uptr(next - start); }
		std::string_view view() const { return std::string_view(start, size()); }
		std::string toString() const { return std::string(start, size()); }
		operator std::string_view() const { return view(); }

	protected:
		char* start;
		char* next;
		char* end; // One past the last usable char position. Null terminator goes at *next.

		StringBuilderBase() : start(nullptr), next(nullptr), end(nullptr) {}
		StringBuilderBase(char* buf, Uptr capacity) : start(buf), next(buf), end(buf + capacity - 1)
		{
			if(capacity > 0) { buf[0] = '\0'; }
		}

		// Called when the buffer is full. Implementations may:
		// - Grow the buffer (updating start/next/end)
		// - Do nothing (output will be silently truncated)
		// - Abort with a fatal error
		virtual bool extendBuffer(Uptr numChars) = 0;
	};

	// Fixed-size string builder with inline storage. Aborts on overflow.
	// Use when the buffer size is calculated to be exactly large enough.
	// No heap allocation; signal-safe.
	template<Uptr N> struct FixedStringBuilder : StringBuilderBase
	{
		char storage[N];

		FixedStringBuilder() : StringBuilderBase(storage, N) {}

		FixedStringBuilder(const FixedStringBuilder&) = delete;
		FixedStringBuilder& operator=(const FixedStringBuilder&) = delete;

	protected:
		virtual bool extendBuffer(Uptr numChars) override
		{
			Errors::fatal("FixedStringBuilder buffer overflow");
		}
	};

	// Fixed-size string builder with inline storage. Truncates when it's full.
	// No heap allocation; signal-safe.
	template<Uptr N> struct TruncatingFixedStringBuilder : StringBuilderBase
	{
		char storage[N];

		TruncatingFixedStringBuilder() : StringBuilderBase(storage, N) {}

		TruncatingFixedStringBuilder(const TruncatingFixedStringBuilder&) = delete;
		TruncatingFixedStringBuilder& operator=(const TruncatingFixedStringBuilder&) = delete;

	protected:
		virtual bool extendBuffer(Uptr numChars) override { return false; }
	};

	// Growable string builder backed by heap allocation.
	// Not signal-safe. Use when output size is unpredictable.
	struct StringBuilder : StringBuilderBase
	{
		StringBuilder(Uptr initialCapacity = 64) : heapBuffer(nullptr)
		{
			heapBuffer = static_cast<char*>(malloc(initialCapacity));
			if(heapBuffer)
			{
				start = heapBuffer;
				next = heapBuffer;
				end = heapBuffer + initialCapacity - 1;
				heapBuffer[0] = '\0';
			}
		}

		~StringBuilder() { free(heapBuffer); }

		StringBuilder(StringBuilder&& other) noexcept : StringBuilderBase(), heapBuffer(nullptr)
		{
			start = other.start;
			next = other.next;
			end = other.end;
			heapBuffer = other.heapBuffer;
			other.start = nullptr;
			other.next = nullptr;
			other.end = nullptr;
			other.heapBuffer = nullptr;
		}

		StringBuilder& operator=(StringBuilder&& other) noexcept
		{
			if(this != &other)
			{
				free(heapBuffer);
				start = other.start;
				next = other.next;
				end = other.end;
				heapBuffer = other.heapBuffer;
				other.start = nullptr;
				other.next = nullptr;
				other.end = nullptr;
				other.heapBuffer = nullptr;
			}
			return *this;
		}

		StringBuilder(const StringBuilder&) = delete;
		StringBuilder& operator=(const StringBuilder&) = delete;

		// Move the contents to a std::string and reset the builder.
		std::string moveToString()
		{
			std::string result(start, size());
			next = start;
			if(start) { *start = '\0'; }
			return result;
		}

	protected:
		char* heapBuffer;

		virtual bool extendBuffer(Uptr numChars) override
		{
			Uptr currentSize = Uptr(next - start);
			Uptr currentCapacity = Uptr(end - start) + 1;
			Uptr needed = currentSize + numChars + 1; // +1 for null terminator
			// Grow by at least 7/5 (same growth factor as ArrayOutputStream).
			Uptr newCapacity = std::max(needed, currentCapacity * 7 / 5 + 32);
			char* newBuffer = static_cast<char*>(realloc(heapBuffer, newCapacity));
			WAVM_ERROR_UNLESS(newBuffer);
			heapBuffer = newBuffer;
			start = newBuffer;
			next = newBuffer + currentSize;
			end = newBuffer + newCapacity - 1;
			return true;
		}
	};

} // namespace WAVM
