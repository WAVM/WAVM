#pragma once

#ifndef THREADTEST_API
#define THREADTEST_API DLL_IMPORT
#endif

namespace WAVM { namespace Runtime {
	struct Compartment;
	struct ModuleInstance;
}}

namespace WAVM { namespace ThreadTest {
	THREADTEST_API Runtime::ModuleInstance* instantiate(Runtime::Compartment* compartment);
}}
