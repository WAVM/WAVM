#include <stdlib.h>
#include <vector>

#include "IR/Module.h"
#include "Inline/BasicTypes.h"
#include "WASTParse/WASTParse.h"

using namespace IR;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	// Copy the data to a std::vector and append a null terminator.
	std::vector<U8> wastBytes(data, data + numBytes);
	wastBytes.push_back(0);

	Module module;
	std::vector<WAST::Error> parseErrors;
	WAST::parseModule((const char*)wastBytes.data(), wastBytes.size(), module, parseErrors);
	return 0;
}

#if !ENABLE_LIBFUZZER
I32 main()
{
	LLVMFuzzerTestOneInput((const U8*)"", 0);
	return EXIT_SUCCESS;
}
#endif
