#pragma once

#include <array>
#include <cstddef>
#include <new>
#include <type_traits>
#if __has_include(<version>)
#include <version>
#endif
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM {
	// A type that holds an optional instance of another type. The lifetime of the contained
	// instance is defined by the user of this type, and the user is responsible for calling
	// construct and destruct.
	template<typename Contents,
			 bool hasTrivialDestructor = std::is_trivially_destructible<Contents>::value>
	struct OptionalStorage
	{
		template<typename... Args> void construct(Args&&... args)
		{
			new(&contents) Contents(std::forward<Args>(args)...);
		}

		void destruct() { get().~Contents(); }

#ifdef __cpp_lib_launder
		Contents& get() { return *std::launder(reinterpret_cast<Contents*>(&contents)); }
		const Contents& get() const
		{
			return *std::launder(reinterpret_cast<const Contents*>(&contents));
		}
#else
		Contents& get() { return *reinterpret_cast<Contents*>(&contents); }
		const Contents& get() const { return *reinterpret_cast<const Contents*>(&contents); }
#endif

	private:
		alignas(alignof(Contents))::std::array<char unsigned, sizeof(Contents)> contents;
	};

	// Partial specialization for types with trivial destructors.
	template<typename Contents> struct OptionalStorage<Contents, true>
	{
		template<typename... Args> void construct(Args&&... args)
		{
			new(&contents) Contents(std::forward<Args>(args)...);
		}

		void destruct() {}

#ifdef __cpp_lib_launder
		Contents& get() { return *std::launder(reinterpret_cast<Contents*>(&contents)); }
		const Contents& get() const
		{
			return *std::launder(reinterpret_cast<const Contents*>(&contents));
		}
#else
		Contents& get() { return *reinterpret_cast<Contents*>(&contents); }
		const Contents& get() const { return *reinterpret_cast<const Contents*>(&contents); }
#endif

	private:
		alignas(alignof(Contents))::std::array<char unsigned, sizeof(Contents)> contents;
	};

	namespace OptionalStorageAssertions {
		struct NonTrivialType
		{
			NonTrivialType();
		};
		static_assert(std::is_trivial<OptionalStorage<NonTrivialType>>::value,
					  "OptionalStorage<NonTrivialType> is non-trivial");
	};
}
