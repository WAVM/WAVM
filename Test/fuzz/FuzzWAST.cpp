#include "IR/Module.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "WASM/WASM.h"
#include "WASTParse/TestScript.h"
#include "WASTParse/WASTParse.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

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
