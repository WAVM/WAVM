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
	struct ModuleInstance;
	struct Context;
	struct Compartment;
} // namespace Runtime

namespace ThreadTest
{
	using namespace Runtime;

	THREADTEST_API ModuleInstance* instantiate(Compartment* compartment);
} // namespace ThreadTest