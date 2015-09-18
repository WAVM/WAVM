#include <cstdint>

int32_t a() { return 1; }
int32_t b() { return -1; }

typedef int32_t (*FunctionPointer)();

int32_t main()
{
	enum { bufferSize = 10000 };
	enum { passes = 1000000 };
	FunctionPointer* buffer = new FunctionPointer[bufferSize];
	int32_t accumulator = 0;

	for(uint32_t pass = 0;pass < passes;++pass)
	{
		for(uint32_t i = 0;i < bufferSize;++i)
		{
			buffer[i] = (i&1) ? &a : &b;
		}

		for(uint32_t i = 0;i < bufferSize;++i)
		{
			accumulator += buffer[i]();
		}
	}

	return accumulator;
}