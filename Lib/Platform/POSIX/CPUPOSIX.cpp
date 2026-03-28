#if WAVM_PLATFORM_POSIX

#include "WAVM/Platform/CPU.h"

#if WAVM_CPU_ARCH_ARM
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <cstddef>
#include "WAVM/Inline/BasicTypes.h"
#elif defined(__linux__)
#include <asm/hwcap.h>
#include <elf.h>
#include <sys/auxv.h>
#endif
#endif

WAVM::Platform::HostCPUFeatures WAVM::Platform::getHostCPUFeatures()
{
	WAVM::Platform::HostCPUFeatures result;
#if WAVM_CPU_ARCH_X86
	__builtin_cpu_init();
	result.sse = __builtin_cpu_supports("sse");
	result.sse2 = __builtin_cpu_supports("sse2");
	result.sse3 = __builtin_cpu_supports("sse3");
	result.ssse3 = __builtin_cpu_supports("ssse3");
	result.sse4_1 = __builtin_cpu_supports("sse4.1");
	result.sse4_2 = __builtin_cpu_supports("sse4.2");
	result.avx = __builtin_cpu_supports("avx");
	result.avx2 = __builtin_cpu_supports("avx2");
	result.popcnt = __builtin_cpu_supports("popcnt");
	result.lzcnt = __builtin_cpu_supports("lzcnt");
	result.bmi1 = __builtin_cpu_supports("bmi");
	result.bmi2 = __builtin_cpu_supports("bmi2");
	result.fma = __builtin_cpu_supports("fma");
	result.avx512f = __builtin_cpu_supports("avx512f");
	result.avx512bw = __builtin_cpu_supports("avx512bw");
	result.avx512dq = __builtin_cpu_supports("avx512dq");
	result.avx512vl = __builtin_cpu_supports("avx512vl");
#elif WAVM_CPU_ARCH_ARM
#if defined(__APPLE__)
	I32 val = 0;
	size_t valSize = sizeof(val);
	result.lse = sysctlbyname("hw.optional.arm.FEAT_LSE", &val, &valSize, nullptr, 0) == 0 && val;
#elif defined(__linux__)
	unsigned long hwcap = getauxval(AT_HWCAP);
	result.lse = (hwcap & HWCAP_ATOMICS) != 0;
#else
	result.lse = false;
#endif
#endif
	return result;
}

#endif // WAVM_PLATFORM_POSIX
