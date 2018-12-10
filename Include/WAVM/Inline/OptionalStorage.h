#pragma once

#include <type_traits>

namespace WAVM {
	// A type that holds an optional instance of another type. The lifetime of the contained
	// instance is defined by the user of this type, and the user is responsible for calling
	// construct and destruct.
	template<typename Contents,
			 bool hasTrivialDestructor = std::is_trivially_destructible<Contents>::value>
	struct OptionalStorage
	{
		union
		{
			// Putting contents in a union allows this type to be default-constructible without
			// calling the Contents default constructor.
			Contents contents;
		};

		OptionalStorage() {}
		~OptionalStorage() {}

		template<typename... Args>
		OptionalStorage(Args&&... args) : contents(std::forward<Args>(args)...)
		{
		}

		template<typename... Args> void construct(Args&&... args)
		{
			new(&contents) Contents(std::forward<Args>(args)...);
		}

		void destruct() { contents.~Contents(); }
	};

	// Partial specialization for types with trivial destructors.
	template<typename Contents> struct OptionalStorage<Contents, true>
	{
		union
		{
			Contents contents;
		};
		OptionalStorage() {}
		~OptionalStorage() {}

		template<typename... Args>
		OptionalStorage(Args&&... args) : contents(std::forward<Args>(args)...)
		{
		}

		template<typename... Args> void construct(Args&&... args)
		{
			new(&contents) Contents(std::forward<Args>(args)...);
		}

		void destruct() {}
	};
}
