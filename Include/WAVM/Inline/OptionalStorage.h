#pragma once

#include <type_traits>
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

		Contents& get() { return *(Contents*)&contents; }
		const Contents& get() const { return *(const Contents*)&contents; }

	private:
		alignas(Contents) U8 contents[sizeof(Contents)];
	};

	// Partial specialization for types with trivial destructors.
	template<typename Contents> struct OptionalStorage<Contents, true>
	{
		template<typename... Args> void construct(Args&&... args)
		{
			new(&contents) Contents(std::forward<Args>(args)...);
		}

		void destruct() {}

		Contents& get() { return *(Contents*)&contents; }
		const Contents& get() const { return *(const Contents*)&contents; }

	private:
		alignas(Contents) U8 contents[sizeof(Contents)];
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
