#pragma once

#include <cstring>
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM {

	// A fixed-capacity, null-terminated string that lives entirely in inline storage.
	// Signal-safe: no heap allocation. Truncates silently on overflow.
	template<Uptr N> struct FixedString
	{
		static_assert(N > 0, "FixedString capacity must be at least 1");

		char data[N];

		FixedString() { data[0] = '\0'; }

		// Assign from a null-terminated C string. Handles null pointers (becomes empty).
		FixedString& operator=(const char* str)
		{
			if(str)
			{
				Uptr len = strlen(str);
				if(len > N - 1) { len = N - 1; }
				memcpy(data, str, len);
				data[len] = '\0';
			}
			else
			{
				data[0] = '\0';
			}
			return *this;
		}

		// Assign from a pointer and known length. Truncates if len > N-1.
		void assign(const char* str, Uptr len)
		{
			if(len > N - 1) { len = N - 1; }
			memcpy(data, str, len);
			data[len] = '\0';
		}

		const char* c_str() const { return data; }
		operator const char*() const { return data; }

		Uptr size() const { return strlen(data); }
		bool empty() const { return data[0] == '\0'; }

		// Mutable access for OS APIs that write directly into the buffer
		// (e.g. GetModuleFileNameA). Caller is responsible for null-termination.
		char* mutableData() { return data; }
		static constexpr Uptr capacity() { return N; }
	};

}
