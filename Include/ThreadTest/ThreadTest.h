#pragma once

#ifndef THREADTEST_API
#define THREADTEST_API DLL_IMPORT
#endif

#include <vector>

namespace IR
{
	struct Module;
}
namespace Runtime
{
	struct Compartment;
	struct Context;
	struct ModuleInstance;
}

namespace ThreadTest
{
	THREADTEST_API Runtime::ModuleInstance* instantiate(Runtime::Compartment* compartment);
}