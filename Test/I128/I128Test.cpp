#include "WAVM/Inline/I128.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"

using namespace WAVM;

struct RandomStream
{
	RandomStream(I128 inSeed) : seed(inSeed) {}

	I128 get()
	{
		seed = ignoreOverflow(seed * U64(6364136223846793005) + U64(1442695040888963407));
		return seed;
	}

private:
	I128 seed;
};

static bool isEqualOrOverflowed(I128 a, I128 b)
{
	return a.getOverflow() || b.getOverflow() || a == b;
}

I32 main()
{
	Timing::Timer timer;
	RandomStream random(0);

	for(Uptr i = 0; i < 10000000; ++i)
	{
		I128 a = random.get();
		I128 b = random.get();
		I128 c = random.get();

		errorUnless(isEqualOrOverflowed(a - a, 0));
		errorUnless(isEqualOrOverflowed(a + (-a), 0));
		errorUnless(isEqualOrOverflowed(a + 1, a - (-1)));
		errorUnless(isEqualOrOverflowed(a - 1, a + (-1)));
		errorUnless(isEqualOrOverflowed(a - 0, a));
		errorUnless(isEqualOrOverflowed(a + 0, a));                 // Identity
		errorUnless(isEqualOrOverflowed(a + b, b + a));             // Commutativity
		errorUnless(isEqualOrOverflowed((a + b) + c, a + (b + c))); // Associativity

		errorUnless(isEqualOrOverflowed(a * 0, 0));
		errorUnless(isEqualOrOverflowed(a * -1, -a));
		errorUnless(isEqualOrOverflowed(a * 1, a));                       // Identity
		errorUnless(isEqualOrOverflowed(a * b, b * a));                   // Commutativity
		errorUnless(isEqualOrOverflowed((a * b) * c, a * (b * c)));       // Associativity
		errorUnless(isEqualOrOverflowed(a * (b + c), (a * b) + (a * c))); // Distributivity

		errorUnless(isEqualOrOverflowed((a * b) / b, a));
		errorUnless(isEqualOrOverflowed((a + b) - b, a));
		errorUnless(isEqualOrOverflowed((a - b) + b, a));

		errorUnless(isEqualOrOverflowed(((a / b) * b) + (a % b), a));
	}

	Timing::logTimer("Ran I128 tests", timer);

	return 0;
}
