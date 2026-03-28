#pragma once

#include <string>
#include <type_traits>
#include <utility>

#if defined(_M_X64) || defined(__x86_64__)
#define WAVM_CPU_ARCH_X86 1
#define WAVM_CPU_ARCH_ARM 0
#elif defined(_M_IX86) || defined(__i386__)
#define WAVM_CPU_ARCH_X86 1
#define WAVM_CPU_ARCH_ARM 0
#elif defined(_M_ARM64) || defined(__aarch64__)
#define WAVM_CPU_ARCH_X86 0
#define WAVM_CPU_ARCH_ARM 1
#endif

// X86 CPU feature list. V(fieldName, cliName)
// cliName is used for CLI parsing and LLVM target attribute names.
#define WAVM_ENUM_X86_CPU_FEATURES(V)                                                              \
	V(sse, "sse")                                                                                  \
	V(sse2, "sse2")                                                                                \
	V(sse3, "sse3")                                                                                \
	V(ssse3, "ssse3")                                                                              \
	V(sse4_1, "sse4.1")                                                                            \
	V(sse4_2, "sse4.2")                                                                            \
	V(avx, "avx")                                                                                  \
	V(avx2, "avx2")                                                                                \
	V(popcnt, "popcnt")                                                                            \
	V(lzcnt, "lzcnt")                                                                              \
	V(bmi1, "bmi")                                                                                 \
	V(bmi2, "bmi2")                                                                                \
	V(fma, "fma")                                                                                  \
	V(avx512f, "avx512f")                                                                          \
	V(avx512bw, "avx512bw")                                                                        \
	V(avx512dq, "avx512dq")                                                                        \
	V(avx512vl, "avx512vl")

// AArch64 CPU feature list. V(fieldName, cliName)
// NEON is baseline on AArch64, so not included.
#define WAVM_ENUM_AARCH64_CPU_FEATURES(V) V(lse, "lse")

#define WAVM_CPU_FEATURE_FIELD_(fieldName, cliName) T fieldName;
#define WAVM_CPU_FEATURE_MAP_(fieldName, cliName) result.fieldName = f(fieldName);
#define WAVM_CPU_FEATURE_FOREACH_(fieldName, cliName) f(cliName, fieldName);

#define WAVM_DECLARE_CPU_FEATURES(Name, FEATURES)                                                  \
	template<typename T> struct Name                                                               \
	{                                                                                              \
		FEATURES(WAVM_CPU_FEATURE_FIELD_)                                                          \
                                                                                                   \
		template<typename F> auto mapFeatures(F&& f) const                                         \
		{                                                                                          \
			using U = decltype(f(std::declval<T>()));                                              \
			Name<U> result;                                                                        \
			FEATURES(WAVM_CPU_FEATURE_MAP_)                                                        \
			return result;                                                                         \
		}                                                                                          \
                                                                                                   \
		template<typename F> void forEachFeature(F&& f) { FEATURES(WAVM_CPU_FEATURE_FOREACH_) }    \
                                                                                                   \
		template<typename F> void forEachFeature(F&& f) const                                      \
		{                                                                                          \
			FEATURES(WAVM_CPU_FEATURE_FOREACH_)                                                    \
		}                                                                                          \
                                                                                                   \
		friend std::string asString(const Name<T>& features)                                       \
		{                                                                                          \
			std::string result;                                                                    \
			const char* sep = "";                                                                  \
			features.forEachFeature([&](const char* name, const T& value) {                        \
				if constexpr(std::is_same_v<T, bool>)                                              \
				{                                                                                  \
					result += sep;                                                                 \
					result += (value ? '+' : '-');                                                 \
					result += name;                                                                \
					sep = " ";                                                                     \
				}                                                                                  \
				else                                                                               \
				{                                                                                  \
					if(value.has_value())                                                          \
					{                                                                              \
						result += sep;                                                             \
						result += (*value ? '+' : '-');                                            \
						result += name;                                                            \
						sep = " ";                                                                 \
					}                                                                              \
				}                                                                                  \
			});                                                                                    \
			return result;                                                                         \
		}                                                                                          \
	};

#define WAVM_CPU_REGISTER_ENUM_(enumName, displayName) enumName,

// clang-format off
// X86 CPU register list. V(enumName, displayName)
#define WAVM_ENUM_X86_CPU_REGISTERS(V)                                                             \
	V(RAX, "RAX") V(RBX, "RBX") V(RCX, "RCX") V(RDX, "RDX")                                     \
	V(RSI, "RSI") V(RDI, "RDI") V(RBP, "RBP") V(RSP, "RSP")                                     \
	V(R8, "R8") V(R9, "R9") V(R10, "R10") V(R11, "R11")                                          \
	V(R12, "R12") V(R13, "R13") V(R14, "R14") V(R15, "R15")                                      \
	V(RIP, "RIP")

// AArch64 CPU register list. V(enumName, displayName)
#define WAVM_ENUM_AARCH64_CPU_REGISTERS(V)                                                         \
	V(X0, "X0") V(X1, "X1") V(X2, "X2") V(X3, "X3")                                              \
	V(X4, "X4") V(X5, "X5") V(X6, "X6") V(X7, "X7")                                              \
	V(X8, "X8") V(X9, "X9") V(X10, "X10") V(X11, "X11")                                          \
	V(X12, "X12") V(X13, "X13") V(X14, "X14") V(X15, "X15")                                      \
	V(X16, "X16") V(X17, "X17") V(X18, "X18") V(X19, "X19")                                      \
	V(X20, "X20") V(X21, "X21") V(X22, "X22") V(X23, "X23")                                      \
	V(X24, "X24") V(X25, "X25") V(X26, "X26") V(X27, "X27")                                      \
	V(X28, "X28") V(FP, "FP") V(LR, "LR") V(SP, "SP") V(PC, "PC")
// clang-format on

#if WAVM_CPU_ARCH_X86
#define WAVM_ENUM_HOST_CPU_REGISTERS WAVM_ENUM_X86_CPU_REGISTERS
#elif WAVM_CPU_ARCH_ARM
#define WAVM_ENUM_HOST_CPU_REGISTERS WAVM_ENUM_AARCH64_CPU_REGISTERS
#endif

namespace WAVM { namespace Platform {

	WAVM_DECLARE_CPU_FEATURES(X86CPUFeatures, WAVM_ENUM_X86_CPU_FEATURES)
	WAVM_DECLARE_CPU_FEATURES(AArch64CPUFeatures, WAVM_ENUM_AARCH64_CPU_FEATURES)

	// clang-format off
	enum class X86CPURegister
	{
		WAVM_ENUM_X86_CPU_REGISTERS(WAVM_CPU_REGISTER_ENUM_)
		IP = RIP, // Portable alias
		SP = RSP, // Portable alias
	};

	enum class AArch64CPURegister
	{
		WAVM_ENUM_AARCH64_CPU_REGISTERS(WAVM_CPU_REGISTER_ENUM_)
		IP = PC, // Portable alias
	};
	// clang-format on

#if WAVM_CPU_ARCH_X86
	using HostCPURegister = X86CPURegister;
	using HostCPUFeatures = X86CPUFeatures<bool>;
#elif WAVM_CPU_ARCH_ARM
	using HostCPURegister = AArch64CPURegister;
	using HostCPUFeatures = AArch64CPUFeatures<bool>;
#endif

	WAVM_API HostCPUFeatures getHostCPUFeatures();
}}

#undef WAVM_DECLARE_CPU_FEATURES
#undef WAVM_CPU_FEATURE_FIELD_
#undef WAVM_CPU_FEATURE_MAP_
#undef WAVM_CPU_FEATURE_FOREACH_
#undef WAVM_CPU_REGISTER_ENUM_