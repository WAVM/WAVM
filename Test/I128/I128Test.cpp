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
		seed = addmod127(mulmod127(seed, U64(6364136223846793005)), U64(1442695040888963407));
		return seed;
	}

private:
	I128 seed;
};

static bool isEqualOrNaN(I128 a, I128 b) { return isNaN(a) || isNaN(b) || a == b; }

I32 main()
{
	Timing::Timer timer;
	RandomStream random(0);

	I128 phaseMasks[3] = {I128::max(), UINT64_MAX, 32767};
	for(Uptr phase = 0; phase < 3; ++phase)
	{
		for(Uptr i = 0; i < 1000000; ++i)
		{
			I128 a = random.get() & phaseMasks[phase];
			I128 b = random.get() & phaseMasks[phase];
			I128 c = random.get() & phaseMasks[phase];

			errorUnless(isEqualOrNaN(a - a, 0));
			errorUnless(isEqualOrNaN(a + (-a), 0));
			errorUnless(isEqualOrNaN(a + 1, a - (-1)));
			errorUnless(isEqualOrNaN(a - 1, a + (-1)));
			errorUnless(isEqualOrNaN(a - 0, a));
			errorUnless(isEqualOrNaN(a + 0, a));                 // Identity
			errorUnless(isEqualOrNaN(a + b, b + a));             // Commutativity
			errorUnless(isEqualOrNaN((a + b) + c, a + (b + c))); // Associativity

			errorUnless(isEqualOrNaN(a * 0, 0));
			errorUnless(isEqualOrNaN(a * -1, -a));
			errorUnless(isEqualOrNaN(a * 1, a));                       // Identity
			errorUnless(isEqualOrNaN(a * b, b * a));                   // Commutativity
			errorUnless(isEqualOrNaN((a * b) * c, a * (b * c)));       // Associativity
			errorUnless(isEqualOrNaN(a * (b + c), (a * b) + (a * c))); // Distributivity

			if(b != 0) { errorUnless(isEqualOrNaN((a * b) / b, a)); }
			errorUnless(isEqualOrNaN((a + b) - b, a));
			errorUnless(isEqualOrNaN((a - b) + b, a));

			if(b != 0) { errorUnless(isEqualOrNaN(((a / b) * b) + (a % b), a)); }
		}
	}

	Timing::logTimer("Ran I128 tests", timer);

	return 0;
}
