#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef min
#undef max
#include <bcrypt.h>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Random.h"

using namespace WAVM;
using namespace WAVM::Platform;

#if defined(_MSC_VER)
#pragma comment(lib, "bcrypt.lib")
#endif

void Platform::getCryptographicRNG(U8* outRandomBytes, Uptr numBytes)
{
	WAVM_ERROR_UNLESS(numBytes <= ULONG_MAX);

	WAVM_ERROR_UNLESS(!BCryptGenRandom(
		nullptr, outRandomBytes, ULONG(numBytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}
