#include "IR/Module.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "WASM/WASM.h"
#include "WAST/TestScript.h"
#include "WAST/WAST.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

using namespace WAST;
using namespace IR;

inline bool loadTextModule(const std::string& wastString, IR::Module& outModule)
{
	try
	{
		std::vector<WAST::Error> parseErrors;
		WAST::parseModule(wastString.c_str(), wastString.size(), outModule, parseErrors);
		return !parseErrors.size();
	}
	catch(...)
	{
		Log::printf(Log::error, "unknown exception!\n");
		return false;
	}
}

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	Module module;
	loadTextModule(std::string((const char*)data, numBytes), module);
	return 0;
}

#if !ENABLE_LIBFUZZER
I32 main()
{
	LLVMFuzzerTestOneInput((const U8*)"", 0);
	return EXIT_SUCCESS;
}
#endif
