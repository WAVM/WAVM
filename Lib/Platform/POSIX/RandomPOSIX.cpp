#if WAVM_PLATFORM_POSIX

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Random.h"

using namespace WAVM;
using namespace WAVM::Platform;

static void readDevRandom(U8* outRandomBytes, Uptr numBytes)
{
	static I32 randomFD = open("/dev/urandom", O_RDONLY);
	if(randomFD < 0) { Errors::fatalf("Failed to open /dev/urandom: %s", strerror(errno)); }

	while(numBytes > 0)
	{
		I32 result = read(randomFD, outRandomBytes, numBytes);
		if(result >= 0)
		{
			outRandomBytes += result;
			numBytes -= result;
		}
		else if(errno != EINTR)
		{
			Errors::fatalf("Failed to read from /dev/urandom: %s", strerror(errno));
		}
	}
}

void Platform::getCryptographicRNG(U8* outRandomBytes, Uptr numBytes)
{
	readDevRandom(outRandomBytes, numBytes);
}

#endif // WAVM_PLATFORM_POSIX
