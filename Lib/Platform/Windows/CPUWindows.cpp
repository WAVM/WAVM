#if WAVM_PLATFORM_WINDOWS

#include "WAVM/Platform/CPU.h"
#include "WindowsPrivate.h"

#if WAVM_CPU_ARCH_X86
#include <intrin.h>
#elif WAVM_CPU_ARCH_ARM
#include <processthreadsapi.h>
#endif

WAVM::Platform::HostCPUFeatures WAVM::Platform::getHostCPUFeatures()
{
	WAVM::Platform::HostCPUFeatures result;
#if WAVM_CPU_ARCH_X86
	// __cpuid returns [EAX, EBX, ECX, EDX]
	int leaf1[4];
	__cpuid(leaf1, 1);
	int ecx1 = leaf1[2];
	int edx1 = leaf1[3];

	int leaf7[4];
	__cpuidex(leaf7, 7, 0);
	int ebx7 = leaf7[1];

	int extLeaf[4];
	__cpuid(extLeaf, 0x80000001);
	int ecxExt = extLeaf[2];

	// Leaf 1 EDX
	result.sse = !!(edx1 & (1 << 25));
	result.sse2 = !!(edx1 & (1 << 26));
	// Leaf 1 ECX
	result.sse3 = !!(ecx1 & (1 << 0));
	result.ssse3 = !!(ecx1 & (1 << 9));
	result.fma = !!(ecx1 & (1 << 12));
	result.sse4_1 = !!(ecx1 & (1 << 19));
	result.sse4_2 = !!(ecx1 & (1 << 20));
	result.popcnt = !!(ecx1 & (1 << 23));
	result.avx = !!(ecx1 & (1 << 28));
	// Leaf 7 subleaf 0 EBX
	result.bmi1 = !!(ebx7 & (1 << 3));
	result.avx2 = !!(ebx7 & (1 << 5));
	result.bmi2 = !!(ebx7 & (1 << 8));
	result.avx512f = !!(ebx7 & (1 << 16));
	result.avx512dq = !!(ebx7 & (1 << 17));
	result.avx512bw = !!(ebx7 & (1 << 30));
	result.avx512vl = !!(ebx7 & (1 << 31));
	// Extended leaf 0x80000001 ECX
	result.lzcnt = !!(ecxExt & (1 << 5));
#elif WAVM_CPU_ARCH_ARM
	result.lse = IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE);
#endif
	return result;
}

#endif // WAVM_PLATFORM_WINDOWS
