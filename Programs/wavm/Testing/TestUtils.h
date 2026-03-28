#pragma once

#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/FloatComponents.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Inline/StringBuilder.h"
#include "WAVM/Logging/Logging.h"

namespace WAVM { namespace Testing {

	// Holds test state including failure count
	struct TestState
	{
		int numFailures = 0;

		bool failed() const { return numFailures > 0; }
		int exitCode() const { return numFailures > 0 ? 1 : 0; }
	};

	// Default testToString for arithmetic types
	template<typename T>
	inline std::enable_if_t<std::is_integral_v<T>, std::string> testToString(T value)
	{
		return std::to_string(value);
	}

	inline std::string testToString(const std::string& value) { return value; }

	inline std::string testToString(bool value) { return value ? "true" : "false"; }

	inline std::string testToString(F32 value)
	{
		FloatComponents<F32> c;
		c.value = value;
		TruncatingFixedStringBuilder<128> sb;
		sb.appendf("%g (sign=%u exp=0x%02x sig=0x%06x)",
				   (double)value,
				   (unsigned)c.bits.sign,
				   (unsigned)c.bits.exponent,
				   (unsigned)c.bits.significand);
		return sb.toString();
	}

	inline std::string testToString(F64 value)
	{
		FloatComponents<F64> c;
		c.value = value;
		TruncatingFixedStringBuilder<128> sb;
		sb.appendf("%g (sign=%u exp=0x%03x sig=0x%013" PRIx64 ")",
				   value,
				   (unsigned)c.bits.sign,
				   (unsigned)c.bits.exponent,
				   (U64)c.bits.significand);
		return sb.toString();
	}

	inline std::string testToString(I128 value)
	{
		// Build decimal representation
		char decBuf[48]; // 39 digits + sign + null
		ToCharsResult result = toChars(decBuf, decBuf + sizeof(decBuf) - 1, value);
		*result.writeEnd = '\0';

		// Get the two 64-bit words
		U64 low = U64(value & I128(U64(0xFFFFFFFFFFFFFFFF)));
		I128 high128 = value >> I128(64);
		U64 high = U64(high128 & I128(U64(0xFFFFFFFFFFFFFFFF)));

		// Return "decimal (high:low)"
		// 41 characters: " (0x" + 16 hex + ":0x" + 16 hex + ")\0"
		FixedStringBuilder<48> sb;
		sb.appendf(" (0x%" PRIx64 ":0x%" PRIx64 ")", high, low);

		return std::string(decBuf) + sb.c_str();
	}

	// testToString for pointers (non-null vs null)
	template<typename T>
	inline std::enable_if_t<std::is_pointer_v<T>, std::string> testToString(T value)
	{
		if(value == nullptr) { return "(null)"; }
		else
		{
			return "(non-null)";
		}
	}

	// Generic check function: calls pred(args...) and reports failure
	// Prints: file(line): FAIL: expr\n followed by format with testToString(args)...
	template<typename Predicate, typename... Args>
	bool check(TestState& state,
			   Predicate pred,
			   const char* file,
			   int line,
			   const char* expr,
			   const char* format,
			   Args&&... args)
	{
		if(!pred(args...))
		{
			Log::printf(Log::error, "%s(%d): FAIL: %s\n", file, line, expr);
			Log::printf(Log::error, format, testToString(args).c_str()...);
			++state.numFailures;
			return false;
		}
		return true;
	}

	// Infrastructure for tracking object lifetimes and copy/move operations

	struct TrackedValueRealm;

	struct HeapValue : std::enable_shared_from_this<HeapValue>
	{
		enum class Provenance
		{
			DefaultConstructed,
			CopyConstructed,
			MoveConstructed,
			CopyAssigned,
			MoveAssigned
		};

		Uptr value;
		Provenance provenance;
		std::shared_ptr<HeapValue> source;
		TrackedValueRealm& realm;
		bool destroyed = false;

	private:
		HeapValue(TrackedValueRealm& r, Uptr v, Provenance p, std::shared_ptr<HeapValue> src)
		: value(v), provenance(p), source(std::move(src)), realm(r)
		{
		}

	public:
		static std::shared_ptr<HeapValue> create(TrackedValueRealm& realm,
												 Uptr value,
												 Provenance p,
												 std::shared_ptr<HeapValue> src = nullptr);

		// Follow the source chain through moves to find the underlying HeapValue
		std::shared_ptr<HeapValue> elideMoves()
		{
			std::shared_ptr<HeapValue> current = shared_from_this();
			while(current && current->provenance == Provenance::MoveConstructed)
			{
				current = current->source;
			}
			return current;
		}
	};

	struct TrackedValueRealm
	{
		std::set<std::weak_ptr<HeapValue>, std::owner_less<std::weak_ptr<HeapValue>>> liveObjects;

		~TrackedValueRealm() { WAVM_ASSERT(allDestroyed()); }

		bool allDestroyed() const
		{
			for(const auto& wp : liveObjects)
			{
				if(!wp.expired()) { return false; }
			}
			return true;
		}
	};

	inline std::shared_ptr<HeapValue> HeapValue::create(TrackedValueRealm& realm,
														Uptr value,
														Provenance p,
														std::shared_ptr<HeapValue> src)
	{
		auto ptr = std::shared_ptr<HeapValue>(new HeapValue(realm, value, p, std::move(src)));
		realm.liveObjects.insert(ptr);
		return ptr;
	}

	struct TrackedValue
	{
		std::shared_ptr<HeapValue> heapValue;

		TrackedValue(TrackedValueRealm& realm, Uptr value)
		: heapValue(HeapValue::create(realm, value, HeapValue::Provenance::DefaultConstructed))
		{
		}

		TrackedValue(const TrackedValue& other)
		: heapValue(HeapValue::create(other.heapValue->realm,
									  other.heapValue->value,
									  HeapValue::Provenance::CopyConstructed,
									  other.heapValue))
		{
		}

		TrackedValue(TrackedValue&& other) noexcept
		: heapValue(HeapValue::create(other.heapValue->realm,
									  other.heapValue->value,
									  HeapValue::Provenance::MoveConstructed,
									  other.heapValue))
		{
		}

		~TrackedValue()
		{
			WAVM_ASSERT(!heapValue->destroyed);
			heapValue->destroyed = true;
		}

		TrackedValue& operator=(const TrackedValue& other)
		{
			heapValue = HeapValue::create(heapValue->realm,
										  other.heapValue->value,
										  HeapValue::Provenance::CopyAssigned,
										  other.heapValue);
			return *this;
		}

		TrackedValue& operator=(TrackedValue&& other) noexcept
		{
			heapValue = HeapValue::create(heapValue->realm,
										  other.heapValue->value,
										  HeapValue::Provenance::MoveAssigned,
										  other.heapValue);
			return *this;
		}

		bool operator==(const TrackedValue& other) const
		{
			return heapValue->value == other.heapValue->value;
		}
	};

	inline std::string testToString(HeapValue::Provenance p)
	{
		switch(p)
		{
		case HeapValue::Provenance::DefaultConstructed: return "DefaultConstructed";
		case HeapValue::Provenance::CopyConstructed: return "CopyConstructed";
		case HeapValue::Provenance::MoveConstructed: return "MoveConstructed";
		case HeapValue::Provenance::CopyAssigned: return "CopyAssigned";
		case HeapValue::Provenance::MoveAssigned: return "MoveAssigned";
		default: return "Unknown";
		}
	}

}}

// Hash specialization for TrackedValue (must be in WAVM namespace)
namespace WAVM {
	template<> struct Hash<Testing::TrackedValue>
	{
		Uptr operator()(const Testing::TrackedValue& v, Uptr seed = 0) const
		{
			return Hash<Uptr>()(v.heapValue->value, seed);
		}
	};
}

// Macros for TestState declaration and passing
#define TEST_STATE_PARAM WAVM::Testing::TestState& testState
#define TEST_STATE_ARG testState
#define TEST_STATE_LOCAL WAVM::Testing::TestState testState

// Macros that capture expression text and source location
// These require a TestState variable named 'testState' in scope

#define CHECK_EQ(actual, expected)                                                                 \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& a, auto&& b) { return a == b; },                                                 \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_EQ(" #actual ", " #expected ")",                                                    \
		"  " #actual ": %s\n  " #expected ": %s\n",                                                \
		actual,                                                                                    \
		expected)

#define CHECK_NE(a, b)                                                                             \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& x, auto&& y) { return x != y; },                                                 \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_NE(" #a ", " #b ")",                                                                \
		"  " #a ": %s\n  " #b ": %s\n",                                                            \
		a,                                                                                         \
		b)

#define CHECK_LT(a, b)                                                                             \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& x, auto&& y) { return x < y; },                                                  \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_LT(" #a ", " #b ")",                                                                \
		"  " #a ": %s\n  " #b ": %s\n",                                                            \
		a,                                                                                         \
		b)

#define CHECK_LE(a, b)                                                                             \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& x, auto&& y) { return x <= y; },                                                 \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_LE(" #a ", " #b ")",                                                                \
		"  " #a ": %s\n  " #b ": %s\n",                                                            \
		a,                                                                                         \
		b)

#define CHECK_GT(a, b)                                                                             \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& x, auto&& y) { return x > y; },                                                  \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_GT(" #a ", " #b ")",                                                                \
		"  " #a ": %s\n  " #b ": %s\n",                                                            \
		a,                                                                                         \
		b)

#define CHECK_GE(a, b)                                                                             \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& x, auto&& y) { return x >= y; },                                                 \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_GE(" #a ", " #b ")",                                                                \
		"  " #a ": %s\n  " #b ": %s\n",                                                            \
		a,                                                                                         \
		b)

#define CHECK_NOT_LT(a, b)                                                                         \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& x, auto&& y) { return !(x < y); },                                               \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_NOT_LT(" #a ", " #b ")",                                                            \
		"  " #a ": %s\n  " #b ": %s\n",                                                            \
		a,                                                                                         \
		b)

#define CHECK_NOT_LE(a, b)                                                                         \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& x, auto&& y) { return !(x <= y); },                                              \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_NOT_LE(" #a ", " #b ")",                                                            \
		"  " #a ": %s\n  " #b ": %s\n",                                                            \
		a,                                                                                         \
		b)

#define CHECK_NOT_GT(a, b)                                                                         \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& x, auto&& y) { return !(x > y); },                                               \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_NOT_GT(" #a ", " #b ")",                                                            \
		"  " #a ": %s\n  " #b ": %s\n",                                                            \
		a,                                                                                         \
		b)

#define CHECK_NOT_GE(a, b)                                                                         \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto&& x, auto&& y) { return !(x >= y); },                                              \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_NOT_GE(" #a ", " #b ")",                                                            \
		"  " #a ": %s\n  " #b ": %s\n",                                                            \
		a,                                                                                         \
		b)

#define CHECK_TRUE(expr)                                                                           \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](bool v) { return v; },                                                                  \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_TRUE(" #expr ")",                                                                   \
		"  " #expr ": %s\n",                                                                       \
		static_cast<bool>(expr))

#define CHECK_FALSE(expr)                                                                          \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](bool v) { return !v; },                                                                 \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_FALSE(" #expr ")",                                                                  \
		"  " #expr ": %s\n",                                                                       \
		static_cast<bool>(expr))

#define CHECK_NULL(expr)                                                                           \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto* p) { return p == nullptr; },                                                      \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_NULL(" #expr ")",                                                                   \
		"  " #expr ": %s\n",                                                                       \
		expr)

#define CHECK_NOT_NULL(expr)                                                                       \
	WAVM::Testing::check(                                                                          \
		testState,                                                                                 \
		[](auto* p) { return p != nullptr; },                                                      \
		__FILE__,                                                                                  \
		__LINE__,                                                                                  \
		"CHECK_NOT_NULL(" #expr ")",                                                               \
		"  " #expr ": %s\n",                                                                       \
		expr)
